# Nivel 2 — Módulo: scheduler.cc / scheduler.hh

## Archivo
- **Ruta:** `threads/scheduler.cc`, `threads/scheduler.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa el dispatcher de NachOS: mantiene las colas de threads listos para correr
y realiza el context switch. Usa un array de 10 listas FIFO (`multiPriorityQueue`)
para soportar prioridades estáticas; `FindNextToRun` elige el thread de mayor prioridad
disponible.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Scheduler` | `Scheduler()` | Crea `QUEUE_LEVEL=10` listas `List<Thread*>` vacías |
| `~Scheduler` | `~Scheduler()` | Destruye todas las listas |
| `ReadyToRun` | `void ReadyToRun(Thread*, int oldPrio=-1)` | Marca thread como READY y lo inserta en `multiPriorityQueue[thread->GetPriority()]`; si `oldPrio != -1` lo quita de la cola anterior (usado para priority inheritance) |
| `FindNextToRun` | `Thread* FindNextToRun()` | Recorre las colas de mayor a menor prioridad y hace `Pop()` del primero que encuentra; retorna `nullptr` si no hay nadie |
| `Run` | `void Run(Thread* nextThread)` | Guarda estado del thread saliente, cambia `currentThread`, llama `SWITCH`, restaura estado del nuevo; destruye `threadToBeDestroyed` si quedó pendiente |
| `Print` | `void Print()` | Imprime contenido de todas las colas (debug) |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `ThreadPrint(Thread*)` (static) | Helper para `Apply`; llama `t->Print()` |
| `QUEUE_LEVEL = 10` | Constante: número de niveles de prioridad |

## Dependencias

- **Incluye:** `scheduler.hh`, `system.hh`
- **Es incluido por:** `thread.cc` (Fork, Yield, Sleep), `semaphore.cc` (V), `lock.cc` (Acquire), cualquier primitiva de sincronización que desbloquea threads

## Notas específicas de implementación

- **Exclusión mutua:** todas las operaciones asumen interrupciones deshabilitadas al momento de ser llamadas. No se usa `Lock` internamente para evitar recursión infinita (un `Lock` necesitaría al scheduler para dormir).
- **`Run` y context switch:** tras `SWITCH(oldThread, nextThread)`, la ejecución continúa en el nuevo thread. Cuando `oldThread` vuelve a ser schedulado, retoma justo después del `SWITCH`. Por eso la destrucción de `threadToBeDestroyed` ocurre en `Run`, no en `Finish`.
- **Priority inheritance:** `ReadyToRun` acepta `oldPrio` para reubicar un thread que cambió de prioridad mientras estaba en ready (llamado desde `Lock::Acquire`). Hace `Remove(thread)` de la cola vieja antes de `Append` en la nueva.
- **Scheduler con `USER_PROGRAM`:** antes del `SWITCH`, guarda registros MIPS del usuario (`SaveUserState`) y el estado del address space (`SaveState`). Después del switch restaura los del thread entrante.
- Con `QUEUE_LEVEL=10` y prioridades 0–9, la prioridad default de un thread nuevo es 4. Dentro de cada nivel se usa FIFO estricto.
