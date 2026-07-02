# Índice de Documentación — NachOS

Generado automáticamente. Organizado en tres niveles:

- **Nivel 1** — Subsistemas (arquitectura y flujos de alto nivel)
- **Nivel 2** — Módulos (un archivo por fuente)
- **Nivel 3** — Referencia de funciones (detalle de cada función no trivial)

Ver también: [00-overview.md](docs/00-overview.md) — identidad del proyecto, arquitectura, herramientas.

---

## Nivel 1 — Subsistemas

| Archivo | Descripción |
|---------|-------------|
| [scheduler.md](docs/01-subsistemas/scheduler.md) | Scheduler con prioridades, threads, context switch, sincronización |
| [procesos.md](docs/01-subsistemas/procesos.md) | Gestión de procesos de usuario, syscalls, excepciones MIPS |
| [vmem.md](docs/01-subsistemas/vmem.md) | Memoria virtual: TLB, demand paging, swap, CoreMap |
| [filesystem.md](docs/01-subsistemas/filesystem.md) | Filesystem sobre disco simulado: inodes, directorios, concurrencia |
| [userland.md](docs/01-subsistemas/userland.md) | Programas de usuario MIPS: runtime, librería mínima, shell |

---

## Nivel 2 — Módulos

### Subsistema: Scheduler / Threads

| Archivo | Fuente |
|---------|--------|
| [thread.md](docs/02-modulos/thread.md) | `threads/thread.cc` / `thread.hh` |
| [scheduler.md](docs/02-modulos/scheduler.md) | `threads/scheduler.cc` / `scheduler.hh` |
| [semaphore.md](docs/02-modulos/semaphore.md) | `threads/semaphore.cc` / `semaphore.hh` |
| [lock.md](docs/02-modulos/lock.md) | `threads/lock.cc` / `lock.hh` |
| [condition.md](docs/02-modulos/condition.md) | `threads/condition.cc` / `condition.hh` |
| [channels.md](docs/02-modulos/channels.md) | `threads/channels.cc` / `channels.hh` |
| [rwlock.md](docs/02-modulos/rwlock.md) | `threads/rwlock.cc` / `rwlock.hh` |
| [switch_x86-64.md](docs/02-modulos/switch_x86-64.md) | `threads/switch_x86-64.S` |

### Subsistema: Procesos y Syscalls

| Archivo | Fuente |
|---------|--------|
| [exception.md](docs/02-modulos/exception.md) | `userprog/exception.cc` |
| [address_space.md](docs/02-modulos/address_space.md) | `userprog/address_space.cc` / `address_space.hh` |
| [executable.md](docs/02-modulos/executable.md) | `userprog/executable.cc` |
| [transfer.md](docs/02-modulos/transfer.md) | `userprog/transfer.cc` |
| [synchconsole.md](docs/02-modulos/synchconsole.md) | `userprog/synchconsole.cc` |

### Subsistema: Memoria Virtual

| Archivo | Fuente |
|---------|--------|
| [coremap.md](docs/02-modulos/coremap.md) | `userprog/coremap.cc` / `coremap.hh` |

### Subsistema: Filesystem

| Archivo | Fuente |
|---------|--------|
| [file_system.md](docs/02-modulos/file_system.md) | `filesys/file_system.cc` / `file_system.hh` |
| [file_header.md](docs/02-modulos/file_header.md) | `filesys/file_header.cc` / `file_header.hh` |
| [directory.md](docs/02-modulos/directory.md) | `filesys/directory.cc` / `directory.hh` |
| [open_file.md](docs/02-modulos/open_file.md) | `filesys/open_file.cc` |
| [file_table.md](docs/02-modulos/file_table.md) | `filesys/file_table.cc` / `file_table.hh` |
| [synch_disk.md](docs/02-modulos/synch_disk.md) | `filesys/synch_disk.cc` |

### Subsistema: Userland

| Archivo | Fuente |
|---------|--------|
| [start_s.md](docs/02-modulos/start_s.md) | `userland/start.s` |
| [lib_userland.md](docs/02-modulos/lib_userland.md) | `userland/lib.c` |
| [shell.md](docs/02-modulos/shell.md) | `userland/shell.c` |

---

## Nivel 3 — Referencia de Funciones

| Archivo | Funciones cubiertas |
|---------|---------------------|
| [referencia-thread.md](docs/03-referencia/referencia-thread.md) | `Thread::Fork`, `Yield`, `Sleep`, `Finish`, `Join`, `StackAllocate`, `SaveUserState`, `RestoreUserState` |
| [referencia-scheduler.md](docs/03-referencia/referencia-scheduler.md) | `Scheduler::ReadyToRun`, `FindNextToRun`, `Run` |
| [referencia-sincronizacion.md](docs/03-referencia/referencia-sincronizacion.md) | `Semaphore::P/V` · `Lock::Acquire/Release` · `Condition::Wait/Signal/Broadcast` · `Channel::Write/Read` · `RWLock::AcquireRead/ReleaseRead/AcquireWrite/ReleaseWrite` |
| [referencia-exception.md](docs/03-referencia/referencia-exception.md) | `SetExceptionHandlers`, `SyscallHandler`, `PageFaultHandler`, `IncrementPC`, `ExecProcess` |
| [referencia-address_space.md](docs/03-referencia/referencia-address_space.md) | `AddressSpace::AddressSpace`, `LoadPage`, `RestoreState`, `InitSwapFile`, `InitRegisters` |
| [referencia-coremap.md](docs/03-referencia/referencia-coremap.md) | `CoreMap::FindPage`, `PickVictim`, `SendToSwap`, `PinPage`, `UnPinPage`, `FreePage` |
| [referencia-file_system.md](docs/03-referencia/referencia-file_system.md) | `FileSystem::Create`, `Open`, `Remove`, `CreateDir`, `ChangeDir`, `ResolvePath`, `DeletePhysicalSector`, `GetDirLock` |
| [referencia-file_header.md](docs/03-referencia/referencia-file_header.md) | `FileHeader::Allocate`, `Deallocate`, `ByteToSector`, `Extend`, `GetIndirectionSector` |
| [referencia-open_file.md](docs/03-referencia/referencia-open_file.md) | `OpenFile::ReadAt`, `WriteAt`, `Read`, `Write`, `~OpenFile` |
| [referencia-file_table.md](docs/03-referencia/referencia-file_table.md) | `FileTable::Acquire`, `Release`, `MarkAsDeleted` |
