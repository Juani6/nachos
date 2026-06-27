# Nivel 2 — Módulo: address_space.cc / address_space.hh

## Archivo
- **Ruta:** `userprog/address_space.cc`, `userprog/address_space.hh`
- **Subsistema:** [Gestión de Procesos](../01-subsistemas/procesos.md) / [Memoria Virtual](../01-subsistemas/vmem.md)

## Propósito del archivo
Gestiona el espacio de direcciones virtuales de un proceso MIPS: parsea el ejecutable
NOFF, construye la tabla de páginas, inicializa (o difiere mediante demand paging) la
carga de segmentos, y administra el archivo de swap por proceso. También guarda el
sector del directorio de trabajo del proceso (`currentDirSector`).

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `AddressSpace` | `AddressSpace(OpenFile*, unsigned pid, Thread* owner)` | Crea tabla de páginas, archivo de swap (si `SWAP`); inicializa páginas según modo (demand / naive) |
| `~AddressSpace` | `~AddressSpace()` | Libera frames físicos (CoreMap o Bitmap), borra swapFile del disco, elimina pageTable, exe y el OpenFile del ejecutable |
| `InitRegisters` | `void InitRegisters()` | Escribe en los registros MIPS: PC=0, NEXT_PC=4, STACK_REG=numPages*PAGE_SIZE-16 |
| `SaveState` | `void SaveState()` | Vacío (el estado MIPS del usuario se guarda en `Thread::userRegisters`) |
| `RestoreState` | `void RestoreState()` | Sin TLB: apunta `mmu->pageTable` a la tabla propia; con TLB: invalida todas las entradas |
| `LoadPage` | `void LoadPage(unsigned vpn)` | Demand paging: busca frame físico, lee el segmento correspondiente (código/data/stack), setea `valid=true` |
| `GetPageTable` | `TranslationEntry* GetPageTable()` | Retorna puntero a la tabla de páginas |
| `GetNumberPages` | `unsigned GetNumberPages()` | Retorna `numPages` |
| `CreateSwapName` | `char* CreateSwapName()` | Retorna string `"SWAP.<pid>"` (heap-allocated, caller libera) |
| `GetSwapFile` | `OpenFile* GetSwapFile()` | Retorna el OpenFile del swap de este proceso |
| `IsInSwap` / `InSwap` / `NotInSwap` | `bool/void (unsigned vpn)` | Consulta y modifica `shadowTable[vpn].isInSwap` |
| `GetReadFlag` | `bool GetReadFlag(unsigned vpn)` | Retorna `shadowTable[vpn].readOnly` |
| `currentDirSector` | `int` (campo público) | Sector del directorio de trabajo actual del proceso; heredado del padre en `SC_EXEC` |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `InitSwapFile()` | Crea el archivo `SWAP.<pid>` en el filesystem con tamaño `numPages*PAGE_SIZE` y lo abre |
| `InitPageTableOnDemand()` | Inicializa todos los entries como `valid=false, physicalPage=-1` — demand loading puro |
| `InitPageTableNaive()` | Asigna un frame físico a cada VPN desde CoreMap/Bitmap; no carga el contenido aún |
| `InitLoadSegments()` | Copia código y data del ejecutable a memoria física; marca páginas como `valid=true` |
| `ExeRead(virtualAddr, size, type)` | Lee un segmento (CODE o DATA) del NOFF byte a byte sobre páginas físicas; gestiona páginas híbridas (que contienen fin de código y comienzo de data) |

## Dependencias

- **Incluye:** `address_space.hh`, `threads/system.hh`, `filesys/file_system.hh`
- **Es incluido por:** `exception.cc` (SC_EXEC, PageFaultHandler), `thread.hh` (campo `space`)

## Notas específicas de implementación

- **Modos de inicialización (seleccionados por flags de compilación):**
  | Flags | `InitPageTable*` | `InitLoadSegments` |
  |-------|------------------|--------------------|
  | sin flags | `Naive` (asigna frames) | sí (carga inmediata) |
  | `DEMAND_LOADING` | `OnDemand` (valid=false) | no — carga en page fault |
  | `SWAP` + `DEMAND_LOADING` | `OnDemand` + crea swapFile | no |

- **`LoadPage` y páginas híbridas:** una página puede contener el final del segmento
  de código y el inicio del segmento de data. `LoadPage` lo detecta comparando
  `segOffset` con `codeSize` y calcula los offsets correctos para cada parte.
  Las páginas de código se marcan `readOnly`; las mixtas o de data no.

- **Swap file layout:** el archivo `SWAP.<pid>` tiene exactamente `numPages * PAGE_SIZE`
  bytes. La página VPN `i` se guarda/lee en `offset = i * PAGE_SIZE`.

- **`RestoreState` con TLB:** en lugar de cargar la tabla de páginas en la MMU
  (como en el modo sin TLB), simplemente invalida la TLB completa. Las traducciones
  se recargan on-demand vía `PageFaultHandler`.

- **`ExeRead` con SWAP:** si en modo `SWAP` la página aún no tiene frame asignado,
  llama `coreMap->FindPage()` (que puede desalojar otra página a swap). Si ya tiene
  frame, hace `PinPage` para que no sea desalojada mientras copia.

- **`ShadowTable`:** se usa en paralelo con `pageTable`. `pageTable[vpn].valid` indica
  si la página está en RAM; `shadowTable[vpn].isInSwap` indica si está en el archivo de
  swap. Cuando una página es desalojada de RAM a swap, ambos campos se actualizan.
