/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


/// El problema de inversión de prioridad no puede ser resuelta directamente en semaforos ya que estos carecen de una nocion de propiedad
/// es decir, que la utilidad de los semaforos son restringir el ingresoa zonas criticas y/o señalizacion (handshakes), por ende el sistema operativo 
/// no tiene la certeza de a quien deberia cambiar la prioridad o beneficiar (en relacion al scheduler).
/// En cambio las variables de condicion y Locks tienen un recurso con ownership, y por ende el sistema operativo puede manejar de forma eficiente los procesos que estan compitiendo por esos recursos.

#include "lock.hh"
#include "system.hh"
#include <string.h>
#include "lib/list.hh"
/// Dummy functions -- so we can compile our later assignments.

Lock::Lock(const char *debugName)
{
    name = debugName;
    sem = new Semaphore("semLock",1);
    holderThread = NULL;
}

Lock::~Lock()
{
    DEBUG('s',"Eliminando Lock: [%s]\n",name);
    ASSERT(holderThread == NULL);
    delete sem;   
}

const char *
Lock::GetName() const
{
    return name;
}

void
Lock::Acquire()
{
    //DEBUG('s', "Thread: %s trying to ACQUIRE\n",currentThread->GetName());
    ASSERT(!this->IsHeldByCurrentThread());

    if(holderThread && holderThread->GetPriority() < currentThread->GetPriority()){
        DEBUG('s',"[%s] Hubo una inversion de prioridad \n",currentThread->GetName());
        
		IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
				
        // Check de seguridad para evitar que un proceso modifique el estado de otro.
		if (holderThread->GetStatus() == READY){
            int oldPrio = holderThread->GetPriority();
            holderThread->SetPriority(currentThread->GetPriority()); 
            scheduler->ReadyToRun(holderThread,oldPrio);
        }    
		interrupt->SetLevel(oldLevel);
	}
    sem -> P();
    DEBUG('s', "Thread: %s ACQUIRE\n",currentThread->GetName());
    holderThread = currentThread;
}

void
Lock::Release()
{
    //DEBUG('s', "Thread: \"%s\" trying to RELEASE\n",currentThread->GetName());
    ASSERT(this->IsHeldByCurrentThread());
    holderThread->SetPriority(holderThread->GetOriginalPriority());
    holderThread = NULL;
    DEBUG('s', "Thread: %s RELEASE\n",currentThread->GetName());
    sem -> V();
}

bool
Lock::IsHeldByCurrentThread() const
{
    if (holderThread == NULL) return false;
    DEBUG('s', "Holder :%s \t Current:%s  \n",holderThread->GetName(),currentThread->GetName());
    return holderThread == currentThread; 
}
