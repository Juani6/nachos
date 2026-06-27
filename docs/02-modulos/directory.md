# Nivel 2 — Módulo: directory.cc / directory.hh / directory_entry.hh / raw_directory.hh

## Archivo
- **Ruta:** `filesys/directory.cc`, `filesys/directory.hh`, `filesys/directory_entry.hh`, `filesys/raw_directory.hh`
- **Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

## Propósito del archivo
Implementa la abstracción de directorio: una tabla de entradas `(nombre, sector, isDirectory)`.
Permite buscar, agregar y eliminar entradas. La tabla puede crecer dinámicamente al
duplicar su tamaño cuando se llena (extensión dinámica). Cada directorio es un archivo
regular cuyo contenido es el array de `DirectoryEntry`.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Directory` | `Directory(unsigned size)` | Aloca `size` entradas, todas marcadas `inUse=false` |
| `~Directory` | `~Directory()` | Libera el array de entradas |
| `FetchFrom` | `void FetchFrom(OpenFile*)` | Lee las entradas desde el archivo de directorio; expande la tabla en memoria si el archivo creció |
| `WriteBack` | `void WriteBack(OpenFile*)` | Escribe la tabla de entradas al archivo de directorio |
| `Find` | `int Find(const char* name)` | Retorna sector del FileHeader de `name`, o -1 si no existe |
| `Add` | `bool Add(const char* name, int sector, bool isDir=false)` | Agrega entrada; si la tabla está llena, la duplica en tamaño (realloc manual); retorna false si el nombre ya existe |
| `Remove` | `bool Remove(const char* name)` | Marca la entrada como `inUse=false`; retorna false si no existe |
| `List` | `void List() const` | Imprime todos los nombres; prefija `./` a subdirectorios |
| `Print` | `void Print() const` | Debug: imprime entradas y contenido de cada FileHeader |
| `GetRaw` | `const RawDirectory* GetRaw() const` | Acceso de bajo nivel a la estructura |
| `FindEntry` | `DirectoryEntry* FindEntry(char* name)` | Retorna puntero a la entrada (para acceso directo a `isDirectory`); retorna nullptr si no existe |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `FindIndex(const char* name)` | Busca el índice de `name` en la tabla; retorna -1 si no está |

## Dependencias

- **Incluye:** `directory.hh`, `directory_entry.hh`, `file_header.hh`, `lib/utility.hh`, `threads/system.hh`
- **Es incluido por:** `file_system.cc`

## Notas específicas de implementación

- **`DirectoryEntry` layout:**
  ```cpp
  bool    inUse;
  bool    isDirectory;
  char    name[FILE_NAME_MAX_LEN + 1];
  int     sector;   // sector del FileHeader del archivo/directorio
  ```
  `FILE_NAME_MAX_LEN = 9` (definido en `process_data.h`), lo que limita los nombres
  a 9 caracteres.

- **Crecimiento dinámico en `Add`:** si todos los slots están ocupados, `Add` duplica
  el array (`newSize = tableSize * 2`), copia los datos con `memcpy`, inicializa los
  nuevos slots a `inUse=false`, y reemplaza `raw.table`. Este cambio en memoria debe
  reflejarse en disco con `WriteBack` para persistir; si el archivo subyacente no
  tiene espacio, `OpenFile::WriteAt` llama a `FileHeader::Extend` automáticamente.

- **`FetchFrom` con archivo crecido:** si `entriesOnDisk > raw.tableSize`, re-aloca
  la tabla en memoria para que coincida con el tamaño del archivo en disco. Esto
  garantiza que `FetchFrom` siempre lee todas las entradas actuales.

- **Comparación de nombres:** usa `strncmp` con `FILE_NAME_MAX_LEN` bytes. Los nombres
  no se null-terminan necesariamente dentro de los primeros `FILE_NAME_MAX_LEN` chars
  si tienen exactamente esa longitud.

- **`isDirectory` flag:** `List` usa este flag para diferenciar archivos de
  subdirectorios en la salida. `FileSystem::ResolvePath` lo usa para verificar que
  los componentes intermedios de un path sean directorios.
