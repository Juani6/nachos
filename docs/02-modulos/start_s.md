# Nivel 2 — Módulo: start.s

## Archivo
- **Ruta:** `userland/start.s`
- **Subsistema:** [Userland](../01-subsistemas/userland.md)

## Propósito del archivo
Assembly MIPS que forma el runtime de arranque de todos los ejecutables de usuario de
NachOS. Define `__start` (entry point en address 0x0) y un stub de 3 instrucciones por
cada syscall. Debe ser el primer objeto linkeado para que `__start` quede en la
dirección 0x0, donde el kernel hace el jump inicial.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `__start` | `void __start()` | Entry point: `jal main`, luego mueve el valor de retorno a `$a0` y llama `Exit` |
| `Halt` | `void Halt()` | Stub: carga `SC_HALT` en `$v0`, `syscall`, `j $ra` |
| `Exit` | `void Exit(int status)` | Stub: `SC_EXIT`; arg en `$a0` |
| `Exec` | `SpaceId Exec(char* name, char** argv)` | Stub: `SC_EXEC`; args en `$a0`, `$a1` |
| `Join` | `int Join(SpaceId id)` | Stub: `SC_JOIN`; arg en `$a0` |
| `Fork` | `void Fork(void (*func)())` | Stub: `SC_FORK`; arg en `$a0` |
| `Yield` | `void Yield()` | Stub: `SC_YIELD` |
| `getPT` | `int getPT(void* buf)` | Stub: `SC_GETPT`; arg en `$a0` |
| `Create` | `int Create(char* name)` | Stub: `SC_CREATE`; arg en `$a0` |
| `Remove` | `int Remove(char* name)` | Stub: `SC_REMOVE`; arg en `$a0` |
| `Open` | `OpenFileId Open(char* name)` | Stub: `SC_OPEN`; arg en `$a0` |
| `Read` | `int Read(char* buf, int size, OpenFileId id)` | Stub: `SC_READ`; args en `$a0`, `$a1`, `$a2` |
| `Write` | `int Write(char* buf, int size, OpenFileId id)` | Stub: `SC_WRITE`; args en `$a0`, `$a1`, `$a2` |
| `Close` | `int Close(OpenFileId id)` | Stub: `SC_CLOSE`; arg en `$a0` |
| `CD` | `int CD(char* path)` | Stub: `SC_CD`; arg en `$a0` |
| `LS` | `void LS()` | Stub: `SC_LS` |
| `MKDIR` | `int MKDIR(char* path)` | Stub: `SC_MKDIR`; arg en `$a0` |
| `__main` | (dummy) | Función vacía para que gcc no se queje; solo hace `j $ra` |

## Funciones/símbolos internos
_(ninguno — todas las etiquetas son globales)_

## Dependencias

- **Incluye:** `syscall.h` (con `#define IN_ASM` para obtener solo las constantes `SC_*` sin declaraciones C)
- **Es incluido por:** el linker — `start.o` es el primer objeto en todos los ejecutables de `userland/`

## Notas específicas de implementación

- **Convención de llamada MIPS (O32 ABI):**
  - Argumentos: `$a0`=`$4`, `$a1`=`$5`, `$a2`=`$6`, `$a3`=`$7`
  - Valor de retorno: `$v0`=`$2`
  - Cada stub carga el número de syscall en `$v0` con `addiu $2, $0, SC_XXX`
  - La instrucción `syscall` hace el trap al kernel MIPS simulado
  - `j $31` retorna al llamador (equivalente a `ret` en x86)

- **`__start` y `main`:**
  - `jal main` — llama a la función `main()` del programa usuario
  - `move $4, $2` — mueve el valor de retorno de `main` a `$a0` (argumento para `Exit`)
  - `jal Exit` — llama al stub de `Exit` que hace `SC_EXIT`
  - El kernel NachOS siempre salta a la dirección 0x0 para iniciar un proceso; por eso
    `__start` debe estar ahí.

- **`#define IN_ASM`:** permite incluir `syscall.h` desde assembly. El header usa
  este guard para omitir las declaraciones de funciones C que no son válidas en ASM,
  exponiendo solo las macros `#define SC_XXX <número>`.

- **Stubs sin stack frame:** cada stub es exactamente 3 instrucciones MIPS y no toca
  el stack. Los argumentos ya están en los registros `$a*` por la convención de llamada
  del compilador C.

- **Por qué ASM:** las syscalls de NachOS requieren la instrucción `syscall` de MIPS,
  que no es generada por el compilador C en código de usuario normal. No hay otra forma
  de generar ese trap desde C sin pasar por un stub en assembly.
