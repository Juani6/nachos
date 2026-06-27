# Nivel 2 — Módulo: lib.c / lib.h

## Archivo
- **Ruta:** `userland/lib.c`, `userland/lib.h`
- **Subsistema:** [Userland](../01-subsistemas/userland.md)

## Propósito del archivo
Librería mínima de C para los programas de usuario de NachOS. Reemplaza las partes de
`libc` que se necesitan (strings, conversión numérica, output) sin depender de la libc
del host. Todo el código opera sobre el stack o variables de tamaño fijo; no hay malloc.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `strlen` | `unsigned strlen(const char *s)` | Longitud de string; retorna 0 si `s == NULL` |
| `strCopy` | `unsigned strCopy(const char *src, char *dst)` | Copia string con null-terminator; retorna cantidad de chars copiados |
| `puts2` | `void puts2(const char *s)` | Escribe string a `CONSOLE_OUTPUT` (fd=1) via syscall `Write` |
| `reverse` | `void reverse(char[], int len)` | Invierte un array de chars in-place |
| `itoa` | `void itoa(int n, char *str)` | Convierte entero a string decimal; soporta negativos; usa `reverse` al final |
| `atoi2` | `int atoi2(char *str)` | Convierte string decimal a entero (sin manejo de negativos ni errores) |
| `putInt` | `void putInt(int x)` | Combina `itoa` + `puts2`: imprime un entero en la consola |

## Funciones/símbolos internos
_(ninguno — todas son públicas)_

## Dependencias

- **Incluye:** `userprog/syscall.h`, `lib.h`
- **Es incluido por:** todos los programas de usuario que usan `#include "lib.h"`

## Notas específicas de implementación

- **Sin libc del host:** este archivo se compila para MIPS (`mipsel-linux-gnu-gcc`)
  y no puede usar stdlib.h, string.h ni ninguna librería del host Linux. La única
  interfaz externa son las syscalls de NachOS via los stubs de `start.s`.

- **`puts2` usa fd=1 (CONSOLE_OUTPUT):** llama `Write(s, len, 1)` directamente;
  el fd 1 está reservado para la consola de salida por convención (ver constructor
  de `Thread` donde se reservan los índices 0 y 1 en `fdTable`).

- **`itoa` con dígitos inversos:** construye el string de derecha a izquierda (cada
  `rem = n % 10` agrega el dígito menos significativo) y luego llama `reverse`.
  El caso `n == 0` se maneja por separado antes del loop.

- **`atoi2`:** no detecta overflow ni caracteres no numéricos. Adecuado para uso
  interno donde el formato está garantizado.
