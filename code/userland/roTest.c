/*
El objetivo de este test es verificar que no se puede escribir en una pagina
que esta con la flag de readOnly. Sin el handler se rompe y si el handler esta puesto
termina el hilo de usuario
*/

#include "../userprog/syscall.h"
#include "lib.h"

int main() {
	
	puts2("Entre a la funcion\n");

	int *ptr = (int*) 128;  // dirección fija en una página readOnly 
  *ptr = 0; 
	
	puts2("Holaaaa\n");
	return 0;
}