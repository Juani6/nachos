# Nivel 3 — Referencia de Funciones: FileHeader

**Módulo:** [`filesys/file_header.cc`](../02-modulos/file_header.md)
**Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

---

### `bool FileHeader::Allocate(Bitmap *freeMap, int fileSize)`

**Ubicación:** `filesys/file_header.cc`

**Descripción:**
Alloca los sectores de disco necesarios para un archivo de `fileSize` bytes.
Calcula cuántos sectores lógicos se necesitan y llama `freeMap->Find()` para cada
uno. Si hay sectores indirectos necesarios (más de `NUM_DIRECT` bloques), alloca
también el sector de primer nivel de indirección.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `freeMap` | `Bitmap*` | Mapa de sectores libres del disco |
| `fileSize` | `int` | Tamaño en bytes del archivo a crear |

**Retorno:** `true` si se pudieron allocar todos los sectores; `false` si el disco
está lleno.

**Efectos secundarios:**
- Setea `raw.numBytes = fileSize`, `raw.numSectors = DIVUP(fileSize, SECTOR_SIZE)`.
- Llena `raw.dataSectors[0..NUM_DIRECT-1]` con sectores directos.
- Si `numSectors > NUM_DIRECT`: alloca `raw.indirection` como sector de primer nivel;
  escribe los punteros de segundo nivel en los sectores intermedios.

**Precondiciones / contexto de llamada:**
- `freeMap` debe estar bajo `fsLock` cuando se llama.
- No llamar dos veces sobre el mismo FileHeader sin `Deallocate` previo.

**Ver también:** `FileHeader::Deallocate`, `FileHeader::Extend`, `FileSystem::Create`

---

### `void FileHeader::Deallocate(Bitmap *freeMap)`

**Ubicación:** `filesys/file_header.cc`

**Descripción:**
Libera todos los sectores de datos del archivo en el freemap. Recorre los sectores
directos y, si hay indirección, también lee y libera los sectores intermedios
(de primer y segundo nivel).

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `freeMap` | `Bitmap*` | Mapa de sectores a actualizar |

**Efectos secundarios:**
- Para cada sector lógico: `freeMap->Clear(sector)`.
- Si había `raw.indirection`: libera también los sectores de punteros.
- No libera el sector del FileHeader mismo — eso lo hace `FileSystem::DeletePhysicalSector`.

**Precondiciones / contexto de llamada:**
- Llamado desde `FileSystem::DeletePhysicalSector` bajo `fsLock`.

**Ver también:** `FileHeader::Allocate`, `FileSystem::DeletePhysicalSector`

---

### `int FileHeader::ByteToSector(int offset)`

**Ubicación:** `filesys/file_header.cc`

**Descripción:**
Traduce un byte offset dentro del archivo a un número de sector físico de disco.
Maneja la doble indirección: si el bloque lógico supera `NUM_DIRECT`, lee el
sector de primer nivel, luego el de segundo nivel para obtener el sector final.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `offset` | `int` | Byte offset dentro del archivo (0-indexed) |

**Retorno:** `int` — número de sector de disco que contiene el byte en `offset`.

**Efectos secundarios:**
- Sin efectos de escritura.
- Con indirección: hace hasta 2 `ReadSector` para resolver el puntero.
  - `logicalBlock = offset / SECTOR_SIZE`
  - Si `logicalBlock < NUM_DIRECT`: retorna `raw.dataSectors[logicalBlock]`.
  - Si no: `indirNum = (logicalBlock - NUM_DIRECT) / NUMBER_POINTERS`;
    `indirOff = (logicalBlock - NUM_DIRECT) % NUMBER_POINTERS`;
    Lee `raw.indirection` → sector de primer nivel → sector de datos.

**Precondiciones / contexto de llamada:**
- `offset` debe ser < `raw.numBytes`.
- El FileHeader debe haber sido cargado desde disco con `FetchFrom`.

**Ver también:** `OpenFile::ReadAt`, `OpenFile::WriteAt`, `GetIndirectionSector`

---

### `bool FileHeader::Extend(OpenFile *freeMapFile, int newSize)`

**Ubicación:** `filesys/file_header.cc`

**Descripción:**
Extiende el archivo para que soporte hasta `newSize` bytes. Alloca sectores
adicionales en el freemap para los bloques lógicos que no tenían sector asignado.
Si el nuevo tamaño requiere doble indirección donde antes no había, alloca también
el sector de primer nivel.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `freeMapFile` | `OpenFile*` | Archivo del freemap (para leer/escribir el bitmap) |
| `newSize` | `int` | Nuevo tamaño total del archivo en bytes |

**Retorno:** `true` si la extensión fue exitosa; `false` si el disco no tiene
sectores suficientes.

**Efectos secundarios:**
- Lee el freemap desde `freeMapFile`.
- Itera desde `oldNumSectors` hasta `newNumSectors - 1`, allocando sectores nuevos.
- Actualiza `raw.numBytes = newSize`, `raw.numSectors = newNumSectors`.
- Escribe el FileHeader actualizado en disco con `WriteBack`.
- Escribe el freemap actualizado en disco.

**Precondiciones / contexto de llamada:**
- Llamado desde `OpenFile::WriteAt` cuando `position + numBytes > fileLength`.
- Bajo `iNodeLock` del archivo (garantizado por `WriteAt`).
- Bajo `fsLock` para la operación sobre el freemap.

**Ver también:** `OpenFile::WriteAt`, `FileHeader::Allocate`, `FileSystem::GetFreeMapFile`

---

### `int FileHeader::GetIndirectionSector(int indirNum, int indirOff)`

**Ubicación:** `filesys/file_header.cc`

**Descripción:**
Resuelve un bloque lógico con doble indirección. Dado el índice de primer nivel
`indirNum` y el offset dentro de ese índice `indirOff`, lee el sector de primer
nivel desde `raw.indirection`, luego lee el sector de segundo nivel y retorna el
sector físico final.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `indirNum` | `int` | Índice en el array de primer nivel de indirección |
| `indirOff` | `int` | Offset dentro del sector de punteros de segundo nivel |

**Retorno:** `int` — sector de disco para el bloque solicitado.

**Efectos secundarios:**
- Hace 2 llamadas a `synchDisk->ReadSector`:
  1. Lee `raw.indirection` → obtiene puntero de primer nivel.
  2. Lee el sector del primer nivel en posición `indirNum` → puntero de segundo nivel.
  3. Retorna el valor en posición `indirOff`.

**Precondiciones / contexto de llamada:**
- `raw.indirection` debe estar allocado (i.e., el archivo tiene más de `NUM_DIRECT` bloques).
- Llamado desde `ByteToSector`.

**Ver también:** `ByteToSector`, `FileHeader::Allocate`
