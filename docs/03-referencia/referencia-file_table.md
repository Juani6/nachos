# Nivel 3 — Referencia de Funciones: FileTable

**Módulo:** [`filesys/file_table.cc`](../02-modulos/file_table.md)
**Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

---

### `int FileTable::Acquire(int sector)`

**Ubicación:** `filesys/file_table.cc`

**Descripción:**
Registra o incrementa el refcount de un archivo abierto identificado por el sector
de su inode. Si el sector ya está en la tabla, incrementa el contador. Si no, busca
una ranura libre (comenzando en índice 2, porque 0 y 1 son stdin/stdout reservados).

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `sector` | `int` | Sector del FileHeader del archivo a abrir |

**Retorno:** índice en la tabla del archivo (handle interno); `-1` si la tabla está llena (`MAX_OPEN_FILES=32`).

**Efectos secundarios:**
- Bajo `tableLock->Acquire()`.
- Si `table[k].sector == sector`: incrementa `table[k].count`.
- Si no encontrado: busca ranura libre desde `k=2`, setea `table[k] = {sector, 1, false}`.

**Precondiciones / contexto de llamada:**
- Llamado desde `FileSystem::Open` con el lock de directorio liberado.
- El sector debe corresponder a un FileHeader válido.

**Ver también:** `FileTable::Release`, `FileSystem::Open`

---

### `bool FileTable::Release(int sector)`

**Ubicación:** `filesys/file_table.cc`

**Descripción:**
Decrementa el refcount del archivo con inode en `sector`. Si llega a 0 y
`deletePending==true`, la tabla limpia la entrada y retorna `true` para indicar
que el llamador debe liberar los sectores físicos.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `sector` | `int` | Sector del FileHeader |

**Retorno:**
- `true` si `count == 0 && deletePending` → el llamador debe llamar `DeletePhysicalSector`.
- `false` en cualquier otro caso (hay más handles abiertos, o no estaba marcado para borrar).

**Efectos secundarios:**
- Bajo `tableLock`.
- Decrementa `table[k].count`.
- Si `count == 0 && deletePending`: limpia la entrada (`sector=-1, deletePending=false`).

**Precondiciones / contexto de llamada:**
- Llamado desde `~OpenFile`.
- Debe existir una entrada previa creada por `Acquire` con el mismo sector.

**Ver también:** `FileTable::MarkAsDeleted`, `OpenFile::~OpenFile`

---

### `bool FileTable::MarkAsDeleted(int sector)`

**Ubicación:** `filesys/file_table.cc`

**Descripción:**
Marca un archivo como pendiente de eliminación. Si el archivo no tiene handles
abiertos (`count == 0`), retorna `true` inmediatamente para que el llamador proceda
con la eliminación física. Si hay handles abiertos, setea `deletePending=true` y
retorna `false`; el último `Release` completará la eliminación.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `sector` | `int` | Sector del FileHeader del archivo a eliminar |

**Retorno:**
- `true` si `count == 0` (borrar ahora).
- `false` si hay handles abiertos (diferir).

**Efectos secundarios:**
- Bajo `tableLock`.
- Si `count > 0`: `table[k].deletePending = true`.
- Si `count == 0`: no modifica la entrada (no existe o ya está limpia).

**Precondiciones / contexto de llamada:**
- Llamado desde `FileSystem::Remove` después de remover la entrada del directorio.
- La entrada en el directorio ya no existe cuando se llama; no puede ser `Open`ado de nuevo.

**Ver también:** `FileTable::Release`, `FileSystem::Remove`, `OpenFile::~OpenFile`
