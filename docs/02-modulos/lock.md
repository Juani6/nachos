# Nivel 2 — Módulo: lock.cc / lock.hh

## Archivo
- **Ruta:** `threads/lock.cc`, `threads/lock.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa un mutex (exclusión mutua) con noción de ownership: solo el thread que
adquirió el lock puede liberarlo. Además implementa **priority inheritance**: si el
holder tiene menor prioridad que un thread que intenta adquirir el lock, su prioridad
se eleva temporalmente para evitar inversión de prioridad.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Lock` | `Lock(const char* name)` | Crea semáforo interno con `value=1`; `holderThread=nullptr` |
| `~Lock` | `~Lock()` | `ASSERT(holderThread==nullptr)`; destruye el semáforo |
| `Acquire` | `void Acquire()` | Intenta tomar el lock; aplica priority inheritance si necesario; bloquea con `sem->P()` |
| `Release` | `void Release()` | `ASSERT(IsHeldByCurrentThread())`; restaura prioridad original del holder; libera con `sem->V()` |
| `IsHeldByCurrentThread` | `bool IsHeldByCurrentThread() const` | Verdadero si `holderThread == currentThread` |
| `GetName` | `const char* GetName() const` | Retorna nombre de debug |

## Funciones/símbolos internos
_(ninguno — todo es público o privado de datos)_

## Dependencias

- **Incluye:** `lock.hh`, `system.hh`, `lib/list.hh`
- **Es incluido por:** `condition.cc`, `rwlock.cc`, `file_table.cc`, `synch_disk.cc`, `synchconsole.cc`, y múltiples sitios del kernel que necesitan exclusión mutua

## Notas específicas de implementación

- **Implementación sobre `Semaphore(1)`:** el lock es un semáforo binario con la restricción extra de que solo el dueño puede hacer `V()`.
- **Priority inheritance:** en `Acquire()`, si `holderThread` existe y su prioridad es menor a la del adquiriente (`currentThread`), se eleva la prioridad del holder:
  1. Se deshabilitan interrupciones.
  2. Si el holder está en estado `READY`, se llama `scheduler->ReadyToRun(holderThread, oldPrio)` para reubicarlo en la cola de prioridad más alta.
  3. Se restauran interrupciones.
  - Luego el thread actual se bloquea en `sem->P()` como siempre.
- **Restauración en `Release`:** `holderThread->SetPriority(holderThread->GetOriginalPriority())` restaura la prioridad antes de hacer `sem->V()`. Esto evita que el holder siga corriendo con prioridad elevada después de liberar el recurso.
- **No re-entrante:** `Acquire` tiene `ASSERT(!this->IsHeldByCurrentThread())`, por lo que un mismo thread no puede adquirir el lock dos veces sin liberarlo.
- El comentario en el archivo explica explícitamente por qué priority inheritance no es directamente implementable en semáforos (falta noción de ownership).
