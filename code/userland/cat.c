#include "lib.h"
#include "../userprog/syscall.h"

int main(int argc, char* argv[]) {
if (argc < 2)
		return -1;
	char* filename = argv[1];

	OpenFileId fd = Open(filename);

	char c = 'a';
	int count = Read(&c,1,fd);
	while(count > 0) {
		Write(&c,1,1);
		count = Read(&c,1,fd);
	}

	Close(fd);
	return count;
}