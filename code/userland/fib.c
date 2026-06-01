/*
La idea de este programa es que sea un stress test para
la vmemcon una funcion que pida muchisimo stack
(spoiler: mala idea (su complejidad es horrible))

Con 24 paginas y demand loading hasta fib(31) calcula

Esto funciona hasta 10 minutos en la ejecucion de fib(45)
tuve que agrandar el stack de 4 kB por que corrompia datos
si se compilaba con mas de 35
*/

#include "../userprog/syscall.h"
#include "lib.h"

int fib(unsigned obj) {
	if (obj <= 1) {
		return 1;
	}
	
	return fib(obj-2) + fib(obj-1);
}

int main(int argc, char* argv[]) {
if (argc < 1)
		return -1;
	int obj = fib(atoi2(argv[1]));
	
	putInt(obj);
	puts2("\n");
	return 0;
}


