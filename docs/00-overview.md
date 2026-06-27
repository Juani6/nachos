# Nivel 0 — Visión General del Sistema Operativo

## 1. Identidad del proyecto
- **Nombre del SO:** NachOS (Not Another Completely Heuristic Operating System)
- **Objetivo / motivación:** Educativo — Serie de trabajos prácticos ("planchas") de la
  materia Sistemas Operativos II de la Licenciatura en Ciencias de la Computación de la 
  Universidad Nacional de Rosario.
- **Alcance actual:**
  - ✅ Multiprogramación y syscalls de proceso (`Exec`, `Exit`, `Join`)
  - ✅ Consola sincrónica (`SynchConsole`), tabla de descriptores de archivo
  - ✅ Syscalls de filesystem (`Create`, `Open`, `Close`, `Read`, `Write`, `Remove`)
  - ✅ Memoria virtual: demand paging, TLB, swap, reemplazo FIFO y CLOCK
  - ✅ Filesystem extensible: archivos de tamaño variable, doble indirección
  - ✅ Filesystem jerárquico: directorios, `CreateDir`, `ChangeDir`, `List`,
    `currentDirSector` por proceso
  - ✅ Concurrencia en filesystem: tabla de archivos abiertos con locking por inodo,
    `RWLock` por directorio, hand-over-hand locking en `ResolvePath`
  - 🔲 Sin planchas pendientes — implementación completa

## 2. Arquitectura objetivo
- **Tipo de kernel:** Monolítico simulado — NachOS corre como proceso de usuario Linux;
  el "kernel" es una biblioteca C++ que simula hardware y modo privilegiado.
- **Arquitectura de CPU:** MIPS (simulada por software mediante el simulador de NachOS).
  El host es x86_64 Debian Linux.
- **Modo de boot:** No aplica en sentido tradicional. El binario de NachOS arranca
  directamente como proceso Linux; la inicialización del sistema se hace en `main()`.
- **Modos de CPU usados:** Modo usuario simulado (MIPS) / modo kernel (código C++ nativo
  del host). No hay ring transitions reales; se emulan mediante `Machine::Run()` y
  excepciones/interrupciones simuladas.

## 3. Diagrama de flujo general
```
[main() en C++]
    |
    v
[Initialize() — subsistemas: Machine, Scheduler, FileSystem, SynchConsole]
    |
    v
[StartProcess() — carga el programa MIPS inicial en AddressSpace]
    |
    v
[Machine::Run() — loop de fetch/decode/execute del simulador MIPS]
    |
    +--[Excepción MIPS]--> [ExceptionHandler()]
    |                           |
    |                     [SyscallHandler() / PageFaultHandler()]
    |                           |
    |                     [regresa a Machine::Run()]
    |
    +--[Interrupción simulada]--> [Scheduler::Run() / Timer handler]
```

## 4. Mapa de memoria (memory layout)
> Nota: "memoria" aquí es la RAM simulada de la máquina MIPS, no la RAM del host.

| Región                  | Dirección inicio (MIPS) | Dirección fin (MIPS) | Descripción                                         |
|-------------------------|-------------------------|----------------------|-----------------------------------------------------|
| Segmento de código/data | 0x0                     | depende del ELF      | Cargado desde el ejecutable MIPS por `AddressSpace` |
| Stack de usuario        | tope del espacio        | ← crece hacia abajo  | Asignado en `AddressSpace::AddressSpace()`          |
| Páginas en swap         | —                       | —                    | Archivo swap en filesystem NachOS; mapeado via `shadowTable` |
| CoreMap (frames físicos)| —                       | — (`NUM_PHYS_PAGES`) | Tabla global de frames; gestión por FIFO o CLOCK    |

> El host (Debian x86_64) maneja su propio layout; NachOS vive enteramente en heap del proceso Linux.

## 5. Toolchain y entorno
- **Compilador:** `g++` del host (Debian, x86_64) para el kernel/simulador;
  `mipsel-linux-gnu-gcc` (GNU, little-endian MIPS) para los programas de usuario
- **Assembler:** `mipsel-linux-gnu-as` con flags `-mips1`
- **Linker script:** `userland/arrangement.ld`, invocado con `-T arrangement.ld -N -s`;
  el ejecutable resultante pasa por `bin/coff2noff` para convertir COFF → NOFF (formato
  nativo de NachOS)
- **Emulador/testing:** no hay emulador externo — NachOS **es** el simulador.
  Se corre directo en Linux.
- **Build system:** `Makefile` por directorio (uno por build target: `threads/`, `userprog/`,
  `vmem/`, `filesys/`)
- **Formato de código:** `clang-format` con estilo Linux kernel (tabs de 8, 80 columnas)
- **Comando para compilar y correr:**
  ```bash
  # Dentro del directorio del target (ej: filesys/)
  make

  # Correr el test de jerarquía
  ./nachos -th

  # Correr con Valgrind
  valgrind --leak-check=full ./nachos -th
  ```

## 6. Estructura de carpetas del repo
```
/threads    -> scheduler, locks, semaphores, threads (build target base)
/userprog   -> AddressSpace, syscall handlers, tabla de procesos, SynchConsole
/vmem       -> demand paging, TLB, CoreMap, swap, reemplazo de páginas
/filesys    -> FileSystem, FileHeader, Directory, OpenFile, FileTable, RWLock
/machine    -> simulador MIPS: Machine, Translate, Interrupt, Timer
/bin        -> herramientas: coff2noff (convierte COFF MIPS a formato NOFF de NachOS)
/userland   -> programas de usuario MIPS (shell.c, halt.c, matmult.c, etc.) y arrangement.ld
/network    -> [?] capa de red (no trabajada en estas planchas)
```

## 7. Estado actual / roadmap corto
- [x] Boot / inicialización del simulador
- [x] Scheduler con threads y semáforos/locks
- [x] Interrupciones simuladas manejadas
- [x] Multiprogramación (`Exec`/`Exit`/`Join`, `processTable`, `memoryMap`)
- [x] Syscalls de I/O y filesystem básico
- [x] Memoria virtual: demand paging, TLB miss, FIFO y CLOCK
- [x] Swap con `shadowTable` por proceso
- [x] Filesystem extensible (archivos grandes, doble indirección)
- [x] Filesystem jerárquico (directorios, paths absolutos/relativos)
- [x] Concurrencia en filesystem (RWLock por directorio, locking por inodo)
