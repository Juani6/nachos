/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"

#include <stdio.h>
#include "lock.hh"
#include "system.hh"
#include <string.h>

#define SIZE_BUFFER  3
#define TOTAL  1000

#define CONS  4
#define PROD  4

int Buffer [SIZE_BUFFER];
int cant_elem  = 0;
bool done [CONS + PROD];
int cant_total = 0;
Lock* lock     = new Lock("ProdCons");

static void Prod(void* i){
    const int *n = (int*) i;
    while(cant_total < TOTAL){

        lock->Acquire();
        printf("Produciendo %s....\n",currentThread->GetName());
        if (cant_elem<SIZE_BUFFER && cant_total < TOTAL) {
            Buffer[cant_elem] = cant_elem;
            cant_elem++;
            cant_total++;
            printf("Producido :)\n");
        }
        lock->Release();
        currentThread->Yield();
    }
    done [CONS + *n ] = true;
		return;
}


static void Cons(void* i){
    const int *n = (int*) i;
    while(cant_total < TOTAL){

        lock->Acquire();
        if(cant_elem > 0){
            printf("ÑamÑamÑam %s\n",currentThread->GetName());
            printf("Consumi: %d\n",Buffer[cant_elem-1]);
            cant_elem--;
        }
        lock->Release();
        currentThread->Yield();
        
    }
    done[*n] = true;
		return;
}

void
ThreadTestProdCons()
{
    //PORQUE SI PASO LAS VARIABLES DE OTRA FORMA SE PASAN MAL
    int* valuesC = new int [CONS];
    int* valuesP = new int [PROD];
    char** nameC = new char*[CONS];
		char** nameP = new char* [PROD];
    for (int i=0;i<CONS;i++){
        nameC[i] = new char[20];
        snprintf(nameC[i],20,"Cons:%d",i);
    }
    for (int i=0;i<PROD;i++){
        nameP[i] = new char[20];
        snprintf(nameP[i],20,"Prod:%d",i);
    }

    for(int i = 0;i<CONS;i++){
        valuesC[i]=i;
        printf("i=%d\n",i);
        Thread* t = new Thread(nameC[i]);
        t->Fork(Cons,(void*) &(valuesC[i]));
    }
    for(int i = 0;i<PROD;i++){
        valuesP[i] = i;
        Thread* t = new Thread( nameP[i] );
        t->Fork(Prod,(void*) &(valuesP[i]));
    }

    for (int i=0; i<(CONS + PROD); i++){
        while(!done[i]){
            currentThread->Yield();
        }
    }
    printf("Total :%d\n",cant_total);

    delete lock;
		for (unsigned i = 0; i < CONS; i++)
			delete[] nameC[i];
		for (unsigned i = 0; i < PROD; i++)
			delete[] nameP[i];
		delete[] valuesC;
		delete[] valuesP;
		delete[] nameC;
		delete[] nameP;
}
