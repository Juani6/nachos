# Nivel 2 — Módulo: file_header.cc / file_header.hh / raw_file_header.hh

## Archivo
- **Ruta:** `filesys/file_header.cc`, `filesys/file_header.hh`, `filesys/raw_file_header.hh`
- **Subsistema:** [Filesystem](../01-subsistemas/filesystem.md)

## Propósito del archivo
Implementa el inode del filesystem NachOS. Mapea offsets de archivo a sectores de disco
usando bloques directos y doble indirección. El `RawFileHeader` (representación on-disk)
ocupa exactamente un sector de disco. `FileHeader` es el wrapper en memoria que provee
la interfaz de alto nivel y la lógica de traducción y extensión.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Allocate` | `bool Allocate(Bitmap* freeMap, unsigned fileSize)` | Asigna sectores del freemap para el archivo; maneja directos e indirectos; retorna false si no hay espacio |
| `Deallocate` | `void Deallocate(Bitmap* freeMap)` | Libera todos los sectores del archivo (directos, bloques de punteros de 2do nivel, bloque raíz de indirección) |
| `FetchFrom` | `void FetchFrom(unsigned sector)` | Lee el `RawFileHeader` desde el disco (sector → `raw`) |
| `WriteBack` | `void WriteBack(unsigned sector)` | Escribe el `RawFileHeader` al disco |
| `ByteToSector` | `unsigned ByteToSector(unsigned offset)` | Traduce offset de byte → número de sector; resuelve bloque directo o doble indirección |
| `FileLength` | `unsigned FileLength() const` | Retorna `raw.numBytes` |
| `Extend` | `bool Extend(unsigned newSize)` | Extiende el archivo a `newSize` bytes; asigna nuevos sectores y actualiza el freemap |
| `Print` | `void Print(const char* title)` | Imprime header y contenido de todos los sectores (debug) |
| `GetRaw` | `const RawFileHeader* GetRaw() const` | Acceso de bajo nivel a la estructura on-disk |
| `GetIndirectionSector` | `unsigned GetIndirectionSector(unsigned logicalBlock)` | Resuelve un bloque lógico ≥ `NUM_DIRECT` a través de los dos niveles de indirección |

## Funciones/símbolos internos
_(ninguno — toda la lógica está en los métodos públicos; `raw` es el único campo privado)_

## Dependencias

- **Incluye:** `file_header.hh`, `threads/system.hh`
- **Es incluido por:** `file_system.cc`, `directory.cc`, `open_file.cc`

## Notas específicas de implementación

- **Estructura on-disk (`RawFileHeader`):**
  ```
  numBytes    (unsigned)
  numSectors  (unsigned)
  dataSectors[NUM_DIRECT]   (unsigned[])   — bloques directos
  indirection (unsigned)                   — sector del 1er nivel de indirección
  ```
  `NUM_DIRECT = (SECTOR_SIZE - 3*sizeof(int)) / sizeof(int)` — tantos punteros
  directos como caben en un sector dejando 3 int para los campos de cabecera.

- **Doble indirección:**
  - `raw.indirection` apunta a un sector que contiene `NUMBER_POINTERS` entradas
    (`NUMBER_POINTERS = SECTOR_SIZE / sizeof(int)`).
  - Cada entrada de ese sector apunta a otro sector que contiene `NUMBER_POINTERS`
    entradas de datos finales.
  - Tamaño máximo: `MAX_DIRECT_SIZE + NUMBER_POINTERS² × SECTOR_SIZE`.

- **`ByteToSector`:** si `logicalBlock < NUM_DIRECT` retorna `dataSectors[logicalBlock]`;
  si no, llama `GetIndirectionSector(logicalBlock)` que hace dos `ReadSector` para
  resolver los dos niveles.

- **`Extend`:** re-usa la misma lógica de asignación que `Allocate` pero itera solo
  sobre los sectores nuevos (`oldNumSectors` a `newNumSectors-1`). Crea el bloque raíz
  de indirección si no existía. Al final, actualiza el freemap en disco con
  `freeMap->WriteBack(fileSystem->GetFreeMapFile())`.

- **`Deallocate` y bloques de punteros:** libera los sectores de 2do nivel cuando se
  termina de procesar el último bloque que apuntaban (`indirectionOffset == NUMBER_POINTERS-1`
  o es el último sector del archivo). Al final libera el bloque raíz de indirección.

- **Un inode = un sector de disco:** la restricción de tamaño de `RawFileHeader` es
  intencional; garantiza que leer/escribir un inode es siempre una operación de un
  sector (`FetchFrom`/`WriteBack`).
