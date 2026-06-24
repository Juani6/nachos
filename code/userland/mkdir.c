#include "syscall.h"
#include "lib.h"

int main(int argc,char* argv[]) {
	if (argc < 2 ) {
		puts2("Cantidad insuficiente de argumentos\n");
		return -1;
	}
	char* path = argv[1];
	int res = MKDIR(path);
	if (res == -1) {
		puts2("Error al crear el directorio\n");
		return res;
	}
	return 0;
}