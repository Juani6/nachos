#ifndef NACHOS_THREADS_RWLOCK__HH
#define NACHOS_THREADS_RWLOCK__HH

class Lock;
class Condition;

class RWLock {
	public:
		RWLock();
		~RWLock();

		void AcquireRead();
		void ReleaseRead();

		void AcquireWrite();
		void ReleaseWrite();



	private:
		Lock* lock;
		Condition* cond;
		int readers;
		bool writing;
};

#endif