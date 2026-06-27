# Nivel 2 — Módulo: transfer.cc / transfer.hh

## Archivo
- **Ruta:** `userprog/transfer.cc`, `userprog/transfer.hh`
- **Subsistema:** [Gestión de Procesos y Syscalls](../01-subsistemas/procesos.md)

## Propósito del archivo
Provee funciones para copiar datos entre el espacio de direcciones del kernel (host)
y el espacio virtual de un proceso MIPS usuario. Toda transferencia pasa por
`machine->ReadMem` / `machine->WriteMem`, que invocan la MMU/TLB simulada. También
serializa la tabla de procesos (`processTable`) al espacio usuario para `SC_GETPT`.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `ReadBufferFromUser` | `void ReadBufferFromUser(int userAddr, char* outBuffer, unsigned byteCount)` | Copia `byteCount` bytes desde el espacio MIPS usuario al buffer del kernel; 1 byte a la vez con retry por TLB miss |
| `ReadStringFromUser` | `bool ReadStringFromUser(int userAddr, char* outString, unsigned maxByteCount)` | Copia string null-terminated desde espacio MIPS; retorna `true` si terminó en `\0` antes de `maxByteCount` |
| `WriteBufferToUser` | `void WriteBufferToUser(const char* buffer, int userAddr, unsigned byteCount)` | Copia `byteCount` bytes del kernel al espacio MIPS usuario; 1 byte a la vez con retry |
| `WriteStringToUser` | `void WriteStringToUser(const char* string, int userAddr)` | Copia string null-terminated (incluyendo el `\0`) al espacio MIPS usuario |
| `writeProcessDataToUser` | `int writeProcessDataToUser(int userAddr)` | Serializa todas las entradas no-nulas de `processTable` como array de `_processData` al espacio usuario; retorna cantidad de procesos escritos |

## Funciones/símbolos internos
_(ninguno — todas las funciones son públicas)_

## Dependencias

- **Incluye:** `transfer.hh`, `lib/utility.hh`, `threads/system.hh`, `process_data.h`
- **Es incluido por:** `exception.cc` (todos los syscall handlers que mueven strings o buffers)

## Notas específicas de implementación

- **Retry por TLB miss (`TLB_TRIES`):** cada llamada a `machine->ReadMem` /
  `machine->WriteMem` puede fallar si la página no está en la TLB (retorna `false`).
  Las funciones de transfer reintentan hasta `TLB_TRIES` veces. En la práctica, la
  primera falla dispara `PageFaultHandler` que carga la página, y el segundo intento
  tiene éxito. `TLB_TRIES` está definido en los headers de la MMU.

- **Granularidad de 1 byte:** todas las transferencias van de a 1 byte. Esto simplifica
  el manejo de páginas parciales y la detección de fin de string, pero implica que copiar
  N bytes hace N llamadas a `ReadMem`/`WriteMem`.

- **`writeProcessDataToUser`:** itera `Table<Thread*>::SIZE` entradas de `processTable`,
  filtra las nulas, y escribe structs `_processData` contiguos en la dirección usuario
  dada. El campo `name` se copia hasta `FILE_NAME_MAX_LEN` caracteres. El resultado
  (cantidad de procesos escritos) va a `r2` via `SC_GETPT`.

- **No hay lock en `writeProcessDataToUser`:** la función no adquiere `pTLock` mientras
  copia; la tabla podría cambiar durante la iteración si otro thread termina en paralelo.
  En la práctica esto no falla porque se usa solo para debugging/`ps`.
