# Nivel 2 — Módulo: open_file.cc / open_file.hh

## Archivo
- **Ruta:** `filesys/open_file.cc`, `filesys/open_file.hh`
- **Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

## Propósito del archivo
Representa un archivo abierto en NachOS. Mantiene el `FileHeader` (inode) en memoria
y un `seekPosition` para `Read`/`Write` secuenciales. Tiene dos implementaciones
seleccionadas por `#ifdef`:
- **`FILESYS_STUB`:** delega a un file descriptor UNIX del host.
- **`FILESYS`:** usa `SynchDisk` + `FileHeader`; soporta extensión automática en
  `WriteAt` y aplica locking por inodo via `FileTableEntry`.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `OpenFile` | `OpenFile(int sector)` (stub) / `OpenFile(int sector, FileTableEntry*)` (real) | Lee FileHeader del sector; inicializa `seekPosition=0`; (real) guarda la entrada de FileTable |
| `~OpenFile` | `~OpenFile()` | Destruye FileHeader; (real) llama `fileTable->Release()` — si deletePending, borra el archivo físicamente |
| `Seek` | `void Seek(unsigned position)` | Mueve `seekPosition` |
| `Read` | `int Read(char* into, unsigned numBytes)` | Lee desde `seekPosition`; avanza posición; (real) bajo `iNodeLock` |
| `Write` | `int Write(const char* from, unsigned numBytes)` | Escribe desde `seekPosition`; avanza posición; (real) bajo `iNodeLock`; extiende el archivo si hace falta |
| `ReadAt` | `int ReadAt(char* into, unsigned numBytes, unsigned position)` | Lee sectores necesarios de disco, copia la parte relevante; sin side effects en `seekPosition` |
| `WriteAt` | `int WriteAt(const char* from, unsigned numBytes, unsigned position)` | (real) Extiende si `position+numBytes > fileLength`; lee sectores parciales, modifica, reescribe |
| `Length` | `unsigned Length() const` | Retorna `hdr->FileLength()` |

## Funciones/símbolos internos
_(ninguno — toda la lógica está en los métodos públicos)_

## Dependencias

- **Incluye:** `open_file.hh`, `file_header.hh`, `threads/system.hh`
- **Es incluido por:** `file_system.cc`, `address_space.cc`, `exception.cc` (via fdTable)

## Notas específicas de implementación

- **Lectura por sectores enteros:** `ReadAt` / `WriteAt` solo pueden operar sector a
  sector (el disco simulado no soporta lecturas parciales). Para escrituras no alineadas
  a sector, primero se leen los sectores afectados, se parchan en un buffer temporal,
  y se reescriben completos.

- **Extensión automática en `WriteAt` (FILESYS):** si `position + numBytes > fileLength`,
  llama `hdr->Extend(numBytes + position)` y luego `hdr->WriteBack(hdrSector)` para
  persistir el nuevo tamaño del inode. El archivo puede crecer on-the-fly.

- **Locking por inode en `Read`/`Write` (FILESYS):** adquiere `tableEntry->iNodeLock`
  antes de `ReadAt`/`WriteAt`, garantizando que dos threads no modifiquen el mismo
  archivo simultáneamente. `ReadAt`/`WriteAt` directos (sin seek) no adquieren el lock
  — son llamados ya sea con el lock tomado (`Write`) o desde contextos donde es seguro
  (carga de ejecutables).

- **Eliminación diferida en `~OpenFile` (FILESYS):** `fileTable->Release(tableEntry)`
  retorna `true` si `count` llegó a 0 y `deletePending` era `true`. En ese caso
  `~OpenFile` llama `fileSystem->DeletePhysicalSector(hdrSector)` para eliminar
  físicamente el inode y liberar los sectores de datos en el freemap.

- **`FILESYS_STUB`:** usa file descriptors UNIX (`SystemDep::OpenForReadWrite`). Los
  mismos métodos están implementados pero delegan al OS host. Útil para testear
  `userprog` sin tener el filesystem completo.
