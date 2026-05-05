#include "../userprog/syscall.h"
#include "../userprog/process_data.h"
#include "lib.h"

/// Thread state.
enum ThreadStatus {
    JUST_CREATED,
    RUNNING,
    READY,
    BLOCKED,
    NUM_THREAD_STATUS
};

void threadStatePrint(int i) {
	switch (i)
	{
	case JUST_CREATED:
		puts2("JUST_CREATED\n"); break;
		break;
	case RUNNING:
		puts2("RUNNING\n"); break;
	case READY:
		puts2("READY\n"); break;
	case BLOCKED:
		puts2("BLOCKED\n"); break;
	default:
		puts2("Estado invalido"); break;
		break;
	}
}

void processPrint(struct _processData* data) {
	if (data != NULL) {

		puts2("PID:"); putInt(data->pid);puts2("\t");
		if (strlen(data->name) > 0) {
			puts2("NAME:"); puts2(data->name);puts2("\t");
		}
		
		if (strlen(data->name) < 4) {
    	puts2("\t");
  	}
		puts2("STATUS:"); threadStatePrint(data->status);
	}
	else {
		puts2("hola no hay nada");
	}


}

int main(void) {
	struct _processData lista[20];
	int cantProcesos = getPT((char*)lista);
	
	 for(int i = 0; i < cantProcesos; i++) {
		if (&lista[i] != NULL)
			processPrint(&lista[i]);
	} 
	
	return 0;
}