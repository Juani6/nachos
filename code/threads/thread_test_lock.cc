/*
    En este programa se ejecutan 2 funciones una con un LOCK (TestLock )y una sin (Test)
    La idea central es que se crean 2 Threads los cuales entran a la funcion con una variable global (valor_compartido) que comparten
    Los Threads escriben el valor que les pasaron al ser creados (0 y 1) y sobreescriben la variable global previo al print, forzando un cambio de contexto
    de esta forma fomentamos un race condition, al no haber lock se podra ver como el Thread 1 imprime 1 (cuando deberia ser 0) y viceversa
    
    En la otra funcion esto no sucede ya que antes de modificar la variable se toma un lock (shareLock) y no se suelta hasta imprimirlo, resultando en que cada thread imprime lo que deberia
 */
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