# Nivel 1 — Subsistema: Userland (Programas de Usuario MIPS)

## Propósito
Proveer los programas de usuario que corren sobre el simulador MIPS de NachOS: una
librería mínima de C (`lib.c`), stubs de syscalls en assembly (`start.s`), una shell
interactiva, comandos básicos de filesystem y utilidades de test. También incluye el
linker script (`arrangement.ld`) que define el layout de memoria de los ejecutables NOFF.

Todos estos programas se compilan para MIPS little-endian (`mipsel-linux-gnu-gcc`) y
se convierten a formato NOFF mediante `bin/coff2noff` antes de ejecutarse en NachOS.

## Archivos involucrados

| Archivo | Lenguaje | Rol |
|---------|----------|-----|
| `userland/start.s` | MIPS ASM | `__start`: entry point de todo ejecutable; llama `main`, luego `Exit(r2)` |
| `userland/arrangement.ld` | Linker script | Layout de memoria: secciones .text/.data/.bss, base address |
| `userland/lib.c / lib.h` | C | Libc mínima: `strlen`, `strCopy`, `itoa`, wrappers de syscalls, `printf` básico |
| `userland/shell.c` | C | Shell interactivo: parsea línea, `Exec` + `Join` de comandos |
| `userland/tinyshell.c` | C | Shell mínimo alternativo |
| `userland/halt.c` | C | Llama `Halt()` — para testing de apagado |
| `userland/cat.c` | C | Lee archivo y escribe a CONSOLE_OUTPUT |
| `userland/cp.c` | C | Copia un archivo a otro |
| `userland/ls.c` | C | Lista directorio actual (llama syscall `LS`) |
| `userland/mkdir.c` | C | Crea directorio (syscall `MKDIR`) |
| `userland/cd.c` | C | Cambia directorio (syscall `CD`) |
| `userland/rm.c` | C | Elimina archivo (syscall `Remove`) |
| `userland/touch.c` | C | Crea archivo vacío (syscall `Create`) |
| `userland/echo.c` | C | Escribe argumentos a CONSOLE_OUTPUT |
| `userland/ps.c` | C | Lista procesos (syscall `getPT`, imprime tabla de procesos) |
| `userland/matmult.c` | C | Multiplicación de matrices — test de CPU y memoria |
| `userland/sort.c` | C | Ordenamiento de array — test de memoria |
| `userland/fib.c` | C | Fibonacci recursivo — test de stack |
| `userland/filetest.c` | C | Test de Create/Open/Read/Write/Close |
| `userland/swapTest.c` | C | Test de presión de memoria para forzar swapping |
| `userland/roTest.c` | C | Test de acceso a páginas read-only (verifica ReadOnlyException) |
| `userland/exitTest.c` | C | Test de SC_EXIT con código de salida |
| `userland/libTest.c` | C | Tests de funciones de `lib.c` |

## Flujo de control

### Arranque de cualquier ejecutable MIPS
```
NachOS carga el NOFF via AddressSpace
    -> PC inicializado a 0x0 (address de __start)
Machine::Run() comienza fetch/decode/execute
    -> __start (start.s):
         jal main          // llama a main() del programa
         move $4, $2       // exit status = valor de retorno de main
         jal Exit          // syscall SC_EXIT
```

### Stubs de syscalls (start.s)
Cada syscall tiene un stub que:
1. Carga el número de syscall (`SC_*`) en `$r2` con `addiu $2, $0, SC_XXX`
2. Emite la instrucción `syscall` (trap al kernel NachOS)
3. Retorna con `j $31`

El valor de retorno queda en `$r2` (convención MIPS C).

Syscalls con stubs: `Halt`, `Exit`, `Exec`, `Join`, `Fork`, `Yield`, `Create`,
`Remove`, `Open`, `Read`, `Write`, `Close`, `getPT`, `CD`, `LS`, `MKDIR`.

### Shell (`shell.c`)
```
main()
    -> loop:
         Write(prompt, CONSOLE_OUTPUT)
         Read(line, CONSOLE_INPUT)
         parsea línea en comando + argumentos
         pid = Exec(comando, argv)
         if (pid > 0): Join(pid)
```

### Programa `ps.c`
```
getPT(buffer)    // SC_GETPT: kernel serializa processTable a espacio usuario
    -> imprime pid/status/name de cada entrada
```

## Estructuras de datos clave

| Estructura | Definida en | Descripción |
|------------|-------------|-------------|
| `__start` | `userland/start.s` | Entry point único en address 0x0 para todos los ejecutables |
| Stubs ASM | `userland/start.s` | Un stub por syscall; cada uno es 3 instrucciones MIPS |
| `lib.h` | `userland/lib.h` | Declaraciones de funciones de librería mínima |

## Dependencias
- **Depende de:** `userprog/syscall.h` (constantes `SC_*`, incluido desde `start.s`
  con `#define IN_ASM`), `userland/arrangement.ld` (linker script para layout)
- **Es usado por:** NachOS lo carga vía `AddressSpace` cuando se ejecuta `SC_EXEC`

## Invariantes / precondiciones importantes
- `__start` **debe** quedar en la dirección 0x0 del ejecutable: el Makefile garantiza
  que `start.o` sea el primer objeto en el link, y el linker script fija la base en 0x0.
- Los stubs de syscall en `start.s` incluyen `syscall.h` con `IN_ASM` definido para
  obtener solo las constantes `SC_*` sin declaraciones C.
- La librería de usuario (`lib.c`) no usa malloc ni funciones de libc del host — todo
  es stack o variables globales de tamaño fijo.
- Los programas de usuario no pueden usar `new`/`delete` de C++ ni la STL: solo C.

## Notas de diseño
- El linker script `arrangement.ld` usa `-N` (segmentos combinados, no alineados a
  página) y `-s` (strip de símbolos) para minimizar el tamaño del ejecutable; el
  resultado pasa por `coff2noff` para generar el header NOFF que `AddressSpace` parsea.
- La separación entre `shell.c` (full-featured) y `tinyshell.c` (mínimo) permite
  testear multiprogramación con una shell más simple si la completa tiene bugs.
- `swapTest.c` asigna arrays grandes en el stack o heap para presionar el CoreMap y
  forzar swapping — es el test de integración del subsistema vmem.
- `ps.c` usa la syscall `SC_GETPT` que no es POSIX; expone el estado interno del
  kernel en forma de snapshot (ver subsistema procesos).

## Limitaciones conocidas / TODOs
- No hay `malloc` en el userland: la asignación dinámica requiere manejar el heap
  manualmente o usar arrays de tamaño fijo.
- `shell.c` no soporta pipes (`|`) ni redirección (`>`, `<`).
- No hay manejo de señales desde el userland.
- `FILE_NAME_MAX_LEN` (9 caracteres) limita los nombres de archivo usables desde
  los programas de usuario.
