# Nivel 3 — Referencia de Funciones: OpenFile

**Módulo:** [`filesys/open_file.cc`](../02-modulos/open_file.md)
**Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

Las dos implementaciones (`FILESYS_STUB` y `FILESYS`) se documentan juntas,
indicando cuándo aplica cada una.

---

### `int OpenFile::ReadAt(char *into, int numBytes, int position)`

**Ubicación:** `filesys/open_file.cc`

**Descripción:**
Lee `numBytes` bytes desde el offset `position` del archivo hacia el buffer `into`.
No usa ni modifica el cursor interno del archivo.

**Implementación `FILESYS`:**
- Bajo `iNodeLock->AcquireRead()`.
- Clipa `numBytes` si `position + numBytes > fileLength`.
- Divide el rango en sectores usando `hdr->ByteToSector`, lee cada sector con
  `synchDisk->ReadSector` y copia la porción relevante.

**Implementación `FILESYS_STUB`:**
- Llama `SystemDep::Read(fd, into, numBytes)` con el descriptor del host.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `into` | `char*` | Buffer destino |
| `numBytes` | `int` | Bytes a leer |
| `position` | `int` | Offset dentro del archivo |

**Retorno:** número de bytes efectivamente leídos (puede ser menor si `position+numBytes > fileLength`).

**Efectos secundarios (`FILESYS`):**
- Adquiere / libera `iNodeLock` en modo lectura.
- Llamadas a `synchDisk->ReadSector` (bloqueantes, serializadas por `SynchDisk`).

**Ver también:** `OpenFile::Read`, `FileHeader::ByteToSector`, `SynchDisk::ReadSector`

---

### `int OpenFile::WriteAt(char *from, int numBytes, int position)`

**Ubicación:** `filesys/open_file.cc`

**Descripción:**
Escribe `numBytes` bytes desde `from` al archivo en el offset `position`.
Si `position + numBytes` excede el tamaño actual, extiende el archivo llamando
`hdr->Extend`.

**Implementación `FILESYS`:**
- Bajo `iNodeLock->AcquireWrite()`.
- Si `position + numBytes > fileLength`: `hdr->Extend(freeMapFile, newSize)`.
- Escribe cada sector afectado (read-modify-write para sectores parciales).

**Implementación `FILESYS_STUB`:**
- Llama `SystemDep::Write(fd, from, numBytes)`.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `from` | `char*` | Buffer fuente |
| `numBytes` | `int` | Bytes a escribir |
| `position` | `int` | Offset dentro del archivo |

**Retorno:** número de bytes escritos.

**Efectos secundarios (`FILESYS`):**
- Adquiere / libera `iNodeLock` en modo escritura.
- Puede extender el FileHeader y actualizar el freemap (bajo `fsLock`).
- Puede hacer llamadas a `synchDisk->ReadSector` (sectores parciales) y `WriteSector`.

**Precondiciones / contexto de llamada:**
- Para extensión: `fileSystem->fsLock` debe ser tomable; no debe estar tomado ya por el mismo thread.

**Ver también:** `FileHeader::Extend`, `OpenFile::ReadAt`

---

### `int OpenFile::Read(char *into, int numBytes)` / `int OpenFile::Write(char *from, int numBytes)`

**Ubicación:** `filesys/open_file.cc`

**Descripción:**
Variantes con cursor. Leen/escriben `numBytes` bytes desde/hacia `seekPosition`
y avanzan el cursor. Implementadas como envoltorios de `ReadAt` / `WriteAt`.

**Efectos secundarios:**
- Avanza `seekPosition += numBytes_efectivos`.

**Ver también:** `ReadAt`, `WriteAt`

---

### `OpenFile::~OpenFile()`

**Ubicación:** `filesys/open_file.cc`

**Descripción:**
Destructor. Llama `fileTable->Release(sector)`:
- Si retorna `true` (refcount llegó a 0 y `deletePending==true`): llama
  `fileSystem->DeletePhysicalSector(sector)` para liberar el inode y los datos del disco.
- Si retorna `false`: solo cierra el handle; otros procesos aún lo tienen abierto.

**Efectos secundarios (`FILESYS`):**
- Puede liberar sectores de disco si era el último handle de un archivo eliminado.
- Decrementa refcount en `fileTable`.

**Implementación `FILESYS_STUB`:**
- Cierra el descriptor de host con `SystemDep::Close(fd)`.

**Ver también:** `FileTable::Release`, `FileSystem::DeletePhysicalSector`
