# Nivel 3 — Referencia de Funciones: Scheduler

**Módulo:** [`threads/scheduler.cc`](../02-modulos/scheduler.md)
**Subsistema:** [Scheduler / Threads](../01-subsistemas/scheduler.md)

---

### `void Scheduler::ReadyToRun(Thread *thread, int oldPrio = -1)`

**Ubicación:** `threads/scheduler.cc:49`

**Descripción:**
Marca el thread como `READY` e inserta en `multiPriorityQueue[thread->GetPriority()]`.
Si se provee `oldPrio`, primero remueve el thread de la cola de esa prioridad (usado
para reubicar un thread que acaba de recibir priority inheritance).

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `thread` | `Thread*` | Thread a encolar; no debe ser `nullptr` |
| `oldPrio` | `int` | Prioridad previa del thread (para removerlo de esa cola); `-1` si no aplica |

**Retorno:** `void`

**Efectos secundarios:**
- Cambia el estado del thread a `READY`.
- Inserta el thread en `multiPriorityQueue[newPriority]` con `Append` (FIFO dentro del nivel).
- Si `oldPrio != -1`: llama `multiPriorityQueue[oldPrio]->Remove(thread)`.

**Precondiciones / contexto de llamada:**
- **Las interrupciones deben estar deshabilitadas** al llamar (invariante del scheduler).
- `thread != nullptr`.

**Ver también:** `Semaphore::V`, `Lock::Acquire` (priority inheritance), `Thread::Fork`

---

### `Thread *Scheduler::FindNextToRun()`

**Ubicación:** `threads/scheduler.cc:66`

**Descripción:**
Selecciona el próximo thread a ejecutar según prioridad. Recorre las colas de
mayor a menor prioridad (9 a 0) y hace `Pop()` en la primera no vacía.

**Parámetros:** (ninguno)

**Retorno:**
- Puntero al próximo thread a ejecutar.
- `nullptr` si todas las colas están vacías (no hay nadie listo).

**Efectos secundarios:**
- Remueve el thread retornado de su cola.

**Precondiciones / contexto de llamada:**
- **Las interrupciones deben estar deshabilitadas.**
- Si retorna `nullptr`, el llamador debe manejar el caso (p.ej. `Thread::Sleep`
  llama `interrupt->Idle()` en loop).

**Ver también:** `Thread::Yield`, `Thread::Sleep`, `Scheduler::Run`

---

### `void Scheduler::Run(Thread *nextThread)`

**Ubicación:** `threads/scheduler.cc:89`

**Descripción:**
Efectúa el context switch: guarda el estado del thread en ejecución, configura
`currentThread = nextThread`, llama `SWITCH`, restaura el estado del nuevo thread.
También destruye `threadToBeDestroyed` si quedó pendiente del switch anterior.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `nextThread` | `Thread*` | Thread que tomará la CPU; no debe ser `nullptr` |

**Retorno:** `void` (retorna en el contexto del `nextThread`, eventualmente)

**Efectos secundarios:**
- `[USER_PROGRAM]` Guarda registros MIPS del usuario y estado del AddressSpace del
  thread saliente (`SaveUserState`, `space->SaveState`).
- Cambia `currentThread` al nuevo thread.
- Llama `SWITCH(oldThread, nextThread)` — ejecución continúa en `nextThread`.
- Tras el switch (cuando `oldThread` es schedulado nuevamente): verifica
  `CheckOverflow()` del stack del thread saliente.
- `[USER_PROGRAM]` Restaura registros MIPS y AddressSpace del thread entrante
  (`RestoreUserState`, `space->RestoreState`).
- Si `threadToBeDestroyed != nullptr` y el thread no es joinable: lo destruye
  con `delete`.

**Precondiciones / contexto de llamada:**
- **Las interrupciones deben estar deshabilitadas** al llamar.
- El estado del thread saliente (READY o BLOCKED) ya debe estar actualizado antes
  de llamar `Run`.
- `nextThread != nullptr`.

**Notas de implementación:**
- `SWITCH` es una función ASM que salva todos los registros del host del thread
  saliente en `oldThread->machineState[]` y los restaura del `nextThread`.
- El `delete threadToBeDestroyed` se hace aquí (no en `Finish`) porque el thread
  terminado aún está corriendo en su propio stack cuando llama `Finish`; no se puede
  destruir hasta estar en el stack de otro thread.

**Ver también:** `SWITCH` (switch_x86-64.S), `Thread::Finish`, `Thread::Sleep`
