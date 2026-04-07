/*
Este programa de ejemplo tiene como objetivo probar la libreria de Channels implementada.

Inicialmente un hilo padre forkea y se ejecuta childFun en un hijo, funcion que imprime 10 veces una cadena en la salida estandar forzando
un yield en cada iteracion.

En el primer caso no se utiliza la funcion Join y deberia mostrar una iteracion del hijo e inmediatamente continua en el hilo padre

En el segundo, se utiliza el metodo Thread::Join, se ejecuta el hilo hijo entero y una vez finalizado este continua la ejecucion del padre.
 
*/

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

	printf("\nProbamos sin la rutina forzando un cambio de contexto...\n\n");
	hijo1->Fork(childFun, NULL);
	currentThread->Yield();
	printf("Soy el padre!\n\n");

	printf("Activamos el JOIN...\n");

	hijo1->Fork(childFun, NULL);
	hijo1->Join();
	printf("Soy el padre!\n\n");

	return;
}