# Nivel 2 — Módulo: rwlock.cc / rwlock.hh

## Archivo
- **Ruta:** `threads/rwlock.cc`, `threads/rwlock.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa un lock lector-escritor. Múltiples lectores pueden tener el lock
simultáneamente; un escritor tiene exclusión total. Usado exclusivamente en el
filesystem para proteger los directorios (`dirLocks[NUM_SECTORS]`).

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `RWLock` | `RWLock()` | Crea `Lock* lock`, `Condition* cond`, `readers=0`, `writing=false` |
| `~RWLock` | `~RWLock()` | Destruye `lock` y `cond` |
| `AcquireRead` | `void AcquireRead()` | Bajo `lock`: espera a que no haya escritor activo (`while writing: cond.Wait()`); incrementa `readers`; libera `lock` |
| `ReleaseRead` | `void ReleaseRead()` | Bajo `lock`: decrementa `readers`; si `readers==0` señala con `cond.Signal()` para despertar un escritor pendiente |
| `AcquireWrite` | `void AcquireWrite()` | Bajo `lock`: espera a que no haya ni escritor ni lectores (`while writing || readers>0: cond.Wait()`); setea `writing=true` |
| `ReleaseWrite` | `void ReleaseWrite()` | Bajo `lock`: setea `writing=false`; `cond.Broadcast()` para despertar todos los pendientes |

## Funciones/símbolos internos
_(ninguno — toda la lógica está en los 4 métodos públicos)_

## Dependencias

- **Incluye:** `rwlock.hh`, `condition.hh`, `lock.hh`
- **Es incluido por:** `file_system.cc` (dirLocks), `file_system.hh`

## Notas específicas de implementación

- **Semántica:** preferencia neutral entre lectores y escritores. Si hay lectores activos
  y llega un escritor, el escritor espera. Si hay un escritor activo, los nuevos lectores
  esperan. No hay política anti-starvation explícita.
- **`ReleaseWrite` usa `Broadcast`:** como puede haber tanto lectores como escritores
  esperando, se despierta a todos y cada uno re-evalúa la condición (`while` en lugar
  de `if` en AcquireRead/AcquireWrite — semántica Mesa).
- **`ReleaseRead` usa `Signal`:** solo un escritor puede entrar, entonces basta con
  despertar uno.
- **Init lazy en FileSystem:** los `RWLock` del array `dirLocks[NUM_SECTORS]` se crean
  en `GetDirLock(sector)` la primera vez que se accede a ese directorio (bajo `lockDirArr`
  para evitar race en la creación).
