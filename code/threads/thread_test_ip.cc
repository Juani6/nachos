#include "thread_test_ip.hh"

#include "thread.hh"
#include "lock.hh"
#include "system.hh"
#include <stdio.h>
#include <limits.h>

Lock* sharedLock;
bool loop;

void hiloLoop(void* ptr) {
	int i = 0;
	while (loop) {
		//DEBUG('s',"Funcion feliz :D\n");
		if (i < 100)
			i++;
		else 
			i = 0;
		currentThread->Yield();
	}
	DEBUG('t', "[%s] Finalizado,\n", currentThread->GetName());
	return;
}

void tareaPesada(void* ptr) {
	unsigned char j;
	unsigned char arr[UCHAR_MAX];
	printf("[%s] Tomando el lock\n", currentThread->GetName());
	sharedLock->Acquire();
	for (unsigned char i = 0; i < UCHAR_MAX - 1; i++) {
		j = 0;
		for (; j < UCHAR_MAX - 1; j++);
		printf("%u", i);
		arr[i] = j;
		currentThread->Yield();
	}
	sharedLock->Release();
	DEBUG('t', "[%s] Finalizado,\n", currentThread->GetName());
	printf("Termino la tarea pesada!\n");
	return;
}

void MUYIMPORTANTE(void* ptr) {
	sharedLock->Acquire();
	printf("Informacion muy importante\n");
	loop = false;
	DEBUG('t', "[%s] Finalizado,\n", currentThread->GetName());
	sharedLock->Release();
	return;
}


void ThreadIPTest() {
	sharedLock = new Lock("Test lock");
	
	Thread *t1 = new Thread("Thread Prioridad 0", true,0);
	Thread *t2 = new Thread("Thread Prioridad 9",true,9);
	Thread *t3 = new Thread("Thread Prioridad 4",true);
	
	loop = true;
	t1->Fork(tareaPesada,NULL); // Prioridad 0
	
	currentThread->Yield();
	
	t3->Fork(hiloLoop,NULL); // Prioridad 4
	
	currentThread->Yield();
	
	t2->Fork(MUYIMPORTANTE,NULL); // Prioridad 9
	
	currentThread->Yield();
	
	t3->Join();
	t2->Join();
	t1->Join();
	printf("Termino el padre\n");
	
	delete sharedLock;
	return;
}

