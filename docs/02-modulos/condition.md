# Nivel 2 — Módulo: condition.cc / condition.hh

## Archivo
- **Ruta:** `threads/condition.cc`, `threads/condition.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa variables de condición (monitor-style). `Wait` libera el lock externo
y bloquea; `Signal` despierta un waiter; `Broadcast` despierta a todos. Internamente
usa un semáforo y un lock auxiliar para contar waiters de forma segura.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Condition` | `Condition(const char* name, Lock* lock)` | Asocia la condición al lock externo; crea `internalLock` y `internalSem(0)` |
| `~Condition` | `~Condition()` | Destruye `internalLock` e `internalSem` |
| `Wait` | `void Wait()` | Incrementa `waiters`; libera `externalLock`; bloquea en `internalSem->P()`; readquiere `externalLock`; decrementa `waiters` |
| `Signal` | `void Signal()` | Si `waiters > 0`, hace `internalSem->V()` para despertar un waiter |
| `Broadcast` | `void Broadcast()` | Hace `internalSem->V()` tantas veces como `waiters` |
| `GetName` | `const char* GetName() const` | Retorna nombre de debug |

## Funciones/símbolos internos
_(ninguno)_

## Dependencias

- **Incluye:** `condition.hh`, `system.hh`
- **Es incluido por:** `rwlock.cc`

## Notas específicas de implementación

- **Patrón Mesa vs. Hoare:** la implementación sigue semántica Mesa (el waiter debe
  re-chequear la condición tras despertar). `Wait` re-adquiere el lock externo antes
  de retornar; el llamador debe usar `while` para re-verificar la condición.
- **`internalLock` para `waiters`:** el contador `waiters` se modifica bajo `internalLock`
  para evitar race conditions entre threads que hacen `Signal`/`Broadcast` y threads
  que entran a `Wait`.
- **Liberación del lock externo:** `Wait` llama `externalLock->Release()` antes de
  `internalSem->P()`. Si el orden fuera inverso, habría deadlock porque el thread se
  bloquearía sin liberar el lock que protege la condición.
- **`Broadcast`:** hace `V()` exactamente `waiters` veces (contadas bajo `internalLock`).
  Si entre el `internalLock->Acquire()` y los `V()` entran nuevos waiters, no serán
  despertados por este `Broadcast` — comportamiento correcto Mesa.
- Usado por `RWLock` para implementar el protocolo lector-escritor.
