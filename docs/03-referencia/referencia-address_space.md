# Nivel 3 — Referencia de Funciones: AddressSpace

**Módulo:** [`userprog/address_space.cc`](../02-modulos/address_space.md)
**Subsistema:** [Memoria Virtual](../01-subsistemas/vmem.md)

---

### `AddressSpace::AddressSpace(OpenFile *executable)`

**Ubicación:** `userprog/address_space.cc`

**Descripción:**
Constructor principal. Lee el header NOFF del ejecutable, calcula el número de
páginas necesarias para código, datos y stack, e inicializa la page table.

Con `DEMAND_LOADING`: usa `InitPageTableOnDemand` — las entradas quedan inválidas y
los segmentos se cargan en el primer acceso (page fault).

Sin `DEMAND_LOADING`: usa `InitPageTableNaive` / `InitLoadSegments` — carga todo
en RAM desde el inicio.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `executable` | `OpenFile*` | Archivo NOFF del proceso; se guarda en `exe` para demand paging |

**Efectos secundarios:**
- Lee `noffHeader` y lo byte-swaps si el host es big-endian (`SwapHeader`).
- Calcula `numPages = DIVUP(codeSize+dataSize+stackSize, PAGE_SIZE)`.
- Aloca `pageTable = new TranslationEntry[numPages]`.
- Con `SWAP`: inicializa el swap file (`InitSwapFile`).
- `currentDirSector` heredado del parent via `kernelDirSector` o `SC_EXEC`.

**Precondiciones / contexto de llamada:**
- Llamado desde `syscall_SC_EXEC` antes de hacer `Fork` del nuevo thread.
- El archivo NOFF debe ser válido (magic number verificado).

**Ver también:** `LoadPage`, `InitSwapFile`, `RestoreState`

---

### `void AddressSpace::LoadPage(int vpn)`

**Ubicación:** `userprog/address_space.cc`

**Descripción:**
Carga la página virtual `vpn` en un frame físico libre. Determina si la página
corresponde a código, datos inicializados, datos BSS o stack y la llena
correspondientemente. Actualiza la `pageTable[vpn]` con el frame asignado.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `vpn` | `int` | Número de página virtual a cargar |

**Efectos secundarios:**
- Llama `coreMap->FindPage(currentThread, vpn)` para obtener un frame físico
  (puede desalojar una página víctima a swap).
- Calcula la dirección física `ppn * PAGE_SIZE`.
- Si la página está en rango de código o datos inicializados: lee del `exe` con `ExeRead`.
- Si está en BSS o stack: rellena con ceros (`bzero` / `memset`).
- Actualiza `pageTable[vpn]: valid=true, physicalPage=ppn, readOnly=(código)`.
- Con `SWAP`: si `shadowTable[vpn].isInSwap`, lee desde `swapFile` en lugar del exe.

**Precondiciones / contexto de llamada:**
- `vpn` debe ser < `numPages`.
- Llamado desde `PageFaultHandler` cuando `pageTable[vpn].valid == false`.
- Interrupciones pueden estar habilitadas; `FindPage` maneja su propia sincronización.

**Ver también:** `CoreMap::FindPage`, `PageFaultHandler`, `ExeRead`

---

### `void AddressSpace::RestoreState()`

**Ubicación:** `userprog/address_space.cc`

**Descripción:**
Restaura el estado de la MMU del simulador MIPS para este proceso. Con `USE_TLB`:
invalida todas las entradas de la TLB (no escribe la page table completa —
las páginas se cargarán a demanda via page faults). Sin `USE_TLB`: copia la page
table completa a `machine->pageTable`.

**Efectos secundarios:**
- Con `USE_TLB`: itera sobre `machine->tlb[0..TLB_SIZE-1]` y setea `valid=false`.
- Sin `USE_TLB`: `machine->pageTable = pageTable; machine->pageTableSize = numPages`.

**Precondiciones / contexto de llamada:**
- Llamada en cada context switch hacia un thread de usuario (`Scheduler::Run` →
  `RestoreUserState` → `space->RestoreState`).
- También llamada explícitamente en `ExecProcess` al inicio del proceso.

**Notas:**
- Con TLB, invalidar en vez de recargar es correcto: los accesos posteriores
  causarán page faults que repoblarán la TLB con las entradas del proceso actual.
  Esto evita manejar ASIDs.

**Ver también:** `PageFaultHandler`, `AddressSpace::SaveState`, `Scheduler::Run`

---

### `void AddressSpace::InitSwapFile()`

**Ubicación:** `userprog/address_space.cc`

**Descripción:**
Crea el archivo de swap del proceso. El nombre es `"SWAP.<pid>"` donde `<pid>`
se obtiene del thread actual. El tamaño inicial es `numPages * PAGE_SIZE` bytes
(reserva espacio para todas las páginas virtuales del proceso).

**Efectos secundarios:**
- Construye el nombre con `CreateSwapName(pid)`.
- Llama `fileSystem->Create(swapName, numPages * PAGE_SIZE)`.
- Abre el archivo resultante y lo guarda en `swapFile`.

**Precondiciones / contexto de llamada:**
- Solo compilado con `#ifdef SWAP`.
- El proceso debe tener un PID asignado antes de llamar.
- Llamado desde el constructor de `AddressSpace`.

**Ver también:** `CoreMap::SendToSwap`, `LoadPage`

---

### `void AddressSpace::InitRegisters()`

**Ubicación:** `userprog/address_space.cc`

**Descripción:**
Inicializa todos los registros MIPS a 0, salvo `PC_REG=0`, `NEXT_PC_REG=4`,
y `STACK_REG = numPages * PAGE_SIZE - 16`. El stack crece hacia abajo desde la
última página virtual; el `-16` reserva espacio para el ABI de llamada MIPS.

**Efectos secundarios:**
- Escribe en `machine->registers[0..NUM_TOTAL_REGS-1]`.

**Precondiciones / contexto de llamada:**
- Llamada en `ExecProcess` antes de `machine->Run()`.

**Ver también:** `ExecProcess`, `AddressSpace::RestoreState`
