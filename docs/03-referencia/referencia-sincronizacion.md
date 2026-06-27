# Nivel 3 — Referencia de Funciones: Primitivas de Sincronización

Agrupa las funciones no triviales de `Semaphore`, `Lock`, `Condition`, `Channel` y `RWLock`.

**Módulos:** [`semaphore.cc`](../02-modulos/semaphore.md) · [`lock.cc`](../02-modulos/lock.md) · [`condition.cc`](../02-modulos/condition.md) · [`channels.cc`](../02-modulos/channels.md) · [`rwlock.cc`](../02-modulos/rwlock.md)
**Subsistema:** [Scheduler / Threads](../01-subsistemas/scheduler.md)

---

## Semaphore

### `void Semaphore::P()`

**Ubicación:** `threads/semaphore.cc:61`

**Descripción:**
Wait: si `value == 0`, encola el thread actual y lo bloquea (`Sleep()`). Cuando
algún otro thread haga `V()`, este thread es despertado y decrementa `value`.

**Retorno:** `void`

**Efectos secundarios:**
- Deshabilita interrupciones; restaura el nivel original al retornar.
- Puede bloquear el thread actual (si `value == 0`).
- Decrementa `value` al adquirir.

**Precondiciones / contexto de llamada:**
- Puede llamarse con interrupciones habilitadas o deshabilitadas.
- No llamar desde un contexto donde bloquear sea inválido (p.ej. durante
  manejo de interrupción en el simulador).

**Ver también:** `Semaphore::V`, `Thread::Sleep`

---

### `void Semaphore::V()`

**Ubicación:** `threads/semaphore.cc:83`

**Descripción:**
Signal: si hay threads esperando, despierta el primero con `ReadyToRun`. Siempre
incrementa `value` (incluso si despertó a alguien, porque el waiter decrementará
en su `P()`).

**Retorno:** `void`

**Efectos secundarios:**
- Deshabilita interrupciones; restaura el nivel original al retornar.
- Si la cola tiene waiters: llama `scheduler->ReadyToRun(thread)` con el primero.
- Incrementa `value`.

**Precondiciones / contexto de llamada:**
- Puede llamarse con interrupciones habilitadas o deshabilitadas (incluido desde
  handlers de interrupción como `SynchDisk::RequestDone`).
- No hay restricción sobre quién llama `V()` — a diferencia de `Lock`, no hay
  noción de dueño.

**Ver también:** `Semaphore::P`, `Scheduler::ReadyToRun`

---

## Lock

### `void Lock::Acquire()`

**Ubicación:** `threads/lock.cc:51`

**Descripción:**
Adquiere el lock (exclusión mutua). Bloquea si ya está tomado. Implementa
**priority inheritance**: si el holder tiene menor prioridad que el adquiriente,
se eleva la prioridad del holder temporalmente.

**Retorno:** `void`

**Efectos secundarios:**
- Si hay inversión de prioridad: deshabilita interrupciones, ajusta la prioridad
  del holder en la ready queue vía `scheduler->ReadyToRun(holderThread, oldPrio)`.
- Llama `sem->P()` — puede bloquear el thread.
- Setea `holderThread = currentThread`.

**Precondiciones / contexto de llamada:**
- El thread actual **no** debe ser el holder (`ASSERT(!IsHeldByCurrentThread())`).
  El lock no es re-entrante.

**Ejemplo de uso:**
```cpp
Lock *l = new Lock("miLock");
l->Acquire();
// sección crítica
l->Release();
```

**Ver también:** `Lock::Release`, `Lock::IsHeldByCurrentThread`

---

### `void Lock::Release()`

**Ubicación:** `threads/lock.cc:77`

**Descripción:**
Libera el lock. Restaura la prioridad original del holder antes de liberar,
para deshacer cualquier priority inheritance aplicado durante `Acquire`.

**Retorno:** `void`

**Efectos secundarios:**
- `ASSERT(IsHeldByCurrentThread())` — solo el dueño puede liberar.
- Restaura `holderThread->priority = holderThread->originalPriority`.
- Setea `holderThread = nullptr`.
- Llama `sem->V()` — puede despertar un thread bloqueado en `Acquire`.

**Precondiciones / contexto de llamada:**
- Solo puede llamarse desde el thread que hizo `Acquire`.

**Ver también:** `Lock::Acquire`, `Semaphore::V`

---

## Condition

### `void Condition::Wait()`

**Ubicación:** `threads/condition.cc:48`

**Descripción:**
Libera atómicamente el lock externo y bloquea el thread hasta recibir `Signal`
o `Broadcast`. Al despertar, readquiere el lock externo antes de retornar.

**Retorno:** `void`

**Efectos secundarios:**
- Bajo `internalLock`: incrementa `waiters`.
- Llama `externalLock->Release()`.
- Bloquea en `internalSem->P()`.
- Al despertar: llama `externalLock->Acquire()`.
- Bajo `internalLock`: decrementa `waiters`.

**Precondiciones / contexto de llamada:**
- El thread llamador **debe** tener el lock externo (`externalLock`) adquirido.
- Usar **siempre** con `while` (semántica Mesa):
  ```cpp
  while (!condicion) cond->Wait();
  ```

**Ver también:** `Condition::Signal`, `Condition::Broadcast`

---

### `void Condition::Signal()` / `void Condition::Broadcast()`

**Ubicación:** `threads/condition.cc:66 / 77`

**Descripción:**
- `Signal`: despierta **un** thread en espera (hace `internalSem->V()` una vez si
  `waiters > 0`).
- `Broadcast`: despierta **todos** los threads en espera (hace `internalSem->V()`
  `waiters` veces).

**Retorno:** `void`

**Efectos secundarios:**
- Ambas operan bajo `internalLock`.
- `Signal`: `V()` una vez si hay waiters.
- `Broadcast`: `V()` exactamente `waiters` veces (snapshot del contador).

**Precondiciones / contexto de llamada:**
- El lock externo puede o no estar tomado al llamar (no es requisito en este
  modelo Mesa; convención depende del uso específico).
- Llamar `Signal`/`Broadcast` sin waiters es no-op seguro.

**Ver también:** `Condition::Wait`, `RWLock::ReleaseWrite`

---

## Channel

### `void Channel::Write(int value)` / `int Channel::Read()`

**Ubicación:** `threads/channels.cc:34 / 44`

**Descripción:**
Paso de mensajes sincrónico (rendez-vous). `Write` bloquea hasta que `Read`
consume el mensaje; `Read` bloquea hasta que `Write` deposita uno.

**Parámetros (`Write`):**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `value` | `int` | Valor a enviar |

**Retorno (`Read`):** `int` — el valor enviado por `Write`.

**Efectos secundarios (`Write`):**
- `writer->P()` — espera turno de escritura (solo un escritor a la vez).
- Deposita `msg = value`.
- `reader->V()` — notifica al lector.
- `consumed->P()` — espera confirmación de consumo.
- `writer->V()` — libera el canal.

**Efectos secundarios (`Read`):**
- `reader->P()` — espera mensaje disponible.
- Lee `msg`.
- `consumed->V()` — confirma consumo al escritor.

**Precondiciones / contexto de llamada:**
- Canal de capacidad 0: exactamente un emisor y un receptor por mensaje.
- No soporta múltiples lectores concurrentes sobre el mismo canal.
- Usado internamente por `Thread::Join` / `Thread::Finish`.

**Ver también:** `Thread::Join`, `Thread::Finish`

---

## RWLock

### `void RWLock::AcquireRead()` / `void RWLock::ReleaseRead()`

**Ubicación:** `threads/rwlock.cc:18 / 27`

**Descripción:**
`AcquireRead`: espera a que no haya escritor activo, luego incrementa `readers`.
`ReleaseRead`: decrementa `readers`; si llega a 0, señala para despertar escritores.

**Efectos secundarios:**
- `AcquireRead`: bajo `lock`; `while(writing) cond->Wait()`; `readers++`.
- `ReleaseRead`: bajo `lock`; `readers--`; si `readers==0`: `cond->Signal()`.

**Precondiciones:** Llamar en pares `AcquireRead` / `ReleaseRead`.

---

### `void RWLock::AcquireWrite()` / `void RWLock::ReleaseWrite()`

**Ubicación:** `threads/rwlock.cc:38 / 48`

**Descripción:**
`AcquireWrite`: espera a que no haya ni escritores ni lectores activos; setea
`writing=true`. `ReleaseWrite`: setea `writing=false` y hace `Broadcast` para
despertar a todos los pendientes (lectores y escritores).

**Efectos secundarios:**
- `AcquireWrite`: bajo `lock`; `while(writing || readers>0) cond->Wait()`; `writing=true`.
- `ReleaseWrite`: bajo `lock`; `writing=false`; `cond->Broadcast()`.

**Precondiciones:** Llamar en pares. Solo un escritor a la vez puede tener el lock.

**Ver también:** `FileSystem::GetDirLock`, `FileSystem::ResolvePath`
