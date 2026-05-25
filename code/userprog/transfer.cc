/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"
#include "process_data.h"

void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    unsigned tries;
    do {
        tries = 0;
        int temp;
        count++;
        do {
            tries++;
        } while (machine->ReadMem(userAddress,1,&temp) == false && tries < TLB_TRIES );
        userAddress++;
        *outBuffer = (unsigned char) temp;
        outBuffer++; 
    } while (count < byteCount);
}

bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    unsigned tries;
    do {
        tries = 0;
        int temp;
        count++;
        
        do {
            tries++;
        } while (machine->ReadMem(userAddress,1,&temp) == false && tries < TLB_TRIES );
        userAddress++;
        *outString = (unsigned char) temp;
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount)
{
    ASSERT(buffer != nullptr);
    ASSERT(userAddress != 0);
    ASSERT(byteCount != 0);

    int temp;
    unsigned count = 0;
    unsigned tries;
    do {
        tries = 0;
        temp = *buffer;
        do {
            tries++;
        } while (machine->WriteMem(userAddress,1,temp) == false && tries < TLB_TRIES);
        userAddress++;
        buffer++;
        count++;
    } while (count < byteCount);

}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(string != nullptr);
    ASSERT(userAddress != 0);
    int temp;
    unsigned tries;
    do {
        tries = 0;
        temp = *string;
        
        do {
            tries++;
        } while (tries < TLB_TRIES && machine->WriteMem(userAddress,1,temp) == false);
        userAddress++;
    } while (*string++ != '\0');
}

int writeProcessDataToUser(int userAddress) {
    ASSERT(userAddress != 0);
    
    struct _processData* temp = new struct _processData;
    Thread * aux;
    int foundProcesses = 0;
    unsigned size = sizeof(struct _processData);
    int actualUserAddress;
    int j;
    unsigned tries;
    for (unsigned i = 0; i < Table<Thread*>::SIZE; i++) {
        aux = processTable->Get(i);
        if (aux == nullptr) {
            continue;
        }
        actualUserAddress = userAddress + foundProcesses * size;
        
        temp->pid    = i;
        temp->status = (int) aux->GetStatus();
        const char* name = aux->GetName();
        
        // Limpiamos el nombre de la estructura auxiliar
        /* for (int k = 0; k < FILE_NAME_MAX_LEN + 1; k++) {
            temp->name[k] = '\0';
        } */
        // Copiamos el nombre del hilo
        if (name != nullptr) {
            for(j = 0; name[j] != '\0' && j < FILE_NAME_MAX_LEN; j++) {
                temp->name[j] = name[j]; 
            }
        temp->name[j] = '\0';
        }
        for (unsigned k = 0; k < size; k++) {
            tries = 0;
            do {
                tries++;
            } while ((machine->WriteMem(actualUserAddress + k, 1, ((char*)temp)[k])) == false && tries < TLB_TRIES);

        }
        
        foundProcesses++;
        }
    delete temp;
    return foundProcesses;
}