#include "../userprog/syscall.h"
#include "lib.h"

int main(int argc, char* argv[]) {
if (argc < 2)
		return -1;
	char* source = argv[1];
	char* destiny = argv[2];

	OpenFileId fdSource = Open(source);
	OpenFileId fdDestiny;

	fdDestiny = Open(destiny);
	if (fdDestiny == -1) {
		int archivo = Create(destiny);
		fdDestiny = Open(destiny);
	}

	char c;
	int bytesRead = Read(&c,1,fdSource);
	int bytesWritten = 0;
	char b[4];
	do {
		bytesWritten = Write(&c,1,fdDestiny);
		bytesRead = Read(&c,1,fdSource);
	} while(bytesRead > 0);

	Close(fdSource);
	Close(fdDestiny);
	return bytesRead;

}