# Nivel 2 — Módulo: file_system.cc / file_system.hh

## Archivo
- **Ruta:** `filesys/file_system.cc`, `filesys/file_system.hh`
- **Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

## Propósito del archivo
Capa de más alto nivel del filesystem: gestiona el formateo del disco, el freemap global,
el directorio raíz, la resolución de paths absolutos y relativos, y las operaciones de
directorio (`CreateDir`, `ChangeDir`, `List`). Coordina el acceso concurrente con
`RWLock` por directorio y `Lock` para el freemap. Tiene implementación dual:
`FILESYS_STUB` (delega al host) y `FILESYS` (filesystem completo sobre disco simulado).

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `FileSystem` | `FileSystem(bool format)` | Si `format=true`: inicializa freemap y directorio raíz en disco. Si `false`: abre los archivos existentes. En ambos casos deja `freeMapFile` y `directoryFile` abiertos. |
| `~FileSystem` | `~FileSystem()` | Cierra freeMapFile, directoryFile; destruye `fsLock`, `lockDirArr`, todos los `dirLocks` allocados. |
| `Create` | `bool Create(const char* name, unsigned initialSize)` | Resuelve path, adquiere RWLock del directorio padre en escritura, alloca FileHeader + sectores, agrega entrada al directorio; bajo `fsLock` para el freemap. |
| `Open` | `OpenFile* Open(const char* name)` | Resuelve path, lee directorio bajo RWLock en lectura, adquiere `FileTable::Acquire(sector)`, retorna `OpenFile`. |
| `Remove` | `bool Remove(const char* name)` | Resuelve path, bajo RWLock en escritura: elimina entrada del directorio, llama `fileTable->MarkAsDeleted(sector)`; si nadie lo tiene abierto, `DeletePhysicalSector`. |
| `CreateDir` | `bool CreateDir(const char* name)` | Crea un nuevo directorio: alloca FileHeader + sectores para el directorio nuevo, agrega la entrada al padre con `isDirectory=true`. |
| `ChangeDir` | `int ChangeDir(char* path)` | Resuelve path, verifica que sea directorio, actualiza `currentDirSector` del proceso (o `kernelDirSector` para threads kernel). |
| `List` | `void List()` | Lista el directorio de trabajo actual del proceso. |
| `Check` | `bool Check()` | Verificación de consistencia: compara el freemap real con un freemap reconstruido recorriendo todos los inodes. |
| `Print` | `void Print()` | Debug: imprime freemap, directorio raíz y contenido de todos los archivos. |
| `DeletePhysicalSector` | `void DeletePhysicalSector(int sector)` | Libera inodo + bloques de datos en el freemap; usado por `Remove` y `~OpenFile`. |
| `GetFreeMapFile` | `OpenFile* GetFreeMapFile()` | Accessor para `FileHeader::Extend` que necesita actualizar el freemap. |
| `GetDirFile` | `OpenFile* GetDirFile()` | Accessor del directorio raíz. |
| `ResolvePath` | `pair<int,char*> ResolvePath(char* path)` | Resuelve path absoluto o relativo a `(sectorDelDirectorioPadre, basename)`; usa hand-over-hand locking con RWLock por directorio. |
| `fsLock` | `Lock*` (campo público) | Lock global para acceder al freemap (accedido externamente por `FileHeader::Extend`). |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `GetDirLock(int sector)` | Crea lazy (bajo `lockDirArr`) y retorna el `RWLock*` para el directorio en `sector` |
| `GetCurrentDirSector()` | Retorna `currentThread->space->currentDirSector` si hay proceso usuario, o `kernelDirSector` si es un thread kernel |
| `SetCurrentDirSector(int sector)` | Actualiza el directorio de trabajo del thread actual |
| `parse(char* buff, char* div)` (static) | Tokeniza un path usando `strtok`; retorna `vector<char*>` — usa STL localmente |
| `AddToShadowBitmap`, `CheckForError`, `CheckSector`, `CheckFileHeader`, `CheckBitmaps`, `CheckDirectory` | Helpers estáticos para `Check()` |

## Dependencias

- **Incluye:** `file_system.hh`, `directory.hh`, `file_header.hh`, `lib/bitmap.hh`, `system.hh`, `<vector>`, `<utility>`, `threads/rwlock.hh`
- **Es incluido por:** `threads/system.cc` (init), `exception.cc` (syscalls de filesystem)

## Notas específicas de implementación

- **Protocolo de locking en `Create` / `Remove`:**
  1. `ResolvePath` adquiere y libera RWLocks de cada componente del path (read) de forma hand-over-hand.
  2. La operación sobre el directorio padre adquiere su RWLock en **escritura**.
  3. Operaciones sobre el freemap se hacen bajo `fsLock` (Lock simple) para serializar
     allocations entre múltiples operaciones concurrentes.

- **`ResolvePath` y hand-over-hand locking:** para cada componente intermedio del path:
  - Adquiere `RWLock(newSector)->AcquireRead()`
  - Luego libera `RWLock(currentSector)->ReleaseRead()`
  - Avanza a `currentSector = newSector`
  
  Esto garantiza que nadie renombra o elimina un directorio intermedio mientras se
  traversa el path.

- **`FILESYS_STUB`:** versión mínima en el `.hh`: `Create` llama `SystemDep::OpenForWrite`,
  `Open` llama `SystemDep::OpenForReadWrite`, `Remove` llama `SystemDep::Unlink`. No hay
  disco simulado, freemap ni directorios — el host Linux hace todo.

- **Uso de STL:** `file_system.cc` incluye `<vector>` y `<utility>` (para `pair` y
  `vector<char*>` en `parse` y `ResolvePath`). Es el único archivo del kernel que usa
  STL, y solo lo hace en la implementación real (`#ifdef FILESYS`). Notablemente, esto
  contrasta con el resto del kernel que evita STL explícitamente.

- **`kernelDirSector`:** variable global (en `system.hh`) para threads sin `AddressSpace`
  (threads kernel). Threads de usuario usan `space->currentDirSector`.

- **`CreateDir` y bug menor:** cuando `fatherDir->Add` falla (ya existe), setea
  `success = true` en lugar de `false` (línea 783). Probablemente un typo que no
  fue detectado porque el `Find` previo ya cubría el caso de nombre duplicado.
