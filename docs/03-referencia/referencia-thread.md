# Nivel 3 — Referencia de Funciones: Thread

**Módulo:** [`threads/thread.cc`](../02-modulos/thread.md)
**Subsistema:** [Scheduler / Threads](../01-subsistemas/scheduler.md)

---

### `void Thread::Fork(VoidFunctionPtr func, void *arg)`

**Ubicación:** `threads/thread.cc:136`

**Descripción:**
Pone el thread en condición de ejecutar `(*func)(arg)` concurrentemente con el
llamador. Primero aloca el stack y prepara el frame inicial; luego inserta el
thread en la ready queue del scheduler.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `func` | `VoidFunctionPtr` | Función a ejecutar en el nuevo thread |
| `arg` | `void*` | Argumento único pasado a `func` |

**Retorno:** `void`

**Efectos secundarios:**
- Aloca un stack del host (`STACK_SIZE * sizeof(uintptr_t)` bytes) via `AllocBoundedArray`.
- Llama `scheduler->ReadyToRun(this)` — el thread queda en la ready queue.
- Modifica `machineState[]` del thread con los punteros de arranque.

**Precondiciones / contexto de llamada:**
- Debe haberse creado el objeto `Thread` con `new Thread(...)` antes.
- No llamar `Fork` dos veces sobre el mismo thread.
- `func != nullptr`.

**Ejemplo de uso:**
```cpp
Thread *t = new Thread("worker", true);
t->Fork(MyFunction, (void *) myArg);
```

**Ver también:** `StackAllocate`, `Scheduler::ReadyToRun`, `Thread::Finish`

---

### `void Thread::Yield()`

**Ubicación:** `threads/thread.cc:244`

**Descripción:**
Cede voluntariamente la CPU si hay otro thread listo. Si la ready queue está
vacía, retorna inmediatamente sin cambiar de contexto.

**Parámetros:** (ninguno)

**Retorno:** `void`

**Efectos secundarios:**
- Deshabilita interrupciones durante la operación.
- Si hay un siguiente thread: se pone a sí mismo en la ready queue y llama
  `Scheduler::Run(nextThread)` — el hilo actual queda suspendido hasta ser
  re-schedulado.

**Precondiciones / contexto de llamada:**
- `this == currentThread`.
- Puede llamarse con interrupciones habilitadas o deshabilitadas; restaura el nivel
  original al retornar.

**Ejemplo de uso:**
```cpp
// En un loop largo, ceder CPU periódicamente:
for (int i = 0; i < BIG_N; i++) {
    DoWork(i);
    currentThread->Yield();
}
```

**Ver también:** `Thread::Sleep`, `Scheduler::FindNextToRun`

---

### `void Thread::Sleep()`

**Ubicación:** `threads/thread.cc:276`

**Descripción:**
Bloquea el thread actual: lo pone en estado `BLOCKED` y entrega la CPU al
siguiente thread listo. El thread no volverá a correr hasta que alguien llame
`scheduler->ReadyToRun(this)` (típicamente desde `Semaphore::V` o un handler
de interrupción).

**Parámetros:** (ninguno)

**Retorno:** `void`

**Efectos secundarios:**
- Cambia el estado del thread a `BLOCKED`.
- Si no hay threads listos, llama `interrupt->Idle()` repetidamente hasta que
  una interrupción ponga alguno.
- Llama `Scheduler::Run(nextThread)` — no retorna hasta que el thread sea
  re-schedulado.

**Precondiciones / contexto de llamada:**
- **Las interrupciones deben estar deshabilitadas** al llamar `Sleep()`. Es
  responsabilidad del llamador (semáforos, locks, etc.).
- `this == currentThread`.

**Ver también:** `Semaphore::P`, `Scheduler::Run`, `Thread::Yield`

---

### `void Thread::Finish()`

**Ubicación:** `threads/thread.cc:208`

**Descripción:**
Marca el thread como terminado. Si es joinable, escribe el exit status en el
`Channel` interno (despertando al joiner). Luego marca `threadToBeDestroyed` y
llama `Sleep()` para ceder la CPU; el destructor real lo invoca `Scheduler::Run`
en el contexto del siguiente thread.

**Parámetros:** (ninguno)

**Retorno:** `void` (no retorna — el thread deja de ejecutar)

**Efectos secundarios:**
- Deshabilita interrupciones.
- Si `joinable`: `pipe->Write(exitStatus)` — desbloquea al thread que hizo `Join`.
- Setea `threadToBeDestroyed = currentThread`.
- Llama `Sleep()` → entrega la CPU para siempre.

**Precondiciones / contexto de llamada:**
- Solo puede llamarse desde el thread que está terminando (`this == currentThread`).
- No puede llamarse dos veces.
- Para threads de usuario, lo llama `ThreadFinish()` que a su vez corre al terminar
  la función forkeada.

**Ver también:** `Thread::Join`, `Scheduler::Run` (donde se hace el `delete`)

---

### `int Thread::Join()`

**Ubicación:** `threads/thread.cc:352`

**Descripción:**
Bloquea el thread llamador hasta que `this` termine, y retorna el exit status
del thread esperado. Implementado con un `Channel` sincrónico interno.

**Parámetros:** (ninguno)

**Retorno:** Exit status del thread hijo (el valor pasado a `SetExitStatus` o
`pipe->Write`).

**Efectos secundarios:**
- Bloquea el thread llamador en `pipe->Read()` hasta que el hijo llame `Finish()`.
- No destruye el thread hijo — eso lo hace `SC_JOIN` o el scheduler.

**Precondiciones / contexto de llamada:**
- El thread debe haber sido creado con `joinable=true`.
- `this != currentThread` (un thread no puede hacer join sobre sí mismo).
- Solo **un** thread puede hacer join sobre el mismo thread hijo; múltiples joiners
  producen comportamiento indeterminado (el `Channel` no soporta múltiples lectores).

**Ejemplo de uso:**
```cpp
Thread *hijo = new Thread("hijo", true);
hijo->Fork(HijaFunc, nullptr);
int status = hijo->Join();  // bloquea hasta que HijaFunc termina
```

**Ver también:** `Thread::Finish`, `Channel::Read`, `syscall_SC_JOIN`

---

### `void Thread::StackAllocate(VoidFunctionPtr func, void *arg)`

**Ubicación:** `threads/thread.cc:324`

**Descripción:**
Función privada llamada por `Fork`. Aloca el stack del host y configura
`machineState[]` para que el primer `SWITCH` hacia este thread arranque
ejecutando `ThreadRoot`, que a su vez habilita interrupciones y llama `func(arg)`.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `func` | `VoidFunctionPtr` | Función del thread |
| `arg` | `void*` | Argumento de la función |

**Retorno:** `void`

**Efectos secundarios:**
- Aloca `STACK_SIZE * sizeof(uintptr_t)` bytes con `SystemDep::AllocBoundedArray`.
- Escribe `STACK_FENCEPOST (0xDEADBEEF)` en el fondo del stack (para `CheckOverflow`).
- Configura `machineState`:
  - `PCState` → `ThreadRoot`
  - `StartupPCState` → `InterruptEnable`
  - `InitialPCState` → `func`
  - `InitialArgState` → `arg`
  - `WhenDonePCState` → `ThreadFinish`
- Escribe `ThreadRoot` como dirección de retorno en el tope del stack (para que
  `SWITCH` lo encuentre con `ret`).

**Ver también:** `SWITCH` (switch_x86-64.S), `ThreadRoot`, `Thread::Fork`

---

### `void Thread::SaveUserState()` / `void Thread::RestoreUserState()`

**Ubicación:** `threads/thread.cc:399 / 412`

**Descripción:**
Guarda/restaura los `NUM_TOTAL_REGS` registros MIPS del proceso usuario en/desde
el array `userRegisters[]` del TCB. Llamados por `Scheduler::Run` en cada context
switch entre threads de usuario.

**Efectos secundarios:**
- `SaveUserState`: lee los registros actuales de `machine` y los guarda en `this->userRegisters[]`.
- `RestoreUserState`: escribe `this->userRegisters[]` en los registros de `machine`.

**Precondiciones / contexto de llamada:**
- Solo disponibles con `#ifdef USER_PROGRAM`.
- Deben llamarse con interrupciones deshabilitadas (lo garantiza `Scheduler::Run`).

**Ver también:** `Scheduler::Run`, `AddressSpace::SaveState / RestoreState`
