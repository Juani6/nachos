# Nivel 2 — Módulo: switch_x86-64.S / switch_x86-64.h

## Archivo
- **Ruta:** `threads/switch_x86-64.S`, `threads/switch_x86-64.h`
- **Subsistema:** [Scheduler / Threads / Sincronización](../01-subsistemas/scheduler.md)

## Propósito del archivo
Implementa el context switch de bajo nivel para arquitectura x86-64. Define dos
símbolos en assembly: `ThreadRoot` (primer frame de ejecución de un thread nuevo) y
`SWITCH` (salva todos los registros callee-saved del thread viejo y restaura los del
nuevo). Es la única parte de NachOS que toca registros de la CPU del host directamente.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `ThreadRoot` | `void ThreadRoot()` | Punto de entrada de cualquier thread nuevo; llama `StartupPC()` (InterruptEnable), luego `InitialPC(InitialArg)`, luego `WhenDonePC()` (ThreadFinish) |
| `SWITCH` | `void SWITCH(Thread *t1, Thread *t2)` | Salva los 16 registros generales + RSP + PC de t1 en `t1->machineState[]`; restaura los de t2 y hace `ret` (que salta al PC guardado de t2) |

## Funciones/símbolos internos
_(ninguno — el archivo contiene solo las dos rutinas globales)_

## Dependencias

- **Incluye:** `switch_x86-64.h` (offsets `_RAX`, `_RBX`, ... `_PC` dentro de `machineState`)
- **Es incluido por:** `threads/switch.h` / `threads/switch.S` (que seleccionan la variante de arquitectura en tiempo de compilación)

## Notas específicas de implementación

- **Registros salvados:** todos los 16 registros de propósito general de x86-64
  (`rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `rbp`, `rsp`, `r8`–`r15`) más el `PC`
  (dirección de retorno extraída del tope del stack).

- **Convención de llamada (System V AMD64 ABI):**
  - `rdi` = primer argumento = `Thread* t1`
  - `rsi` = segundo argumento = `Thread* t2`
  - Registros callee-saved: `rbx`, `rbp`, `r12`–`r15`. Sin embargo, `SWITCH` guarda
    **todos** los registros (incluyendo caller-saved) porque el context switch puede
    ocurrir en cualquier momento.

- **`ThreadRoot` y convención de llamada:**
  Los registros usados al inicio de `ThreadRoot`:
  - `rax` → `StartupPC` (InterruptEnable)
  - `rbx` → `InitialArg`
  - `rsi` → `InitialPC` (la función del thread)
  - `rdi` → `WhenDonePC` (ThreadFinish)
  
  Estos valores son colocados por `Thread::StackAllocate` en `machineState` antes
  del primer `SWITCH` hacia el thread.

- **`SWITCH`: truco con `rsi`:** al restaurar el estado de t2, `rsi` contiene el
  puntero a t2 durante toda la restauración. Al final:
  1. `_PC(%rsi)` → `rax` (dirección de retorno guardada de t2)
  2. `rax` → `0(%rsp)` (sobreescribe la dirección de retorno en el stack de t2)
  3. `_RSI(%rsi)` → `rsi` (restaura el valor real de rsi de t2)
  4. `_RAX(%rax)` → `rax` (restaura rax usando t2 como puntero — en ese momento
     rax ya apunta a t2 gracias al paso anterior)
  5. `ret` → salta al PC guardado de t2

- **Por qué ASM en lugar de C:** el context switch requiere acceso directo a todos
  los registros de la CPU (incluyendo el stack pointer) y manipulación precisa de la
  dirección de retorno en el stack. Esto no es expresable en C/C++ portables; requiere
  que el compilador no modifique el estado entre instrucciones.

- **Alineación de stack:** `ThreadRoot` hace `push %rdi` y `push %rsi` antes de la
  primera llamada (`callq *%rax`) para mantener el stack alineado a 16 bytes según el
  ABI de x86-64.
