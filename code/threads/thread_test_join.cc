#include "thread_test_join.hh"


#include "thread.hh"
#include "stdio.h"
#include <unistd.h>
#include "system.hh"

void childFun(void* x) {
		for (int i = 0; i < 10; i++){
			printf("Hola soy el hijo joineable!\n");
			currentThread->Yield();
		}
}


void ThreadJoinTest() {
	Thread *hijo1 = new Thread("Hijo joineable",1);

	printf("\nProbamos sin la rutina forzando un cambio de contexto...\n");
	hijo1->Fork(childFun, NULL);
	currentThread->Yield();
	printf("Soy el padre!\n\n");

	printf("Activamos el JOIN...\n");

	hijo1->Fork(childFun, NULL);
	hijo1->Join();
	printf("Soy el padre!\n\n");

	return;
}