# Nivel 2 — Módulo: thread.cc / thread.hh

## Archivo
- **Ruta:** `threads/thread.cc`, `threads/thread.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Define la clase `Thread`, que es el TCB (Thread Control Block) de NachOS. Gestiona el
ciclo de vida de un thread (creación, fork, yield, sleep, finish), el stack de ejecución
del host, el join vía `Channel`, prioridades con soporte de priority inheritance, y —
cuando `USER_PROGRAM` está activo — el estado de registros MIPS del usuario, el
`AddressSpace` y la tabla de file descriptors.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Thread` | `Thread(const char*, bool join=false, int priority=4)` | Constructor: inicializa TCB, aloca `fdTable`, crea `Channel` si joinable |
| `~Thread` | `~Thread()` | Destructor: libera stack, `fdTable`, `AddressSpace`, `Channel` |
| `Fork` | `void Fork(VoidFunctionPtr func, void *arg)` | Aloca stack, pone el thread en ready queue |
| `Yield` | `void Yield()` | Cede CPU si hay otro thread listo; atómico con interrupciones off |
| `Sleep` | `void Sleep()` | Bloquea thread; pasa a BLOCKED; llama `Scheduler::Run` con el siguiente |
| `Finish` | `void Finish()` | Termina thread: escribe en `pipe` si joinable, marca `threadToBeDestroyed`, llama `Sleep` |
| `Join` | `int Join()` | Bloquea llamador en `pipe->Read()` hasta que el thread termina; retorna exit status |
| `CheckOverflow` | `void CheckOverflow() const` | Verifica fencepost `0xDEADBEEF` en el fondo del stack |
| `SetStatus` / `GetStatus` | `void/ThreadStatus` | Acceso al estado del thread |
| `GetPriority` / `SetPriority` | `int/void` | Prioridad actual (puede variar por priority inheritance) |
| `GetOriginalPriority` | `int` | Prioridad original (para restaurar tras priority inheritance) |
| `IsJoinable` | `const bool` | True si el thread fue creado con `join=true` |
| `SaveUserState` / `RestoreUserState` | `void` | Guarda/restaura los `NUM_TOTAL_REGS` registros MIPS en `userRegisters[]` |
| `GetExitStatus` / `SetExitStatus` | `int/void` | Exit status del proceso usuario (`#ifdef USER_PROGRAM`) |
| `SWITCH` | `extern "C" void SWITCH(Thread*, Thread*)` | ASM: efectúa el context switch entre dos threads |
| `ThreadRoot` | `extern "C" void ThreadRoot()` | ASM: primer frame del stack de un thread nuevo |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `StackAllocate(func, arg)` | Aloca el stack con `AllocBoundedArray`; setea `machineState` para que `SWITCH` arranque en `ThreadRoot` |
| `ThreadFinish()` (static) | Wrapper C de `currentThread->Finish()`; necesario para pasar puntero a función a `ThreadRoot` |
| `InterruptEnable()` (static) | Primer función que corre `ThreadRoot`: habilita interrupciones y destruye el thread anterior si quedó pendiente |
| `STACK_FENCEPOST = 0xDEADBEEF` | Constante en el fondo del stack para detectar overflow |

## Dependencias

- **Incluye:** `thread.hh`, `switch.h`, `system.hh`, `channels.hh`, `machine/machine.hh` (con `USER_PROGRAM`), `userprog/syscall.h`
- **Es incluido por:** `scheduler.cc`, `semaphore.cc`, `lock.cc`, `condition.cc`, y prácticamente todo el kernel

## Notas específicas de implementación

- **Orden de campos crítico:** `stackTop` debe ser el primer campo y `machineState` el segundo en la clase `Thread`; el código ASM de `SWITCH` asume esos offsets fijos. Están marcados con `// NOTE: DO NOT CHANGE the order`.
- **Stack layout:** `StackAllocate` construye el frame inicial empujando `ThreadRoot` como dirección de retorno para `SWITCH`. `machineState[PCState]` apunta a `ThreadRoot`; `StartupPCState` a `InterruptEnable`; `InitialPCState` a la función del thread; `WhenDonePCState` a `ThreadFinish`.
- **Join via Channel:** cada thread joinable tiene un `Channel* pipe` privado. `Finish()` hace `pipe->Write(exitStatus)` y `Join()` hace `pipe->Read()`, logrando un rendez-vous sincrónico.
- **Priority inheritance:** en `Lock::Acquire`, si el holder tiene menor prioridad que el adquiriente, se eleva temporalmente la prioridad del holder vía `SetPriority`. `Lock::Release` restaura la prioridad original con `GetOriginalPriority()`.
- **fdTable:** los índices 0 y 1 se reservan en el constructor para `CONSOLE_INPUT` y `CONSOLE_OUTPUT` (se insertan `nullptr`), siguiendo la convención de POSIX stdin/stdout.
- No se usan excepciones C++ ni RTTI. El destructor no puede ser llamado sobre el thread en ejecución (`ASSERT(this != currentThread)`).
