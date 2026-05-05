// process_data.h
#ifndef PROCESS_DATA_H
#define PROCESS_DATA_H

#define FILE_NAME_MAX_LEN 9

struct _processData {
    int pid;
    int status; 
    char name[FILE_NAME_MAX_LEN + 3];
};

#endif