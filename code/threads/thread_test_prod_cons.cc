/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"

#include <stdio.h>
#include "lock.hh"
#include "condition.hh"
#include "system.hh"
#include <string.h>

#define SIZE_BUFFER 3
#define TOTAL  1000

#define SIZE_CONS 1
#define SIZE_PROD 1

int Buffer [SIZE_BUFFER];
int cant_elem  = 0;
bool done [SIZE_CONS + SIZE_PROD];
int cant_total = 0;
Lock* lock     = new Lock("ProdCons");
Condition* fullBufferLock	 = new Condition("fullBufferLock", lock);
Condition* notEmptyBufferLock  = new Condition("cantFree", lock);

static void Prod(void* i){
    const int *n = (int*) i;
	lock->Acquire();
    while(cant_total < TOTAL){
		
		while (cant_elem == SIZE_BUFFER) {
			printf("Productor esperando (buffer lleno)\n");
			fullBufferLock->Wait();
		}
		if (cant_total < TOTAL) {

			
			Buffer[cant_elem] = cant_total;
			cant_elem++;
			cant_total++;
			printf("Productor produce: [%s] en %d\n", currentThread->GetName(), cant_total);
			
			notEmptyBufferLock->Broadcast();
			
		}
        lock->Release();
        currentThread->Yield();
		lock->Acquire();
    }
	notEmptyBufferLock->Broadcast();
	lock->Release();
    done [SIZE_CONS + *n ] = true;
		return;
}


static void Cons(void* i){
    const int *n = (int*) i;


	lock->Acquire();

    while(cant_total < TOTAL){
        
        while (cant_elem == 0 && cant_total < TOTAL) {
			printf("Consumidor esperando (buffer vacio)\n");
			notEmptyBufferLock->Wait();
		}
		printf("Consumidor consume: [%s] en %d\n", currentThread->GetName(), cant_total);
        cant_elem--;
        fullBufferLock->Broadcast();
        
        lock->Release();
        currentThread->Yield();
		lock->Acquire();
    }
	notEmptyBufferLock->Broadcast();
	lock->Release();
    done[*n] = true;
	return;
}

void
ThreadTestProdCons()
{
    //PORQUE SI PASO LAS VARIABLES DE OTRA FORMA SE PASAN MAL
    int* valuesC = new int [SIZE_CONS];
    int* valuesP = new int [SIZE_PROD];
    char** nameC = new char*[SIZE_CONS];
		char** nameP = new char* [SIZE_PROD];
    for (int i=0;i<SIZE_CONS;i++){
        nameC[i] = new char[20];
        snprintf(nameC[i],20,"Cons:%d",i);
    }
    for (int i=0;i<SIZE_PROD;i++){
        nameP[i] = new char[20];
        snprintf(nameP[i],20,"Prod:%d",i);
    }

    for(int i = 0;i<SIZE_CONS;i++){
        valuesC[i]=i;
        printf("i=%d\n",i);
        Thread* t = new Thread(nameC[i]);
        t->Fork(Cons,(void*) &(valuesC[i]));
    }
    for(int i = 0;i<SIZE_PROD;i++){
        valuesP[i] = i;
        Thread* t = new Thread( nameP[i] );
        t->Fork(Prod,(void*) &(valuesP[i]));
    }

    for (int i=0; i<(SIZE_CONS + SIZE_PROD); i++){
        while(!done[i]){
            currentThread->Yield();
        }
    }
    printf("Total :%d\n",cant_total);

    for (unsigned i = 0; i < SIZE_CONS; i++)
    delete[] nameC[i];
    for (unsigned i = 0; i < SIZE_PROD; i++)
    delete[] nameP[i];
    delete[] valuesC;
    delete[] valuesP;
    delete[] nameC;
    delete[] nameP;
    delete fullBufferLock;
    delete notEmptyBufferLock;
    delete lock;
}
