# Nivel 2 — Módulo: shell.c

## Archivo
- **Ruta:** `userland/shell.c`
- **Subsistema:** [Userland](../01-subsistemas/userland.md)

## Propósito del archivo
Shell interactiva de NachOS. Lee líneas de la consola, parsea comando y argumentos,
y ejecuta el proceso con `Exec`. Si la línea comienza con `&`, el proceso corre en
background (sin `Join`); de lo contrario espera a que termine (`Join`).

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `main` | `int main(void)` | Loop principal: prompt → readline → parse → exec [+ join] |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `strlen(const char*)` (static inline) | Longitud de string (copia local, no usa lib.h) |
| `WritePrompt(OpenFileId)` (static inline) | Escribe `"--> "` a la consola |
| `WriteError(const char*, OpenFileId)` (static inline) | Escribe `"Error: <msg>\n"` a la consola |
| `ReadLine(char*, unsigned, OpenFileId)` (static) | Lee hasta `\n` o `size` chars de la consola; null-termina; retorna cantidad leída |
| `PrepareArguments(char*, char**, unsigned)` (static) | Tokeniza `line` in-place reemplazando espacios por `\0`; llena `argv[]`; retorna 0 si hay demasiados args |

## Dependencias

- **Incluye:** `userprog/syscall.h`
- **No depende de `lib.h`** (tiene su propia copia estática de `strlen`)

## Notas específicas de implementación

- **Constantes:**
  - `MAX_LINE_SIZE = 60` — largo máximo de la línea de comando
  - `MAX_ARG_COUNT = 32` — máximo de argumentos
  - `ARG_SEPARATOR = ' '` — separador de argumentos (espacio)

- **Parseo in-place:** `PrepareArguments` modifica el buffer `line` reemplazando
  cada espacio por `'\0'`, convirtiendo la línea en una secuencia de strings
  null-terminated contiguos. `argv[i]` apunta a `&line[posición_del_arg_i]`.

- **Background (`&`):** si `line[0] == '&'`, se llama `Exec(line+1, argv)` sin
  `Join`. El proceso hijo corre en background; el shell sigue sin esperar.
  Si no hay `&`, `Join(newProc)` bloquea el shell hasta que el hijo termina.

- **Errores no chequeados:** el `SpaceId` retornado por `Exec` no se valida contra
  `SC_ERROR` — si el ejecutable no existe, `Join` fallará silenciosamente. Hay varios
  `TODO` en el código indicando estos casos pendientes.

- **Loop infinito:** `main` nunca retorna (`return -1` está comentado como
  `// Never reached`). La shell solo termina si el proceso NachOS hace `Halt` o
  si `SC_EXIT` detecta que es el último proceso vivo.

- **Limitaciones conocidas del shell:** sin soporte de pipes (`|`), redirección
  (`>`, `<`), variables de entorno, o historial. Espacios múltiples entre argumentos
  generan argumentos vacíos (no filtrados).
