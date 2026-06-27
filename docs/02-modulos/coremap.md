# Nivel 2 — Módulo: coremap.cc / coremap.hh

## Archivo
- **Ruta:** `userprog/coremap.cc`, `userprog/coremap.hh`
- **Subsistema:** [Memoria Virtual](../01-subsistemas/vmem.md)

## Propósito del archivo
Implementa la tabla global de frames físicos (CoreMap). Sabe quién ocupa cada frame
(`Thread* owner`, `vpn`), y ejecuta los algoritmos de reemplazo de página **FIFO**
o **CLOCK** para elegir víctimas cuando no hay frames libres. También gestiona el
desalojo a swap (`SendToSwap`) y el pinning de páginas activas.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `CoreMap` | `CoreMap(unsigned numPhysPages)` | Aloca array de `CoreMapEntry`; `victimIdx = size-1` |
| `~CoreMap` | `~CoreMap()` | Libera el array |
| `FindPage` | `unsigned FindPage(Thread* owner, uint32_t vpn)` | Busca frame libre; si no hay, elige víctima (PickVictim), la desaloja a swap, retorna el índice del frame asignado |
| `PickVictim` | `unsigned PickVictim()` | Algoritmo de reemplazo: FIFO circular o CLOCK con 4 pasadas; seleccionado por `#ifdef PRPOLICY_FIFO` / `FRPOLICY_CLOCK` |
| `FreePage` | `void FreePage(uint32_t physAddrs)` | Limpia el frame (memset a 0) y lo marca libre |
| `PinPage` | `void PinPage(uint32_t physAddrs)` | Marca frame como no desalojable |
| `UnPinPage` | `void UnPinPage(uint32_t physAddrs)` | Desmarca el frame |
| `SendToSwap` | `void SendToSwap(unsigned pfn)` | Invalida entrada TLB (CheckTLB), marca página como en swap, copia frame a swapFile del proceso |
| `UpdateCoreMap` | `void UpdateCoreMap(uint32_t vpn, unsigned idx)` | Actualiza owner y vpn de un frame (usado tras demand loading) |
| `GetPage` | `CoreMapEntry* GetPage(unsigned idx)` | Retorna puntero al entry del frame |
| `GetFreePages` | `unsigned GetFreePages()` | Cuenta frames libres (para debug/stats) |
| `PhysAdrrToIdx` | `uint32_t PhysAdrrToIdx(uint32_t physAddr)` | `physAddr / PAGE_SIZE` |
| `IdxToPhysAddr` | `uint32_t IdxToPhysAddr(uint32_t idx)` | `idx * PAGE_SIZE` |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `CheckTLB(pfn, owner)` (static) | Invalida cualquier entrada de la TLB que apunte al frame `pfn`; guarda `dirty`/`use` en la `pageTable` del proceso antes de invalidar |

## Dependencias

- **Incluye:** `coremap.hh`, `lib/assert.hh`, `threads/thread.hh`, `system.hh`
- **Es incluido por:** `address_space.cc` (LoadPage, ExeRead, destructor), `exception.cc` (LoadFromSwap, PageFaultHandler)

## Notas específicas de implementación

- **Exclusión mutua en `FindPage`:** usa `mMapLock` (global en `system.hh`). La secuencia
  es: adquirir lock → buscar libre → si no hay: elegir víctima → pin bajo lock → release lock →
  `SendToSwap` (sin lock, para no tener lock mientras hace I/O) → adquirir lock nuevamente →
  marcar entrada como ocupada → release lock. El pin garantiza que la víctima no sea robada
  por otro thread entre el release y el SendToSwap.

- **FIFO:** `victimIdx = (victimIdx + 1) % size` en cada llamada; simple circular.

- **CLOCK (4 pasadas):**
  1. Busca `(use=0, dirty=0)` → desalojo inmediato sin escritura a disco.
  2. Busca `(use=0, dirty=1)` → requiere escritura a swap.
  3. Busca `(use=1, dirty=0)` → resetea `use=false` en el camino.
  4. Busca `(use=1, dirty=1)` → peor caso; siempre encuentra alguno.
  
  `victimIdx` no se resetea entre pasadas; avanza continuamente sobre el array.

- **`SendToSwap` y TLB:** `CheckTLB` escanea las `TLB_SIZE` entradas; si alguna
  mapea el frame desalojado, copia `dirty`/`use` a la `pageTable` y la invalida.
  Esto es crucial: sin este paso, la MMU podría usar una traducción stale que apunta
  a un frame ya reasignado a otra página.

- **`memset` en `FreePage`:** limpia el contenido del frame a cero al liberarlo,
  evitando que datos de un proceso sean visibles a otro (seguridad básica).

- Selección de algoritmo en tiempo de compilación: `vmem/Makefile` define
  `-DFRPOLICY_CLOCK`; si no está definido ni FIFO ni CLOCK, `PickVictim` retorna
  `rand() % size` (fallback).
