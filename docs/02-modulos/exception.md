# Nivel 2 — Módulo: exception.cc / exception.hh

## Archivo
- **Ruta:** `userprog/exception.cc`, `userprog/exception.hh`
- **Subsistema:** [Gestión de Procesos y Syscalls](../01-subsistemas/procesos.md)

## Propósito del archivo
Es el punto de entrada del kernel cuando ocurre una excepción MIPS: syscall, page fault,
acceso read-only, o error de bus/dirección. Registra un handler por tipo de excepción
en `Machine` y los implementa todos. Contiene la lógica de todas las syscalls del
sistema (`SC_HALT` a `SC_MKDIR`).

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `SetExceptionHandlers` | `void SetExceptionHandlers()` | Registra handlers en `machine` para todos los tipos de excepción MIPS |

## Funciones/símbolos internos

| Función/símbolo | Descripción breve |
|-----------------|-------------------|
| `SyscallHandler(ExceptionType)` | Dispatcher principal: lee `r2` (scid) y llama al handler específico; llama `IncrementPC()` al terminar |
| `PageFaultHandler(ExceptionType)` | Maneja TLB miss: calcula VPN de `BAD_VADDR_REG`, carga página desde RAM/swap/ejecutable, recarga la TLB con política FIFO circular |
| `ReadOnlyHandler(ExceptionType)` | Acceso a página read-only: imprime error y llama `syscall_SC_EXIT()` |
| `DefaultHandler(ExceptionType)` | Handler de excepciones inesperadas: imprime tipo y `ASSERT(false)` |
| `IncrementPC()` | Avanza PC, PREV_PC y NEXT_PC del MIPS en 4 bytes; **debe** llamarse al finalizar toda syscall exitosa |
| `ExecProcess(void* arg)` | Función que corre el nuevo thread tras `SC_EXEC`: `InitRegisters`, `RestoreState`, `WriteArgs` si hay argv, luego `machine->Run()` |
| `LoadFromSwap(Thread*, unsigned vpn)` | Carga una página desde el archivo de swap o vía demand loading; usado por `PageFaultHandler` |
| `syscall_SC_HALT()` | Llama `interrupt->Halt()` |
| `syscall_SC_CREATE()` | Lee filename de r4, llama `fileSystem->Create()` |
| `syscall_SC_OPEN()` | Lee filename de r4, `fileSystem->Open()`, inserta en `fdTable`, retorna fd en r2 |
| `syscall_SC_CLOSE()` | Lee fd de r4, `fdTable->Remove(fd)`, `delete file` |
| `syscall_SC_REMOVE()` | Lee filename de r4, `fileSystem->Remove()` |
| `syscall_SC_READ()` | Lee de consola (`CONSOLE_INPUT`) o de `fdTable[fid]`; escribe bytes en espacio usuario |
| `syscall_SC_WRITE()` | Escribe a consola (`CONSOLE_OUTPUT`) o a `fdTable[fid]`; lee bytes de espacio usuario |
| `syscall_SC_EXIT()` | Setea exit status; si hay más procesos `Finish()`; si es el último `interrupt->Halt()` |
| `syscall_SC_EXEC()` | Abre ejecutable, crea Thread + AddressSpace, inserta en processTable, Fork a ExecProcess; retorna pid |
| `syscall_SC_JOIN()` | `processTable->Get(pid)->Join()`; elimina de processTable; retorna exit status |
| `syscall_SC_GETPT()` | Serializa processTable al espacio usuario vía `writeProcessDataToUser()` |
| `syscall_SC_CD()` | `#ifdef FILESYS`: `fileSystem->ChangeDir(path)` |
| `syscall_SC_LS()` | `#ifdef FILESYS`: `fileSystem->List()` |
| `syscall_SC_MKDIR()` | `#ifdef FILESYS`: `fileSystem->CreateDir(path)` |

## Dependencias

- **Incluye:** `transfer.hh`, `syscall.h`, `filesys/directory_entry.hh`, `threads/system.hh`, `args.hh`
- **Es incluido por:** `threads/system.cc` (llama a `SetExceptionHandlers()` durante `Initialize()`)

## Notas específicas de implementación

- **Convención de llamada de syscalls MIPS:**
  - `r2` = número de syscall (leído por `SyscallHandler`)
  - `r4`, `r5`, `r6`, `r7` = argumentos 1–4
  - `r2` = valor de retorno (escrito con `machine->WriteRegister(2, ...)`)
  - `IncrementPC()` avanza el PC tres registros: `PREV_PC = PC`, `PC = NEXT_PC`, `NEXT_PC += 4`

- **TLB replacement en `PageFaultHandler`:** usa una variable estática `tlb_index`
  (round-robin sobre `TLB_SIZE`). Antes de cargar la nueva entrada, guarda los bits
  `dirty`/`use` de la entrada que se va a desalojar en la `pageTable` del proceso.

- **`SC_EXIT` y apagado:** cuenta cuántas entradas no-nulas hay en `processTable` bajo
  `pTLock`. Si es la última, llama `interrupt->Halt()` en lugar de `Finish()`, porque
  no quedaría ningún thread que limpiara el último thread terminado.

- **`SC_EXEC` hereda directorio de trabajo:** `newThread->space->currentDirSector = currentThread->space->currentDirSector`. El hijo comienza en el mismo directorio que el padre.

- **`#ifdef FILESYS`:** `SC_CD`, `SC_LS`, `SC_MKDIR` solo están disponibles cuando
  se compila con el filesystem completo (no con `FILESYS_STUB`).

- **`SC_READ` sobre archivo:** nota que en la rama de archivo regular, retorna `SC_ERROR`
  cuando `readBytes > 0` y retorna `readBytes` cuando `readBytes <= 0`; el comportamiento
  parece invertido respecto a la intención, pero se deja así tal cual está en el código.
