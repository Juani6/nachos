# Nivel 3 — Referencia de Funciones: Exception Handlers

**Módulo:** [`userprog/exception.cc`](../02-modulos/exception.md)
**Subsistema:** [Gestión de Procesos y Syscalls](../01-subsistemas/procesos.md)

---

### `void SetExceptionHandlers()`

**Ubicación:** `userprog/exception.cc:630`

**Descripción:**
Registra en `machine` el handler de C++ correspondiente a cada tipo de excepción
MIPS. Debe llamarse una vez durante la inicialización del sistema, antes de que
cualquier proceso de usuario corra.

**Retorno:** `void`

**Efectos secundarios:**
- Llama `machine->SetHandler(tipo, &handler)` para cada `ExceptionType`.
- Handlers registrados:
  - `NO_EXCEPTION` → `DefaultHandler`
  - `SYSCALL_EXCEPTION` → `SyscallHandler`
  - `PAGE_FAULT_EXCEPTION` → `PageFaultHandler`
  - `READ_ONLY_EXCEPTION` → `ReadOnlyHandler`
  - `BUS_ERROR_EXCEPTION`, `ADDRESS_ERROR_EXCEPTION`, `OVERFLOW_EXCEPTION`,
    `ILLEGAL_INSTR_EXCEPTION` → `DefaultHandler`

**Precondiciones / contexto de llamada:**
- `machine` debe estar inicializado.
- Se llama desde `threads/system.cc::Initialize()`.

**Ver también:** `SyscallHandler`, `PageFaultHandler`

---

### `static void SyscallHandler(ExceptionType _et)`

**Ubicación:** `userprog/exception.cc:476`

**Descripción:**
Dispatcher central de syscalls. Lee el número de syscall de `r2` y delega a la
función específica. Llama `IncrementPC()` al final para avanzar el PC del simulador
MIPS y evitar re-ejecutar la instrucción `syscall`.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `_et` | `ExceptionType` | Siempre `SYSCALL_EXCEPTION` (no se usa internamente) |

**Retorno:** `void`

**Efectos secundarios:**
- Lee `r2` con `machine->ReadRegister(2)` para obtener el `scid`.
- Despacha a `syscall_SC_*()` según el `scid`.
- Llama `IncrementPC()` al terminar (salvo que la syscall termine el proceso).

**Precondiciones / contexto de llamada:**
- Llamado desde `Machine::Run()` cuando el MIPS emite `syscall`.
- Los argumentos están en `r4`–`r7`; el valor de retorno va en `r2`.
- `IncrementPC()` **no** debe llamarse en syscalls que no retornan (`SC_EXIT`
  cuando es el último proceso, `SC_HALT`).

**Ver también:** `IncrementPC`, `PageFaultHandler`

---

### `static void PageFaultHandler(ExceptionType _et)`

**Ubicación:** `userprog/exception.cc:576`

**Descripción:**
Maneja los TLB misses del simulador MIPS. Calcula el VPN de la dirección que
causó el fault, carga la página en RAM si no está (`valid==false`), y recarga
la TLB con política de reemplazo FIFO circular.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `_et` | `ExceptionType` | Siempre `PAGE_FAULT_EXCEPTION` |

**Retorno:** `void`

**Efectos secundarios:**
- Lee `BAD_VADDR_REG` para obtener la dirección que causó el fault.
- Calcula `vpn = badAddrs / PAGE_SIZE`.
- Si `pageTable[vpn].valid == false`:
  - Con `SWAP`: llama `LoadFromSwap(owner, vpn)` que carga desde swapFile o ejecutable.
  - Sin `SWAP`: llama `owner->space->LoadPage(vpn)`.
- Guarda `dirty`/`use` de la entrada TLB que se desaloja de vuelta a `pageTable`.
- Invalida la entrada TLB saliente.
- Carga `pageTable[vpn]` en `tlb[tlb_index]`.
- Avanza `tlb_index = (tlb_index + 1) % TLB_SIZE`.
- Incrementa `stats->numPageFaults`.

**Precondiciones / contexto de llamada:**
- `currentThread->space != nullptr`.
- Solo activo con `#ifdef USE_TLB`.
- `tlb_index` es una variable estática local — persiste entre llamadas.

**Ver también:** `AddressSpace::LoadPage`, `LoadFromSwap`, `CoreMap::FindPage`

---

### `static void IncrementPC()`

**Ubicación:** `userprog/exception.cc:36`

**Descripción:**
Avanza los tres registros de PC del simulador MIPS en 4 bytes para que la próxima
instrucción ejecutada no sea la `syscall` que acabó de procesarse.

**Retorno:** `void`

**Efectos secundarios:**
- `PREV_PC = PC`
- `PC = NEXT_PC`
- `NEXT_PC = NEXT_PC + 4`

**Precondiciones / contexto de llamada:**
- **Debe** llamarse al final de cada syscall handler que retorna normalmente.
- No llamar en `SC_HALT`, `SC_EXIT` (cuando termina el proceso), ni ante errores
  que matan el proceso.

---

### `void ExecProcess(void *arg)`

**Ubicación:** `userprog/exception.cc:83`

**Descripción:**
Función que ejecuta el nuevo thread creado por `SC_EXEC`. Inicializa los registros
MIPS del proceso, restaura el estado del AddressSpace, carga los argumentos en el
stack del usuario si los hay, y entra al loop del simulador MIPS.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `arg` | `void*` | `char**` con el argv del proceso (puede ser `nullptr`) |

**Retorno:** `void` (no retorna — `machine->Run()` corre indefinidamente hasta syscall `Exit`)

**Efectos secundarios:**
- `currentThread->space->InitRegisters()` — PC=0, STACK_REG=...
- `currentThread->space->RestoreState()` — invalida TLB.
- Si `arg != nullptr`: `WriteArgs(argv)` + ajuste de `STACK_REG` y `r4`/`r5`.
- `machine->Run()` — entra al simulador MIPS.

**Precondiciones / contexto de llamada:**
- Llamada via `newThread->Fork(ExecProcess, argv)`.
- `currentThread->space` debe estar inicializado antes de que este thread corra.

**Ver también:** `AddressSpace::InitRegisters`, `AddressSpace::RestoreState`, `args.cc::WriteArgs`
