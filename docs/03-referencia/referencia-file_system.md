# Nivel 3 — Referencia de Funciones: FileSystem

**Módulo:** [`filesys/file_system.cc`](../02-modulos/file_system.md)
**Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

---

### `bool FileSystem::Create(const char *name, unsigned initialSize)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Crea un archivo regular en el filesystem. Resuelve el path para obtener el directorio
padre, alloca un FileHeader + sectores de datos en el freemap, y agrega la entrada
al directorio padre.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `name` | `const char*` | Path absoluto o relativo del archivo |
| `initialSize` | `unsigned` | Tamaño inicial en bytes (puede ser 0) |

**Retorno:** `true` si creado con éxito; `false` si ya existe, disco lleno, o directorio padre inaccesible.

**Efectos secundarios:**
- `ResolvePath(name)` → obtiene `(dirSector, basename)`.
- Bajo `fsLock`: alloca sector para FileHeader en freemap; alloca sectores de datos.
- Bajo `RWLock(dirSector)` en escritura: `dir->Add(basename, hdrSector, false)`.
- Escribe FileHeader en disco.
- No abre el archivo: el llamador debe hacer `Open` luego.

**Precondiciones / contexto de llamada:**
- Solo con `#ifdef FILESYS`.
- El directorio padre debe existir y tener espacio para una entrada más (o admitir expansión dinámica).

**Ver también:** `ResolvePath`, `FileHeader::Allocate`, `Directory::Add`

---

### `OpenFile* FileSystem::Open(const char *name)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Abre un archivo existente. Retorna un puntero a `OpenFile` cuya vida útil está
manejada por `FileTable`. El archivo queda abierto hasta que se destruya el `OpenFile`.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `name` | `const char*` | Path del archivo a abrir |

**Retorno:** `OpenFile*` si el archivo existe; `nullptr` si no se encuentra.

**Efectos secundarios:**
- `ResolvePath` → sector del directorio padre.
- Bajo `RWLock(dirSector)` en lectura: `dir->Find(basename)` → sector del inode.
- `fileTable->Acquire(sector)` — incrementa el refcount del archivo abierto.
- `new OpenFile(sector)` — construye el handle con el sector del FileHeader.

**Precondiciones / contexto de llamada:**
- El archivo debe existir. Para directorios: usar internamente, no desde userland directamente.

**Ver también:** `FileTable::Acquire`, `OpenFile`, `ResolvePath`

---

### `bool FileSystem::Remove(const char *name)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Elimina un archivo del filesystem. Si el archivo está actualmente abierto por algún
proceso, la eliminación física se difiere hasta que todos los handles sean cerrados
(semántica Unix `unlink`).

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `name` | `const char*` | Path del archivo a eliminar |

**Retorno:** `true` si la entrada fue removida del directorio; `false` si no existe.

**Efectos secundarios:**
- `ResolvePath` + `dir->Find(basename)` bajo `RWLock` en lectura → sector del inode.
- Bajo `RWLock(dirSector)` en escritura: `dir->Remove(basename)`.
- `fileTable->MarkAsDeleted(sector)`:
  - Si retorna `true` (nadie lo tiene abierto): llama `DeletePhysicalSector(sector)`.
  - Si retorna `false`: la liberación queda pendiente en `FileTable`; `~OpenFile` la completará.

**Ver también:** `FileTable::MarkAsDeleted`, `DeletePhysicalSector`, `OpenFile::~OpenFile`

---

### `bool FileSystem::CreateDir(const char *name)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Crea un nuevo directorio. Similar a `Create` pero el `isDirectory` flag de la
entrada se setea en `true`, y el contenido inicial del FileHeader es un array
de `DirectoryEntry` vacíos.

**Retorno:** `true` si creado; `false` si ya existe o disco lleno.

**Efectos secundarios:**
- Alloca FileHeader + sectores en freemap (bajo `fsLock`).
- Crea un `Directory` vacío con `NUM_DIR_ENTRIES` entradas y lo escribe en disco.
- `dir->Add(basename, hdrSector, true)` bajo `RWLock` en escritura sobre el padre.

**Notas:**
- Bug documentado en línea 783: en la rama de fallo de `dir->Add` (nombre duplicado
  tras el check previo con `Find`), el código setea `success = true` en lugar de `false`.
  <!-- nota: condición de carrera improbable — Find y Add no son atómicos — pero el 
  resultado sería crear el directorio sin entrada, con sectors ya allocados (leak). -->

**Ver también:** `FileSystem::Create`, `Directory::Add`

---

### `int FileSystem::ChangeDir(char *path)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Cambia el directorio de trabajo del proceso actual. Valida que el path resuelto
sea un directorio (no un archivo regular) antes de actualizar `currentDirSector`.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `path` | `char*` | Path absoluto o relativo del directorio destino |

**Retorno:** `0` en éxito; `-1` si el path no existe o no es directorio.

**Efectos secundarios:**
- `ResolvePath` para obtener el sector padre + basename.
- Bajo `RWLock` en lectura: `dir->FindEntry(basename)` para verificar `isDirectory`.
- `SetCurrentDirSector(sector)` actualiza el campo del thread actual.

**Ver también:** `GetCurrentDirSector`, `SetCurrentDirSector`, `ResolvePath`

---

### `pair<int,char*> FileSystem::ResolvePath(char* path)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Traduce un path a `(sector_del_directorio_padre, basename)`. Soporta paths
absolutos (`/`) y relativos (desde `currentDirSector`). Traversa componente a
componente con hand-over-hand locking en lectura sobre los `RWLock` de cada directorio.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `path` | `char*` | Path a resolver (puede ser modificado internamente por `strtok`) |

**Retorno:** `pair<int, char*>` — `(sector del directorio padre, puntero al basename)`.
Retorna `(-1, nullptr)` si algún componente no existe o no es directorio.

**Efectos secundarios:**
- Tokeniza el path con `parse()` → `vector<char*>`.
- Para cada componente intermedio:
  1. Adquiere `RWLock(nextSector)->AcquireRead()`.
  2. Libera `RWLock(currentSector)->ReleaseRead()`.
- Al retornar, el `RWLock` del directorio padre **no** está tomado — el llamador debe
  adquirirlo en el modo apropiado (lectura u escritura) para la operación que sigue.

**Precondiciones / contexto de llamada:**
- No tomar ningún lock de directorio antes de llamar.
- El path `"/"` retorna `(ROOT_SECTOR, nullptr)` — caso especial manejado.

**Notas:**
- Único uso de `std::vector` en el kernel (para la lista de componentes).
- El hand-over-hand garantiza que ningún directorio intermedio sea renombrado o
  eliminado mientras se traversa.

**Ver también:** `GetDirLock`, `Directory::Find`, `GetCurrentDirSector`

---

### `void FileSystem::DeletePhysicalSector(int sector)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Libera los sectores de datos y el sector del FileHeader de un archivo dado su
sector de inode. Operación destructiva e irreversible.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `sector` | `int` | Sector del FileHeader a liberar |

**Efectos secundarios:**
- Abre `FreeMap` desde disco.
- `hdr->Deallocate(freeMap)` — libera todos los sectores de datos.
- `freeMap->Clear(sector)` — libera el sector del FileHeader mismo.
- Escribe el freemap actualizado a disco.

**Precondiciones / contexto de llamada:**
- Solo debe llamarse cuando el refcount en `FileTable` llegó a 0 (nadie lo tiene abierto).
- Llamado desde `Remove` (si no había handles abiertos) o desde `~OpenFile` (si había
  handles y `deletePending` era true).
- Bajo `fsLock` para serializar con otras operaciones sobre el freemap.

**Ver también:** `FileHeader::Deallocate`, `FileTable::Release`, `OpenFile::~OpenFile`

---

### `RWLock* FileSystem::GetDirLock(int sector)`

**Ubicación:** `filesys/file_system.cc`

**Descripción:**
Retorna el `RWLock` asociado al directorio en `sector`. Inicialización lazy: si no
existe todavía, lo crea bajo `lockDirArr` (un `Lock` que protege el array de punteros).

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `sector` | `int` | Sector del directorio cuyo lock se necesita |

**Retorno:** `RWLock*` — nunca `nullptr`.

**Efectos secundarios:**
- Bajo `lockDirArr->Acquire()`: si `dirLocks[sector] == nullptr`, aloca `new RWLock(...)`.

**Precondiciones / contexto de llamada:**
- `sector` debe ser un sector válido del disco (0 ≤ sector < NUM_SECTORS).

**Ver también:** `ResolvePath`, `FileSystem::Create`, `FileSystem::Remove`
