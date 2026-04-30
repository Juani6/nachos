#include "../userprog/syscall.h"
#include "lib.h"

int main(int argc,char* argv[]) {
	if (argc < 2) {
		return -1;
	}
	char *filename = argv[1];
	unsigned len = strlen(filename);
	putInt(len);
	char strFilename[len+1];
	strCopy(filename,strFilename);
	strFilename[len+1] = '\0';
	puts2(strFilename);
	
	int err = Remove(strFilename);

	return err;
}