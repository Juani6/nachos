# Nivel 1 — Subsistema: Gestión de Procesos y Syscalls

## Propósito
Implementar la capa de multiprogramación sobre el simulador MIPS: cargar ejecutables
NOFF en un `AddressSpace`, mapear cada proceso a un `Thread` del host, despachar las
syscalls que el código MIPS emite vía `syscall`, y mantener la tabla global de procesos
(`processTable`) para `Exec`/`Join`/`Exit`. También provee la consola sincrónica y
las utilidades de transferencia de datos entre espacio usuario y kernel.

## Archivos involucrados

| Archivo | Lenguaje | Rol |
|---------|----------|-----|
| `userprog/exception.cc / .hh` | C++ | `SetExceptionHandlers()`, `SyscallHandler`, `PageFaultHandler`, `ReadOnlyHandler` |
| `userprog/address_space.cc / .hh` | C++ | `AddressSpace`: carga NOFF, tabla de páginas, demand paging, swap por proceso |
| `userprog/executable.cc / .hh` | C++ | Parseo de ejecutables NOFF (cabecera, segmentos) |
| `userprog/args.cc / .hh` | C++ | `SaveArgs` / `WriteArgs`: marshaling de argv entre kernel y espacio usuario |
| `userprog/transfer.cc / .hh` | C++ | `ReadStringFromUser`, `ReadBufferFromUser`, `WriteBufferToUser`, `writeProcessDataToUser` |
| `userprog/synchconsole.cc / .hh` | C++ | `SynchConsole`: consola sincrónica (GetChar/PutChar sobre la console hardware) |
| `userprog/process_data.h` | C | Struct `_processData` (pid, status, name) — snapshot para syscall `GETPT` |
| `userprog/syscall.h` | C | Números de syscall (`SC_*`), `SpaceId`, `OpenFileId`, `SC_ERROR` |
| `userprog/debugger.cc / .hh` | C++ | Debugger interactivo de instrucciones MIPS |
| `userprog/debugger_command_manager.cc / .hh` | C++ | Parser de comandos del debugger |
| `userprog/prog_test.cc` | C++ | Tests de ejecución de programas de usuario |
| `lib/table.hh` | C++ | `Table<T>`: tabla indexada usada para `processTable` y `fdTable` |

## Flujo de control

### Arranque de un proceso (`Exec`)
```
syscall SC_EXEC (en código MIPS)
    |
SyscallHandler -> syscall_SC_EXEC()
    -> ReadStringFromUser(r4) = filename
    -> SaveArgs(r5)           = argv desde espacio usuario
    -> fileSystem->Open(filename) = OpenFile*
    -> new Thread(name, joinable=true)
    -> processTable->Add(newThread) = pid  (bajo pTLock)
    -> new AddressSpace(executable, pid, newThread)
    -> newThread->space->currentDirSector = currentThread->space->currentDirSector
    -> WriteRegister(2, pid)
    -> newThread->Fork(ExecProcess, argv)
          |
          ExecProcess() [corre en el nuevo thread]
              -> space->InitRegisters()
              -> space->RestoreState()
              -> WriteArgs(argv) si hay args
              -> machine->Run()   <-- entra al loop MIPS
```

### Exit de un proceso
```
syscall SC_EXIT
    -> SetExitStatus(r4)
    -> pTLock: cuenta procesos vivos en processTable
    -> si alive > 1: currentThread->Finish()  (el joiner limpia la entrada)
    -> si alive == 1: interrupt->Halt()        (última proceso, apaga NachOS)
```

### Join
```
syscall SC_JOIN(pid)
    -> processTable->Get(pid) = hijo
    -> hijo->Join()            [bloquea; desbloquea cuando hijo llama Finish()]
    -> processTable->Remove(pid)
    -> WriteRegister(2, exitStatus)
```

### Despacho de excepciones MIPS
```
Machine::Run() detecta excepción
    -> llama handler registrado por SetExceptionHandlers()
         SYSCALL_EXCEPTION    -> SyscallHandler   (switch en r2)
         PAGE_FAULT_EXCEPTION -> PageFaultHandler (ver subsistema vmem)
         READ_ONLY_EXCEPTION  -> ReadOnlyHandler  -> Exit
         resto                -> DefaultHandler   -> ASSERT(false)
    -> IncrementPC() al volver (avanza PC+4 para no re-ejecutar syscall)
```

### Tabla de file descriptors por proceso
```
Thread::fdTable  (Table<OpenFile*>*, inicializado en AddressSpace o en Fork)
    -> SC_OPEN:  fileSystem->Open() -> fdTable->Add(f) -> devuelve fd
    -> SC_READ:  fdTable->Get(fd)   -> f->Read()
    -> SC_WRITE: fdTable->Get(fd)   -> f->Write()
    -> SC_CLOSE: fdTable->Remove(fd) -> delete f
```

## Estructuras de datos clave

| Estructura | Definida en | Descripción |
|------------|-------------|-------------|
| `AddressSpace` | `userprog/address_space.hh` | pageTable, numPages, shadowTable, swapFile, pid, owner, exe |
| `ShadowTable` | `userprog/address_space.hh` | Por VPN: `isInSwap`, `readOnly`, `vpn` |
| `processTable` | `threads/system.hh` | `Table<Thread*>*` global, indexada por pid |
| `pTLock` | `threads/system.hh` | `Lock*` que protege acceso a `processTable` |
| `Thread::fdTable` | `threads/thread.hh` | `Table<OpenFile*>*` — descriptores de archivo del proceso |
| `_processData` | `userprog/process_data.h` | Snapshot de pid/status/name para `SC_GETPT` |
| `SynchConsole` | `userprog/synchconsole.hh` | Consola sincrónica global (`synchConsole`) |

## Dependencias
- **Depende de:** `machine` (Machine, MMU, registros MIPS), `threads/scheduler`
  (Thread, Fork, Finish, Join), `filesys` (Open/Read/Write ejecutable y archivos),
  `vmem` (CoreMap, demand paging — activo con `-DVMEM`)
- **Es usado por:** `userland` (los programas MIPS generan syscalls que llegan aquí)

## Invariantes / precondiciones importantes
- `processTable` siempre se accede bajo `pTLock`.
- `IncrementPC()` **debe** llamarse al final de todo handler de syscall exitoso
  (de lo contrario el MIPS re-ejecuta la misma syscall indefinidamente).
- `currentDirSector` del hijo se hereda del padre en `SC_EXEC`; es por proceso,
  almacenado en `AddressSpace`.
- `fdTable` es por thread (proceso): los hijos no comparten descriptores con el padre.
- La consola (`synchConsole`) se crea lazy en la primera llamada a `SC_READ`/`SC_WRITE`
  con `CONSOLE_INPUT`/`CONSOLE_OUTPUT`.

## Notas de diseño
- La syscall `SC_GETPT` expone una copia del estado de `processTable` al espacio
  usuario (usada por el programa `ps.c`): `writeProcessDataToUser` serializa los
  `_processData` en la memoria virtual del proceso.
- Todos los threads de procesos de usuario se crean como `joinable=true`; el join
  es obligatorio para recuperar el exit status y liberar la entrada de `processTable`.
- El número de pid es el índice en `processTable` (`Table<Thread*>`) — no hay asignación
  separada de PID.
- No hay `fork()` semántico (copia de espacio de direcciones); `SC_EXEC` crea siempre
  un nuevo espacio.

## Limitaciones conocidas / TODOs
- `Table<T>::SIZE` (constante) limita el número máximo de procesos simultáneos.
- No hay señales entre procesos (kill, etc.).

