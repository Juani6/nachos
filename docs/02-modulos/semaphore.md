# Nivel 2 — Módulo: semaphore.cc / semaphore.hh

## Archivo
- **Ruta:** `threads/semaphore.cc`, `threads/semaphore.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa el semáforo clásico (Dijkstra P/V). Es la única primitiva de sincronización
que viene implementada en la base de NachOS; `Lock` y `Condition` se construyen sobre
ella. La atomicidad se logra deshabilitando interrupciones (sistema monoprocesador).

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Semaphore` | `Semaphore(const char* name, int initialValue)` | Inicializa value e crea cola de espera vacía |
| `~Semaphore` | `~Semaphore()` | Destruye la cola (no verifica que esté vacía) |
| `P` | `void P()` | Wait: deshabilita interrupciones; si `value==0` encola el thread y llama `Sleep()`; decrementa `value` |
| `V` | `void V()` | Signal: deshabilita interrupciones; si hay waiter lo despierta con `ReadyToRun`; incrementa `value` |
| `GetName` | `const char* GetName() const` | Retorna nombre de debug |

## Funciones/símbolos internos
_(ninguno — implementación directa en las dos funciones públicas)_

## Dependencias

- **Incluye:** `semaphore.hh`, `system.hh`
- **Es incluido por:** `lock.cc`, `condition.cc`, `channels.cc`, `synch_disk.cc`, `synchconsole.cc`

## Notas específicas de implementación

- **Atomicidad por deshabilitación de interrupciones:** `P()` y `V()` llaman `interrupt->SetLevel(INT_OFF)` y restauran el nivel anterior al terminar. Esto funciona porque NachOS simula un sistema uniprocessor.
- **`P()` usa `while` (no `if`):** aunque en un uniprocessor la condición no podría cambiar durante `Sleep` sin que el thread sea despertado por un `V`, el `while` defiende contra spurious wakeups o uso incorrecto.
- **`V()` sin dueño:** el semáforo no registra quién hizo `P()`; no detecta si el mismo thread llama `V()` dos veces seguidas. Esto lo diferencia del `Lock`, que sí tiene noción de ownership.
- **`Sleep()` con interrupciones deshabilitadas:** `Thread::Sleep` requiere que las interrupciones estén off cuando es llamado. `P()` las deshabilita antes de llamar `Sleep()`, que a su vez llama `Scheduler::Run`.
- El campo `value` es siempre `>= 0` por invariante (no hay semáforos negativos).
