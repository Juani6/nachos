# Nivel 2 — Módulo: executable.cc / executable.hh

## Archivo
- **Ruta:** `userprog/executable.cc`, `userprog/executable.hh`
- **Subsistema:** [Gestión de Procesos y Syscalls](../01-subsistemas/procesos.md)

## Propósito del archivo
Parsea el header de un ejecutable en formato NOFF (Nachos Object File Format) y
provee acceso a los segmentos de código e inicialización de datos. Abstrae la
conversión de endianness (el ejecutable MIPS es little-endian; el host puede ser
big-endian). Usado exclusivamente por `AddressSpace`.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `Executable` | `Executable(OpenFile*)` | Lee el header NOFF del inicio del archivo |
| `CheckMagic` | `bool CheckMagic()` | Verifica `NOFF_MAGIC`; hace byte-swap del header si el magic está invertido (endianness) |
| `GetSize` | `uint32_t GetSize() const` | `code.size + initData.size + uninitData.size` |
| `GetCodeSize` | `uint32_t GetCodeSize() const` | Tamaño del segmento de código |
| `GetInitDataSize` | `uint32_t GetInitDataSize() const` | Tamaño del segmento de datos inicializados |
| `GetUninitDataSize` | `uint32_t GetUninitDataSize() const` | Tamaño del BSS (datos no inicializados) |
| `GetCodeAddr` | `uint32_t GetCodeAddr() const` | Dirección virtual de inicio del segmento de código |
| `GetInitDataAddr` | `uint32_t GetInitDataAddr() const` | Dirección virtual de inicio del segmento de datos inicializados |
| `ReadCodeBlock` | `int ReadCodeBlock(char* dest, uint32_t size, uint32_t offset)` | Lee `size` bytes del segmento de código desde `offset`; `ASSERT(offset < code.size)` |
| `ReadDataBlock` | `int ReadDataBlock(char* dest, uint32_t size, uint32_t offset)` | Lee `size` bytes del segmento de datos desde `offset`; `ASSERT(offset < initData.size)` |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `SwapHeader(noffHeader*)` (static) | Convierte todos los campos del header de little a big endian usando `WordToHost()` |

## Dependencias

- **Incluye:** `executable.hh`, `machine/endianness.hh`
- **Es incluido por:** `address_space.cc`

## Notas específicas de implementación

- **Formato NOFF:** cabecera de tipo `noffHeader` con tres segmentos:
  `code`, `initData`, `uninitData`, cada uno con `(size, virtualAddr, inFileAddr)`.
  La cabecera se lee en el constructor con `file->ReadAt(&header, sizeof header, 0)`.

- **Detección de endianness:** `CheckMagic` verifica si `header.noffMagic == NOFF_MAGIC`
  directamente; si no, intenta `WordToHost(header.noffMagic) == NOFF_MAGIC` y si coincide,
  invoca `SwapHeader`. Esta lógica permite ejecutar binarios generados en cualquier
  endianness sobre un host de endianness opuesto.

- **`ReadCodeBlock` / `ReadDataBlock`:** delegan en `file->ReadAt(dest, size, inFileAddr + offset)`. El offset es dentro del segmento (no del archivo); `inFileAddr` lo convierte a offset de archivo.

- **Dato unitData (BSS):** el segmento `uninitData` no se lee del archivo (el SO lo
  inicializa a cero con `memset` en `InitPageTableNaive` o en `LoadPage`); solo su
  `size` se usa para calcular `numPages`.
