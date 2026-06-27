# Nivel 2 — Módulo: synchconsole.cc / synchconsole.hh

## Archivo
- **Ruta:** `userprog/synchconsole.cc`, `userprog/synchconsole.hh`
- **Subsistema:** [Gestión de Procesos y Syscalls](../01-subsistemas/procesos.md)

## Propósito del archivo
Envuelve la `Console` hardware simulada (que es asincrónica) con una interfaz
sincrónica. `PutChar` bloquea hasta que el caracter fue escrito; `GetChar` bloquea
hasta que hay un caracter disponible. Además serializa el acceso concurrente con
locks separados para lectura y escritura.

## Funciones/símbolos públicos

| Función/símbolo | Firma | Descripción breve |
|-----------------|-------|-------------------|
| `SynchConsole` | `SynchConsole()` | Crea `Console`, semáforos `readAvail(0)` y `writeDone(0)`, locks `writeLock` y `readLock` |
| `~SynchConsole` | `~SynchConsole()` | Destruye todos los objetos creados |
| `PutChar` | `void PutChar(char ch)` | Adquiere `writeLock`, llama `Console::PutChar`, espera en `writeDone->P()` hasta interrupción, libera lock |
| `GetChar` | `char GetChar()` | Adquiere `readLock`, espera en `readAvail->P()` hasta interrupción, llama `Console::GetChar()`, libera lock |
| `ReadAvail` | `static void ReadAvail(void* arg)` | Callback de la console hardware: hace `readAvail->V()` |
| `WriteDone` | `static void WriteDone(void* arg)` | Callback de la console hardware: hace `writeDone->V()` |

## Funciones/símbolos internos
_(ninguno — todas las funciones son públicas o callbacks estáticos)_

## Dependencias

- **Incluye:** `synchconsole.hh` (incluye `machine/console.hh`, `threads/semaphore.hh`, `threads/lock.hh`)
- **Es incluido por:** `exception.cc` (handlers de `SC_READ` y `SC_WRITE`)

## Notas específicas de implementación

- **Instanciación lazy:** `synchConsole` global se crea en el primer `SC_READ` o
  `SC_WRITE` con `CONSOLE_INPUT`/`CONSOLE_OUTPUT`, no en `Initialize()`.

- **Callbacks estáticos:** `Console` toma punteros a función C para sus interrupciones.
  `ReadAvail` y `WriteDone` reciben `void* arg = this` (puntero a `SynchConsole`)
  y llaman los semáforos del objeto.

- **Locks separados para read y write:** `readLock` serializa accesos concurrentes de
  múltiples threads que leen de consola; `writeLock` serializa escrituras. Esto permite
  que un thread lea y otro escriba en paralelo.

- **No hay buffer:** cada `PutChar` escribe exactamente un byte y espera el `writeDone`.
  Para escribir un string se llama `PutChar` en loop desde `SC_WRITE`.

- **Uso en SC_READ consola:** el handler de `SC_READ` llama `GetChar` en loop hasta
  leer `size` bytes o encontrar `'\n'`, acumulando en un buffer auxiliar que luego
  se escribe al espacio usuario con `WriteBufferToUser`.
