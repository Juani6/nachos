#include "lib.h"
#include "../userprog/syscall.h"

int main(int argc, char* argv[]) {
if (argc < 1)
		return -1;
	char* filename = argv[1];

	OpenFileId fd = Open(filename);

	char c = 'a';
	int count = Read(&c,1,fd);
	do {
		puts2(&c);
		count = Read(&c,1,fd);
	} while(count > 0);

	Close(fd);
	return count;
}