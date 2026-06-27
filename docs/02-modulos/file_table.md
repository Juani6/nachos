# Nivel 2 — Módulo: file_table.cc / file_table.hh

## Archivo
- **Ruta:** `filesys/file_table.cc`, `filesys/file_table.hh`
- **Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

## Propósito del archivo
Tabla global de archivos abiertos, indexada por sector de inodo. Permite que múltiples
threads abran el mismo archivo y compartan un lock por inodo (`iNodeLock`), con contador
de referencias (`count`). Implementa eliminación diferida: si un archivo se marca para
borrar mientras hay referencias activas, se borra recién cuando el último `Release` lleva
el count a 0.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `FileTable` | `FileTable(unsigned size)` | Aloca `size` entradas; inicializa todas a `inUse=false, count=0` |
| `~FileTable` | `~FileTable()` | Destruye locks de todas las entradas activas y el array |
| `Acquire` | `FileTableEntry* Acquire(int sectorInodo)` | Bajo `internalLock`: busca entry existente (count++), o crea nueva (count=1, nuevo Lock); retorna nullptr si deletePending o tabla llena |
| `Release` | `bool Release(FileTableEntry*)` | Bajo `internalLock`: decrementa count; si count≤0 limpia la entrada; retorna true si `deletePending` (señal para borrar físicamente) |
| `MarkAsDeleted` | `bool MarkAsDeleted(int sectorInodo)` | Bajo `internalLock`: si nadie lo tiene abierto retorna true (borrar ya); si hay referencias setea `deletePending=true` y retorna false |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `FindEntry(int sector)` | Busca índice de la entrada con ese sector; retorna `(unsigned)-1` si no existe |

## Dependencias

- **Incluye:** `file_table.hh`, `lib/utility.hh`, `threads/lock.hh`
- **Es incluido por:** `file_system.cc` (Open → Acquire, Remove → MarkAsDeleted), `open_file.cc` (destructor → Release)

## Notas específicas de implementación

- **`internalLock`:** protege la integridad del array y el counter. Se toma y libera
  en cada operación pública; no se mantiene durante el uso del archivo (solo durante
  la búsqueda/modificación de metadata).

- **`iNodeLock` por entrada:** es el lock que `OpenFile::Read`/`Write` adquieren para
  serializar accesos al mismo archivo entre threads concurrentes. Se crea al crear la
  entrada en `Acquire` y se destruye en `Release` cuando count llega a 0.

- **Índice desde 2:** `Acquire` empieza a buscar entradas libres desde `k=2`, dejando
  las primeras dos reservadas (análogo a stdin/stdout, aunque no se usan así aquí).

- **Protocolo de eliminación diferida:**
  1. `FileSystem::Remove` llama `fileTable->MarkAsDeleted(sector)`.
  2. Si `MarkAsDeleted` retorna `true` → nadie lo tiene abierto → borrar directamente.
  3. Si retorna `false` → setea `deletePending=true` → cada `Release` posterior chequea.
  4. El último `Release` (count→0 con deletePending) retorna `true`, y `~OpenFile`
     llama `fileSystem->DeletePhysicalSector(hdrSector)`.

- **`MAX_OPEN_FILES = 32`:** si hay más de 32 archivos simultáneamente abiertos
  en el sistema, `Acquire` retorna nullptr y la operación falla.
