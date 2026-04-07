/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons_channels.hh"

#include <stdio.h>
#include "lock.hh"
#include "channels.hh"
#include "system.hh"
#include <string.h>

#define NUMBER_MSG 5

#define SIZE_CONS  1
#define SIZE_PROD  1

static_assert(SIZE_PROD == SIZE_CONS);

//static bool done [SIZE_CONS + SIZE_PROD];
Channel* ch = new Channel("ProdCons channel");



static void Prod(void* i){
    const int *n = (int*) i;
		int val;
		for(int j = 0; j < NUMBER_MSG; j++) {
			val = (*n) *100 + j;
			printf("Productor  [%s]: produce: %d\n", currentThread->GetName(), val);
			ch->Write(val);
			currentThread->Yield();
		}
  //  done[SIZE_CONS + *n ] = true;
		return;
}


static void Cons(void* i){
    const int *n = (int*) i;
		int val;
		for(int j = 0; j < NUMBER_MSG; j++) {
			val = ch->Read();
			printf("Consumidor [%s]: consume: %d\n", currentThread->GetName(), val);
			currentThread->Yield();
		}
    //done[*n] = true;
		return;
}

void
ThreadTestProdConsChannels()
{
    //PORQUE SI PASO LAS VARIABLES DE OTRA FORMA SE PASAN MAL
    int* valuesC = new int [SIZE_CONS];
    int* valuesP = new int [SIZE_PROD];
    char** nameC = new char*[SIZE_CONS];
		char** nameP = new char* [SIZE_PROD];
		Thread *threadArray[SIZE_PROD + SIZE_CONS];
    
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
        DEBUG('s',"i=%d\n",i);
        threadArray[i] = new Thread(nameC[i],1);
        threadArray[i]->Fork(Cons,(void*) &(valuesC[i]));
    }
    
		for(int i = 0;i<SIZE_PROD;i++){
        valuesP[i] = i;
        threadArray[SIZE_CONS + i] = new Thread( nameP[i],1);
        threadArray[SIZE_CONS + i]->Fork(Prod,(void*) &(valuesP[i]));
    }

    for (int i=0; i<(SIZE_CONS + SIZE_PROD); i++){
        threadArray[i]->Join();
    }

    for (unsigned i = 0; i < SIZE_CONS; i++)
    delete[] nameC[i];
    for (unsigned i = 0; i < SIZE_PROD; i++)
    delete[] nameP[i];
    delete[] valuesC;
    delete[] valuesP;
    delete[] nameC;
    delete[] nameP;
		//delete[] threadArray;
    delete ch;
}
