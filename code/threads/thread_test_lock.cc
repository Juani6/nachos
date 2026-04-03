#include "thread_test_lock.hh"

#include "thread.hh"
#include "lock.hh"
#include "stdio.h"
#include <unistd.h>
#include "system.hh"

Lock* shareLock = new Lock("TestLock");
int valor_compartido;

void Test(void* i)
{
    const int *n = (int*) i;
    for (int j=0;j<5;j++)
    {
        valor_compartido = *n;
        currentThread->Yield();
        printf("Mi valor [%s] es: %d\n",currentThread->GetName(),valor_compartido);
    }

}

void TestLock(void* i)
{
    const int *n = (int*) i;
    for (int j=0;j<5;j++)
    {
        shareLock->Acquire();
        valor_compartido = *n;
        currentThread->Yield();
        printf("Mi valor [%s] es: %d\n",currentThread->GetName(),valor_compartido);
        shareLock->Release();
    }

}



void ThreadLockTest()
{
    int a = 0;
    int b = 1;
    Thread* t1 = new Thread("Thread1",1);
    Thread* t2 = new Thread("Thread2",1);
    printf("\nAhora EJECUTAMOS sin LOCK\n");
    t1->Fork(Test,(void*) &a);
    t2->Fork(Test,(void*) &b);
    
    t1->Join();
    t2->Join();
    
    printf("\nAhora EJECUTAMOS con LOCK\n");
    
    Thread* t3 = new Thread("Thread1",1);
    Thread* t4 = new Thread("Thread2",1);
    t3->Fork(TestLock,(void*) &a);
    t4->Fork(TestLock,(void*) &b);
    
    t3->Join();
    t4->Join();
    
    delete shareLock;
    return;
}