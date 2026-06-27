# Nivel 1 — Subsistema: Filesystem

## Propósito
Proveer un sistema de archivos persistente en disco simulado con soporte para:
archivos de tamaño variable con bloques directos e indirección doble, directorios
jerárquicos con paths absolutos y relativos, directorio de trabajo por proceso
(`currentDirSector`), tabla global de archivos abiertos con locking por inodo, y
`RWLock` por directorio para concurrencia segura entre múltiples threads.

Existe también una implementación stub (`FILESYS_STUB`) que delega directamente al
filesystem del host Linux, usada durante la fase de userprog antes de tener el
filesystem completo.

## Archivos involucrados

| Archivo | Lenguaje | Rol |
|---------|----------|-----|
| `filesys/file_system.cc / .hh` | C++ | `FileSystem`: formateo, `Create`/`Open`/`Remove`/`CreateDir`/`ChangeDir`/`List`/`ResolvePath` |
| `filesys/file_header.cc / .hh` | C++ | `FileHeader` (inode): `Allocate`, `ByteToSector`, `Extend`, bloques directos + indirección doble |
| `filesys/raw_file_header.hh` | C++ | `RawFileHeader`: representación on-disk del inode (dataSectors[], indirection) |
| `filesys/directory.cc / .hh` | C++ | `Directory`: entradas nombre→sector, `Find`/`Add`/`Remove`/`List`, resize dinámico |
| `filesys/directory_entry.hh` | C++ | `DirectoryEntry`: nombre + sector del FileHeader |
| `filesys/raw_directory.hh` | C++ | Representación on-disk del directorio |
| `filesys/open_file.cc / .hh` | C++ | `OpenFile`: Read/Write con seek, `ReadAt`/`WriteAt` |
| `filesys/file_table.cc / .hh` | C++ | `FileTable`: tabla global de inodos abiertos (MAX_OPEN_FILES=32), locking por inodo |
| `filesys/synch_disk.cc / .hh` | C++ | `SynchDisk`: acceso sincrónico al disco (Semaphore sobre callbacks del disco) |
| `machine/disk.cc / .hh` | C++ | `Disk`: disco simulado con latencia, sectores de SECTOR_SIZE bytes |
| `filesys/fs_test.cc` | C++ | Tests básicos (Create/Open/Read/Write/Remove) |
| `filesys/fs_conc_test.cc` | C++ | Tests de concurrencia (múltiples threads sobre el mismo archivo) |
| `filesys/fs_rw_test.cc` | C++ | Tests de lectura/escritura concurrente |

## Flujo de control

### Formateo inicial (`FileSystem(format=true)`)
```
new FileSystem(true)
    -> Bitmap freeMap(NUM_SECTORS)
    -> Directory dir(NUM_DIR_ENTRIES)
    -> FileHeader mapH, dirH
    -> mapH.Allocate(freeMap, FREE_MAP_FILE_SIZE) -> sector 0
    -> dirH.Allocate(freeMap, DIRECTORY_FILE_SIZE) -> sector 1
    -> Escribe freeMap y dir a disco
    -> Deja freeMapFile y directoryFile abiertos permanentemente
    -> Inicializa dirLocks[NUM_SECTORS] = nullptr (lazy)
```

### Creación de archivo (`FileSystem::Create`)
```
Create(name, size)
    -> ResolvePath(name) -> (parentDirSector, basename)
    -> GetDirLock(parentDirSector)->AcquireWrite()
    -> Lee Directory del sector padre
    -> dir.Find(basename) debe ser -1 (no existe)
    -> FileHeader::Allocate(freeMap, size)  -> sectores asignados
    -> dir.Add(basename, headerSector)
    -> Escribe freeMap, dir y FileHeader a disco
    -> GetDirLock(parentDirSector)->ReleaseWrite()
```

### Apertura de archivo (`FileSystem::Open`)
```
Open(name)
    -> ResolvePath(name) -> (parentDirSector, basename)
    -> GetDirLock(parentDirSector)->AcquireRead()
    -> dir.Find(basename) -> sector del FileHeader (inode)
    -> GetDirLock(parentDirSector)->ReleaseRead()
    -> fileTable->Acquire(inodeSector)   // lock por inodo, count++
    -> new OpenFile(inodeSector)
```

### Resolución de paths (`FileSystem::ResolvePath`)
```
ResolvePath(path)
    -> si path[0]=='/' : empieza desde DIRECTORY_SECTOR (raíz)
       si no           : empieza desde currentThread->space->currentDirSector
    -> para cada componente del path:
         GetDirLock(currentSector)->AcquireRead()   // hand-over-hand locking
         dir = leer Directory de currentSector
         nextSector = dir.Find(componente)
         GetDirLock(currentSector)->ReleaseRead()
         currentSector = nextSector
    -> retorna (parentSector, basename)
```

### Extensión de archivo (`FileHeader::Extend`)
```
Extend(newSize)
    -> si newSize <= MAX_DIRECT_SIZE: usa dataSectors[] directos
    -> si newSize > MAX_DIRECT_SIZE:
         asigna sector de indirección doble (raw.indirection)
         ese sector contiene punteros a sectores de datos adicionales
    -> actualiza raw.numBytes, raw.numSectors
    -> escribe FileHeader a disco
```

### Concurrencia en `Open` / `Close` (FileTable)
```
fileTable->Acquire(inodeSector)
    -> si ya existe entrada con ese sector: count++, devuelve entrada
    -> si no: crea nueva entrada con count=1, nuevo Lock iNodeLock
fileTable->Release(entry)
    -> count--
    -> si count==0 y deletePending: elimina físicamente el archivo
```

## Estructuras de datos clave

| Estructura | Definida en | Descripción |
|------------|-------------|-------------|
| `RawFileHeader` | `filesys/raw_file_header.hh` | numBytes, numSectors, dataSectors[NUM_DIRECT], indirection (sector) |
| `FileHeader` | `filesys/file_header.hh` | Wrapper de RawFileHeader; ByteToSector resuelve bloques directos e indirectos |
| `DirectoryEntry` | `filesys/directory_entry.hh` | name[FILE_NAME_MAX_LEN], sector (int) |
| `FileTableEntry` | `filesys/file_table.hh` | inodeSector, count, iNodeLock, inUse, deletePending |
| `FileTable` | `filesys/file_table.hh` | Array de MAX_OPEN_FILES=32 FileTableEntry + internalLock |
| `dirLocks[NUM_SECTORS]` | `filesys/file_system.hh` | `RWLock*` por sector de directorio (lazy init bajo lockDirArr) |
| `freeMapFile` | `filesys/file_system.hh` | `OpenFile*` del bitmap de sectores libres (siempre abierto) |
| `directoryFile` | `filesys/file_system.hh` | `OpenFile*` del directorio raíz (siempre abierto) |
| `currentDirSector` | `userprog/address_space.hh` | Sector del directorio de trabajo por proceso; se hereda en `Exec` |

## Dependencias
- **Depende de:** `machine/disk` + `filesys/synch_disk` (I/O a disco), `threads/lock`
  y `threads/rwlock` (sincronización), `lib/bitmap` (freemap)
- **Es usado por:** `userprog/exception` (syscalls de filesystem), `userprog/address_space`
  (swapFile, carga del ejecutable), `threads/system` (inicialización de `fileSystem` y `synchDisk`)

## Invariantes / precondiciones importantes
- El sector 0 siempre contiene el FileHeader del freemap; el sector 1 siempre contiene
  el FileHeader del directorio raíz.
- `freeMapFile` y `directoryFile` se mantienen abiertos durante toda la ejecución —
  no deben cerrarse.
- `ResolvePath` usa hand-over-hand locking: adquiere el lock del directorio hijo antes
  de soltar el del padre, evitando condiciones de carrera en estructuras en árbol.
- `FileTable::Release` borra físicamente el archivo solo cuando count==0 y deletePending
  (semantica UNIX: el archivo existe hasta que todos los descriptores se cierran).
- Los `dirLocks` se crean lazy bajo `lockDirArr` para evitar allocar NUM_SECTORS locks
  innecesarios al arranque.

## Notas de diseño
- `RawFileHeader.indirection` es un puntero a un sector que contiene `NUMBER_POINTERS`
  punteros a sectores de datos (`NUMBER_POINTERS = SECTOR_SIZE / sizeof(int)`), lo que
  permite archivos de hasta `MAX_DIRECT_SIZE + INDIRECTION_SIZE` bytes (doble indirección
  efectiva: el sector de indirección apunta a sectores de datos, no a bloques de punteros).
  <!-- Nota: INDIRECTION_SIZE = NUMBER_POINTERS² × SECTOR_SIZE sugiere doble indirección real; verificar contra la implementación de ByteToSector. -->
- El `fsLock` global existe pero se usa de forma selectiva; el locking fino es por
  directorio (dirLocks) y por inodo (FileTableEntry.iNodeLock).
- La eliminación diferida (`deletePending`) permite que un archivo que está siendo leído
  pueda ser `Remove`d sin interrumpir a los lectores activos.
- Los directorios son archivos regulares cuyo contenido es un array de `DirectoryEntry`;
  pueden crecer dinámicamente (via `Extend`).

## Limitaciones conocidas / TODOs
- No hay journaling: si NachOS termina en medio de una operación, el disco puede quedar
  inconsistente.
- `NUM_DIR_ENTRIES` inicial es 10; crecer el directorio raíz requiere que `Extend` esté
  implementado y activo.
- No hay soporte de links simbólicos ni hard links.
- El tamaño máximo de archivo está limitado por `MAX_FILE_SIZE` (función de SECTOR_SIZE).
