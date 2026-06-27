# Nivel 1 — Subsistema: Memoria Virtual

## Propósito
Extender el espacio de direcciones de los procesos MIPS más allá de la RAM física
simulada mediante demand paging, TLB software y swap en disco. Provee los algoritmos
de reemplazo de páginas FIFO y CLOCK, y garantiza que las páginas en uso por una
syscall no sean desalojadas (pinning).

Este subsistema no tiene directorio propio: sus fuentes viven en `userprog/` y se
activan mediante flags de compilación (`-DVMEM -DUSE_TLB -DSWAP -DDEMAND_LOADING
-DFRPOLICY_CLOCK`) definidos en `vmem/Makefile`.

## Archivos involucrados

| Archivo | Lenguaje | Rol |
|---------|----------|-----|
| `userprog/coremap.cc / .hh` | C++ | `CoreMap`: tabla de frames físicos, FindPage, PickVictim, SendToSwap, Pin/Unpin |
| `userprog/address_space.cc / .hh` | C++ | `AddressSpace`: shadow table, archivo de swap por proceso, `LoadPage`, demand paging |
| `userprog/exception.cc` | C++ | `PageFaultHandler`: TLB miss → carga página desde RAM o swap; `LoadFromSwap` |
| `machine/mmu.hh` | C++ | Definición del TLB (`tlb[TLB_SIZE]`, `TranslationEntry`) |
| `machine/translation_entry.hh` | C++ | `TranslationEntry`: virtualPage, physicalPage, valid, dirty, use, readOnly |

## Flujo de control

### TLB miss (page fault)
```
machine->Run() — instrucción MIPS accede a dirección virtual
    -> MMU no encuentra entrada válida en TLB
    -> excepción PAGE_FAULT_EXCEPTION
    -> PageFaultHandler(et)
         badAddrs = ReadRegister(BAD_VADDR_REG)
         vpn = badAddrs / PAGE_SIZE
         if pageTable[vpn].valid == false:
             #ifdef SWAP
             LoadFromSwap(owner, vpn)
                 if shadowTable[vpn].isInSwap:
                     coreMap->FindPage(owner, vpn)   -> pfn
                     swapFile->ReadAt(mainMemory[pfn*PAGE_SIZE], PAGE_SIZE, vpn*PAGE_SIZE)
                     pageTable[vpn].valid = true
                 else:  // demand loading
                     owner->space->LoadPage(vpn)     // carga desde ejecutable
             #else
             owner->space->LoadPage(vpn)
         // Reemplazo FIFO de entrada TLB (round-robin sobre TLB_SIZE)
         Si la entrada TLB que se va a reemplazar es válida:
             guarda dirty/use en pageTable del VPN que sale
             invalida tlb[tlb_index]
         tlb[tlb_index] = pageTable[vpn]
         tlb_index = (tlb_index + 1) % TLB_SIZE
```

### Selección y desalojo de víctima (CoreMap::FindPage)
```
CoreMap::FindPage(owner, vpn)
    -> bajo mMapLock:
    -> busca primer frame libre (arr[i].isFree == true)
    -> si no hay libre:
         do { idx = PickVictim() } while arr[idx].isPinned
         PinPage(idx)
         mMapLock->Release()
         SendToSwap(idx)           // escribe marco a disco
         mMapLock->Acquire()
    -> arr[idx] = { isFree=0, owner, vpn }
    -> retorna idx (número de frame físico)
```

### SendToSwap (desalojo)
```
CoreMap::SendToSwap(pfn)
    -> owner = arr[pfn].owner
    -> vpn   = arr[pfn].vpn
    -> shadowTable[vpn].isInSwap = true
    -> pageTable[vpn].valid = false
    -> CheckTLB(pfn, owner)   // invalida entrada TLB si apunta a este frame
    -> swapFile->WriteAt(mainMemory[pfn*PAGE_SIZE], PAGE_SIZE, vpn*PAGE_SIZE)
    -> stats->numSwapOuts++
```

### Algoritmo CLOCK (PickVictim con FRPOLICY_CLOCK)
```
Dos bits por página en pageTable: use, dirty
Cuatro pasadas en orden de prioridad de desalojo:
    1. (use=0, dirty=0) — desalojo inmediato
    2. (use=0, dirty=1) — requiere escritura a swap
    3. (use=1, dirty=0) — pone use=false en el camino
    4. (use=1, dirty=1) — peor caso
victimIdx avanza circularmente sobre el tamaño de CoreMap.
```

### Context switch y TLB
```
AddressSpace::SaveState()
    -> invalida todas las entradas de la TLB (para el proceso que sale)
AddressSpace::RestoreState()
    -> no recarga la TLB; se recargará on-demand vía page faults
```

## Estructuras de datos clave

| Estructura | Definida en | Descripción |
|------------|-------------|-------------|
| `CoreMapEntry` | `userprog/coremap.hh` | owner (Thread*), vpn, isFree, isPinned — una por frame físico |
| `CoreMap` | `userprog/coremap.hh` | Array de CoreMapEntry, victimIdx para CLOCK/FIFO |
| `ShadowTable` | `userprog/address_space.hh` | Por VPN: isInSwap, readOnly — sigue el estado fuera de RAM |
| `TranslationEntry` | `machine/translation_entry.hh` | VPN→PFN, valid, dirty, use, readOnly |
| `swapFile` | `userprog/address_space.hh` | `OpenFile*` — archivo de swap por proceso (nombre único por pid) |
| `coreMap` | `threads/system.hh` | Global `CoreMap*` (NUM_PHYS_PAGES entradas) |
| `mMapLock` | `threads/system.hh` | `Lock*` que protege acceso a CoreMap |

## Dependencias
- **Depende de:** `machine/mmu` (TLB), `machine/machine` (mainMemory), `filesys`
  (OpenFile para swapFile), `threads/scheduler` (currentThread, Lock)
- **Es usado por:** `userprog/exception` (PageFaultHandler lo invoca), `userprog/address_space`
  (destructor libera frames y borra swapFile)

## Invariantes / precondiciones importantes
- Un frame **pinned** nunca es seleccionado como víctima; el pin se mantiene mientras
  la syscall que provocó el fault está en curso y se libera con `UnPinPage` al terminar.
- Antes de escribir a swap, `CheckTLB` invalida cualquier entrada TLB que apunte al
  frame desalojado — evita que la MMU use una traducción stale.
- La shadow table (`isInSwap`) y `pageTable[vpn].valid` son la fuente de verdad sobre
  si una página está en RAM, en swap o sin cargar todavía (demand loading).
- El archivo de swap de cada proceso tiene tamaño fijo = `numPages * PAGE_SIZE` bytes
  (pre-allocado); las páginas se indexan directamente por VPN.

## Notas de diseño
- La separación entre `CoreMap` y `AddressSpace` permite que el reemplazo de página
  sea global (no por proceso): `PickVictim` puede elegir un frame de cualquier proceso.
- El algoritmo CLOCK se implementa en cuatro pasadas explícitas sobre el array circular,
  siguiendo las 4 combinaciones (use, dirty). Si ninguna pasada encuentra candidato en
  las primeras tres, la cuarta garantiza encontrar uno.
- El flag `-DFRPOLICY_CLOCK` / `-DPRPOLICY_FIFO` en el Makefile selecciona el algoritmo
  en tiempo de compilación mediante `#ifdef` en `CoreMap::PickVictim`.
- Demand loading: `InitPageTableOnDemand()` no carga ninguna página al crear el proceso;
  cada página se carga en su primer acceso vía PageFaultHandler.

## Limitaciones conocidas / TODOs
- No hay protección contra thrashing: si `NUM_PHYS_PAGES` es muy chico respecto al
  working set total, el sistema degrada indefinidamente.
- El algoritmo CLOCK no reinicia los bits `use` en la pasada 2 (solo los resetea en la pasada 3); revisar si es intencional.
- El archivo de swap no se borra si el proceso termina abruptamente (e.g., ASSERT fail).
