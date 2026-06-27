#include "rwlock.hh"
#include "condition.hh"
#include "lock.hh"

RWLock::RWLock() {
	lock = new Lock("rwlock");
	cond = new Condition("rwcond",lock);
	readers = 0;
	writing = 0;
}

RWLock::~RWLock() {
	delete lock;
	delete cond;
}

void
RWLock::AcquireRead() {
	lock->Acquire();
	while(writing) {
		cond->Wait();
	}
	readers++;
	lock->Release();
}

void
RWLock::ReleaseRead() {
	lock->Acquire();
	readers--;
	if (readers == 0) {
		cond->Signal();
	}
	lock->Release();
}

void 
RWLock::AcquireWrite() {
	lock->Acquire();
	while(writing || readers > 0) {
		cond->Wait();
	}
	writing = true;
	lock->Release();
}

void
RWLock::ReleaseWrite() {
	lock->Acquire();
	writing = false;
	cond->Broadcast();
	lock->Release();
}