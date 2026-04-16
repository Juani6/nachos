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


#include "condition.hh"
#include "system.hh"


/// Dummy functions -- so we can compile our later assignments.
///

Condition::Condition(const char *debugName, Lock *conditionLock)
{
	name = debugName;
	externalLock = conditionLock;
	internalLock = new Lock("internalConditionLock");
	internalSem = new Semaphore("conditionSem", 1);
	waiters = 0;
}

Condition::~Condition()
{
	DEBUG('s',"Eliminando Condition: [%s]\n",name);
  delete internalLock;
	delete internalSem;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
	internalLock->Acquire();
	waiters++;
	DEBUG('s',"%s: Entre en Wait [%s]\n",name,currentThread->GetName());
	internalLock->Release();
	
	externalLock->Release();
	internalSem->P();
	externalLock->Acquire();

	internalLock->Acquire();
	waiters--;
	DEBUG('s',"%s: Sali de Wait [%s]\n",name,currentThread->GetName());
	internalLock->Release();
}

void
Condition::Signal()
{
  internalLock->Acquire();
	if (waiters > 0)
		internalSem->V();
	DEBUG('s',"%s: Hice signal [%s]\n",name,currentThread->GetName());
	internalLock->Release();
}

void
Condition::Broadcast()
{
	internalLock->Acquire();
	for(unsigned i = 0; i < waiters; i++)
		internalSem->V();
	DEBUG('s',"%s: Hice Broadcast [%s]\n",name,currentThread->GetName());
	internalLock->Release();
}
