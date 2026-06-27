# Nivel 3 — Referencia de Funciones: CoreMap

**Módulo:** [`userprog/coremap.cc`](../02-modulos/coremap.md)
**Subsistema:** [Memoria Virtual](../01-subsistemas/vmem.md)

---

### `int CoreMap::FindPage(Thread *owner, int vpn)`

**Ubicación:** `userprog/coremap.cc`

**Descripción:**
Reserva un frame físico para la página virtual `vpn` del proceso `owner`.
Busca primero un frame libre; si no hay, llama `PickVictim` para seleccionar
uno con la política configurada (`FIFO` o `CLOCK`). Luego hace `PinPage`,
envía la víctima a swap si es dirty, y retorna el frame.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `owner` | `Thread*` | Thread dueño de la página a cargar |
| `vpn` | `int` | VPN de la página que se va a cargar en el frame |

**Retorno:** `int` — número de frame físico (ppn) asignado.

**Efectos secundarios:**
- Si el frame tenía dueño: actualiza `owner->space->pageTable[victimVpn].valid = false`.
- Si la página víctima es `dirty`: llama `SendToSwap` para persistirla.
- Actualiza `coreMap[ppn]: owner, vpn, isFree=false`.
- Llama `PinPage(ppn)` antes de enviar a swap (protege contra doble desalojo).

**Precondiciones / contexto de llamada:**
- Llamado desde `AddressSpace::LoadPage`.
- Requiere que `mMapLock` no esté tomado por el llamador (lo adquiere internamente).

**Ver también:** `PickVictim`, `SendToSwap`, `PinPage`

---

### `int CoreMap::PickVictim()`

**Ubicación:** `userprog/coremap.cc`

**Descripción:**
Selecciona el frame a desalojar según la política de reemplazo:

- **FIFO** (sin `-DFRPOLICY_CLOCK`): round-robin simple sobre todos los frames;
  salta los pinneados.
- **CLOCK** (con `-DFRPOLICY_CLOCK`): 4 pasadas:
  1. `!use && !dirty` (hit ideal).
  2. `!use && dirty` (candidato sucio, limpia `use`).
  3. `!use && !dirty` (segunda oportunidad si pasada 1 no encontró).
  4. Cualquier frame no pinneado (fallback).
  
  En cada pasada, al pasar sobre un frame con `use=true`, lo limpia (`use=false`).

**Retorno:** `int` — ppn del frame víctima seleccionado.

**Efectos secundarios:**
- Puede modificar el bit `use` de entradas en la CoreMap (CLOCK).
- Avanza el puntero de reloj interno.

**Precondiciones / contexto de llamada:**
- Llamado solo desde `FindPage`, bajo el lock de la CoreMap.
- Al menos un frame no puede estar pinneado (si todos están pinneados: deadlock).

**Ver también:** `FindPage`, `PinPage`

---

### `void CoreMap::SendToSwap(int ppn)`

**Ubicación:** `userprog/coremap.cc`

**Descripción:**
Escribe el contenido del frame `ppn` al archivo de swap del proceso dueño,
en la posición correspondiente a su VPN. Luego invalida la entrada de la TLB
si la página está en ella.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `ppn` | `int` | Frame físico a enviar a swap |

**Efectos secundarios:**
- Llama `CheckTLB(ppn)` para invalidar la entrada en `machine->tlb` si coincide.
- `swapFile->WriteAt(mem + ppn*PAGE_SIZE, PAGE_SIZE, vpn*PAGE_SIZE)`.
- Actualiza `shadowTable[vpn].isInSwap = true` en el `AddressSpace` dueño.
- Setea `pageTable[vpn].dirty = false`, `pageTable[vpn].valid = false`.

**Precondiciones / contexto de llamada:**
- El frame debe estar pinneado (`isPinned==true`) antes de llamar.
- Llamado desde `FindPage` cuando la víctima es dirty.

**Ver también:** `FindPage`, `AddressSpace::LoadPage`, `CheckTLB`

---

### `void CoreMap::PinPage(int ppn)` / `void CoreMap::UnPinPage(int ppn)`

**Ubicación:** `userprog/coremap.cc`

**Descripción:**
`PinPage`: marca el frame como pinneado (`isPinned=true`) para que `PickVictim`
no lo seleccione mientras una operación de I/O está en curso.

`UnPinPage`: libera el pin (`isPinned=false`) una vez que el I/O terminó.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `ppn` | `int` | Frame a pinnear / despinnear |

**Efectos secundarios:**
- Modifica `coreMap[ppn].isPinned`.

**Precondiciones / contexto de llamada:**
- `PinPage` debe llamarse antes de cualquier operación de swap para proteger el frame.
- `UnPinPage` debe llamarse después de que la operación de I/O complete.
- Ambas deben llamarse bajo `mMapLock`.

**Ver también:** `FindPage`, `SendToSwap`

---

### `void CoreMap::FreePage(int ppn)`

**Ubicación:** `userprog/coremap.cc`

**Descripción:**
Libera un frame al destruir el proceso. Pone a cero el contenido físico del frame
(`memset`) para evitar fuga de datos entre procesos, y lo marca como libre en la CoreMap.

**Parámetros:**
| Nombre | Tipo | Descripción |
|--------|------|-------------|
| `ppn` | `int` | Frame a liberar |

**Efectos secundarios:**
- `memset(machine->mainMemory + ppn*PAGE_SIZE, 0, PAGE_SIZE)`.
- `coreMap[ppn].isFree = true; coreMap[ppn].owner = nullptr`.

**Precondiciones / contexto de llamada:**
- Llamado desde el destructor de `AddressSpace` para cada frame del proceso.
- No debe llamarse con el frame pinneado.

**Ver también:** `AddressSpace::~AddressSpace`
