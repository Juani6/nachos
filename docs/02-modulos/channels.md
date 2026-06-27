# Nivel 2 — Módulo: channels.cc / channels.hh

## Archivo
- **Ruta:** `threads/channels.cc`, `threads/channels.hh`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa un canal sincrónico de paso de mensajes enteros entre threads (rendez-vous).
El emisor bloquea hasta que el receptor haya consumido el mensaje. Usado internamente
por `Thread::Join` (cada thread joinable tiene un `Channel` privado) y expuesto como
primitiva de usuario via `SC_FORK`/tests.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Channel` | `Channel(const char* name)` | Crea tres semáforos: `writer(1)`, `reader(0)`, `consumed(0)` |
| `~Channel` | `~Channel()` | Destruye los tres semáforos |
| `Write` | `void Write(int value)` | Envío bloqueante: espera turno (`writer->P()`), deposita msg, señala al receptor (`reader->V()`), espera confirmación de consumo (`consumed->P()`), libera turno (`writer->V()`) |
| `Read` | `int Read()` | Recepción bloqueante: espera mensaje (`reader->P()`), lee msg, confirma consumo (`consumed->V()`); retorna el valor |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `msg` (int) | Campo de la clase donde se deposita el valor entre `Write` y `Read` |
| `writer` (Semaphore*) | Semáforo binario (init=1): garantiza que solo un emisor a la vez |
| `reader` (Semaphore*) | Semáforo (init=0): señala al lector que hay mensaje disponible |
| `consumed` (Semaphore*) | Semáforo (init=0): el lector confirma al escritor que consumió el mensaje |

## Dependencias

- **Incluye:** `channels.hh`, `system.hh`
- **Es incluido por:** `thread.cc` (para Join), `thread_test_prod_cons_channels.cc`

## Notas específicas de implementación

- **Protocolo de 3 semáforos:** garantiza que el mensaje no se sobreescriba antes de
  ser leído, y que el emisor no libere el canal antes de la confirmación de consumo.
  El flujo exacto:
  ```
  Write:  writer.P → msg=value → reader.V → consumed.P → writer.V
  Read:              reader.P → readMsg=msg → consumed.V → return readMsg
  ```
- **Un solo mensaje en vuelo:** `writer` es binario (init=1), por lo que solo un
  emisor puede estar en la fase "enviando" a la vez.
- **Uso en Join:** `Thread::Finish` llama `pipe->Write(exitStatus)` y `Thread::Join`
  llama `pipe->Read()`. El canal asegura que el joiner recibe exactamente un valor y
  que el thread terminado espera hasta que el joiner lo recibió antes de continuar
  hacia `Sleep()`.
- **Sin buffer:** el canal es de capacidad 0 (rendez-vous puro). No hay cola de
  mensajes pendientes.
