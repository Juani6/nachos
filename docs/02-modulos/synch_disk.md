# Nivel 2 — Módulo: synch_disk.cc / synch_disk.hh

## Archivo
- **Ruta:** `filesys/synch_disk.cc`, `filesys/synch_disk.hh`
- **Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

## Propósito del archivo
Envuelve el `Disk` simulado (que es asincrónico — dispara una interrupción al terminar)
con una interfaz sincrónica bloqueante. `ReadSector`/`WriteSector` no retornan hasta
que la operación de disco terminó. Garantiza también que solo una operación de disco
ocurra a la vez.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `SynchDisk` | `SynchDisk(const char* name)` | Crea `Disk`, `Semaphore("synch disk", 0)` y `Lock("synch disk lock")` |
| `~SynchDisk` | `~SynchDisk()` | Destruye disco, lock y semáforo |
| `ReadSector` | `void ReadSector(int sectorNumber, char* data)` | Bajo lock: llama `disk->ReadRequest()`, espera en semáforo hasta interrupción |
| `WriteSector` | `void WriteSector(int sectorNumber, const char* data)` | Bajo lock: llama `disk->WriteRequest()`, espera en semáforo hasta interrupción |
| `RequestDone` | `void RequestDone()` | Callback del Disk al terminar: `semaphore->V()` |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `DiskRequestDone(void* arg)` (static) | Wrapper C para el callback: castea arg a `SynchDisk*` y llama `RequestDone()` |

## Dependencias

- **Incluye:** `synch_disk.hh`
- **Es incluido por:** `file_header.cc`, `open_file.cc`, `file_system.cc` (via `synchDisk` global de `system.hh`)

## Notas específicas de implementación

- **Patrón async→sync:** el disco hardware simulado (`machine/disk.cc`) toma una
  request y retorna inmediatamente; la operación real ocurre con latencia simulada y
  dispara un callback. `SynchDisk` convierte este modelo en sincrónico con el patrón
  clásico: `request → semaphore.P() → [interrupción] → semaphore.V() → return`.

- **Un lock para serializar:** `lock` garantiza que solo una operación de disco esté
  en vuelo a la vez (el disco simulado no soporta operaciones paralelas). Un thread
  que llega mientras hay una operación en curso queda bloqueado en `lock->Acquire()`.

- **`RequestDone` y semáforo:** el semáforo `semaphore` tiene valor inicial 0. Cada
  `ReadSector`/`WriteSector` hace `P()` (bloquea) y la interrupción del disco hace
  `V()` (desbloquea). El lock no se libera hasta después del `P()`, garantizando
  que no haya una segunda request mientras se espera la primera.

- **Uso global:** `synchDisk` es una variable global en `system.hh` (disponible con
  `#ifdef FILESYS`), inicializada en `system.cc`. Toda operación de disco del filesystem
  pasa por este objeto.
