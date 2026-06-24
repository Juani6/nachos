#include "syscall.h"
#include "lib.h"
int main(int argc, char* argv[]) {
	if (argc < 2) {
		return -1;
	}
	char* path = argv[1];

	if (CD(path) == -1) {
		puts2("Syscall error. Aborting.");
		return -1;
	}
	return 0;
}