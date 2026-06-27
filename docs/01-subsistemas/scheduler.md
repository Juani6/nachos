# Nivel 1 — Subsistema: Scheduler / Threads / Sincronización

## Propósito
Gestionar la multiprogramación cooperativa/preemptiva dentro del proceso Linux que
hospeda NachOS. Provee la abstracción de thread (TCB), un scheduler con múltiples
colas de prioridad, y primitivas de sincronización (Semaphore, Lock, Condition,
Channel, RWLock) que el resto del kernel usa internamente.

## Archivos involucrados

| Archivo | Lenguaje | Rol |
|---------|----------|-----|
| `threads/thread.cc / .hh` | C++ | Clase `Thread`: TCB, ciclo de vida, stack, join, prioridad |
| `threads/scheduler.cc / .hh` | C++ | `Scheduler`: colas de prioridad, `ReadyToRun`, `FindNextToRun`, `Run` |
| `threads/semaphore.cc / .hh` | C++ | `Semaphore`: primitiva base P/V |
| `threads/lock.cc / .hh` | C++ | `Lock`: mutex implementado sobre `Semaphore` |
| `threads/condition.cc / .hh` | C++ | `Condition`: variable de condición (Wait/Signal/Broadcast) |
| `threads/channels.cc / .hh` | C++ | `Channel`: canal sincrónico entero (Write/Read con rendez-vous) |
| `threads/rwlock.cc / .hh` | C++ | `RWLock`: lector-escritor sobre Lock+Condition |
| `threads/synch_list.hh` | C++ | Lista sincronizada genérica (template, usa Lock) |
| `threads/switch.S` | ASM | Wrapper de context switch — delega a la variante de arquitectura |
| `threads/switch_x86-64.S / .h` | ASM | Context switch real en x86-64 |
| `threads/switch_i386.S / .h` | ASM | Context switch real en i386 |
| `threads/thread_test*.cc / .hh` | C++ | Suite de tests: simple, garden, prod/cons, join, lock, channels, IP |

## Flujo de control

### Creación y arranque de un thread
```
new Thread(name, joinable, priority)
    |
Thread::Fork(func, arg)
    -> StackAllocate()   // aloca stack del host, pushea ThreadRoot frame
    -> scheduler->ReadyToRun(this)  // lo pone en multiPriorityQueue[priority]
```

### Context switch (preemptivo o voluntario)
```
Timer interrupt / Thread::Yield() / Thread::Sleep()
    |
    v
scheduler->FindNextToRun()
    -> recorre multiPriorityQueue[9..0], devuelve el primero no vacío
    |
    v
scheduler->Run(nextThread)
    -> [USER_PROGRAM] currentThread->SaveUserState() + space->SaveState()
    -> SWITCH(oldThread, nextThread)   // ASM: salva registros host, restaura los de next
    -> [USER_PROGRAM] currentThread->RestoreUserState() + space->RestoreState()
    -> destruye threadToBeDestroyed si corresponde
```

### Join
```
Thread::Join()
    -> usa Channel interno (pipe): bloquea al llamador en pipe->Read()
Thread::Finish()
    -> escribe exitStatus en pipe->Write()  (desbloquea al joiner)
    -> se marca como threadToBeDestroyed
```

### Semaphore
```
P()  -> deshabilita interrupciones; si value==0, Sleep(); else value--
V()  -> deshabilita interrupciones; si hay waiter, ReadyToRun(waiter); else value++
```

## Estructuras de datos clave

| Estructura | Definida en | Descripción |
|------------|-------------|-------------|
| `Thread` | `threads/thread.hh` | TCB: stackTop, machineState[17], status, priority, joinable, pipe, fdTable, space |
| `ThreadStatus` | `threads/thread.hh` | Enum: JUST_CREATED / RUNNING / READY / BLOCKED |
| `Scheduler::multiPriorityQueue` | `threads/scheduler.hh` | Array de 10 `List<Thread*>*`, índice = prioridad (0=baja, 9=alta) |
| `Semaphore` | `threads/semaphore.hh` | value (int ≥ 0) + queue de threads bloqueados |
| `RWLock` | `threads/rwlock.hh` | readers (int), writing (bool), Lock + Condition |
| `Channel` | `threads/channels.hh` | msg (int), semáforos writer/reader/consumed |
| `currentThread` | `threads/system.hh` | Puntero global al thread en CPU |
| `threadToBeDestroyed` | `threads/system.hh` | Thread que terminó pero aún no fue `delete`d |

## Dependencias
- **Depende de:** `machine/interrupt` (deshabilitar interrupciones en P/V), `lib/list`
- **Es usado por:** todos los demás subsistemas (filesystem usa RWLock; userprog usa
  Thread para procesos de usuario; vmem usa Lock para CoreMap)

## Invariantes / precondiciones importantes
- Las interrupciones deben estar deshabilitadas durante `P()`, `V()`, `ReadyToRun()`,
  `FindNextToRun()` y `Run()` — es el mecanismo de exclusión mutua del kernel.
- `SWITCH` asume que `stackTop` es el **primer campo** de `Thread` y `machineState`
  el **segundo** (están marcados con `// NOTE: DO NOT CHANGE the order`).
- El stack de cada thread es de tamaño fijo (`STACK_SIZE = 4 * 1024` palabras);
  overflow no se detecta de forma garantizada.
- Un thread `joinable` no se destruye en `Finish()` — el joiner lo destruye tras
  leer el exit status vía `Channel`.

## Notas de diseño
- El scheduler es de prioridad estática con 10 niveles; dentro de cada nivel, FIFO.
  `ReadyToRun` acepta un `oldPrio` opcional para reubicar el thread si cambió de
  prioridad (usado para priority inheritance).
- El `Channel` se usa tanto para la primitiva de comunicación entre threads de
  usuario (`SC_FORK`/channels test) como internamente para implementar `Join`:
  cada thread joinable tiene un `Channel* pipe` privado.
- Se evitan `Lock` dentro del scheduler para no crear dependencias circulares
  (el scheduler necesita ser llamado cuando un lock no puede adquirirse).
- ASM de context switch separado por arquitectura (`switch_x86-64.S` / `switch_i386.S`);
  `switch.S` / `switch.h` actúan como wrapper que elige cuál incluir según flags de
  compilación.
- No se usan excepciones C++ ni RTTI en ningún archivo del subsistema (entorno
  freestanding).

## Limitaciones conocidas / TODOs
- Stack de tamaño fijo: overflow silencioso si el thread usa mucho stack.
- El scheduler no implementa aging (un thread de baja prioridad puede hacer starvation).
