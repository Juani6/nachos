#include "lib.h"

int main(void) {
	
	char str[10] = "Hola Mundo";
	puts2(str);
	unsigned len = strlen(str);
	char entero[2];
	itoa(-1,entero);
	puts2(entero);
	return 0;
}