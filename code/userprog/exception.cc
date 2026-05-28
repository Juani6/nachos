/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "args.hh"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
void
ExecProcess(void* arg) {
    
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();
    if(arg){
        char** argv = (char**)arg;
        int args = WriteArgs(argv);
        int sp = machine->ReadRegister(STACK_REG);
        machine->WriteRegister(STACK_REG,sp-24);
        machine->WriteRegister(4,args);
        machine->WriteRegister(5,sp);
    }

    machine->Run();
}


static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT: {

            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;
        }
        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);
            if(fileSystem->Create(filename,1000)) 
                machine->WriteRegister(2,0);
            else
                machine->WriteRegister(2,-1);
            break;
        }
        case SC_OPEN: {

            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) { 
                DEBUG('e', "Error: address to filename string is null.\n");
            }
            
            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
            }
            
            OpenFile *f = fileSystem->Open(filename);
            if (f == nullptr) {
                machine->WriteRegister(2,SC_ERROR);
                break;
            }
            
            OpenFileId fd = currentThread->fdTable->Add(f);
            if (fd == -1){
                DEBUG('e', "Error: fdTable full\n");
            }

            machine->WriteRegister(2,(int) fd);
            break;
        }
        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);
            DEBUG('e', "`Close` requested for id %u.\n", fid);
            
            if (currentThread->fdTable->HasKey(fid))
                machine->WriteRegister(2,0);
            else { 
                machine->WriteRegister(2,SC_ERROR);
                break;
            }
            
            OpenFile *file = currentThread->fdTable->Remove(fid); 
            delete file;
            break;
        }
        case SC_REMOVE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null");
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
            }

            bool err = fileSystem->Remove(filename);
            DEBUG('e', "Intentando eliminar %s [%s] \n",filename, err ? "true" : "false");
            if (err) {
                machine->WriteRegister(2,0);
            }
            else {
                machine->WriteRegister(2,SC_ERROR);
            }
            break;
        }
        case SC_READ: {
            int buffAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFileId fid = machine->ReadRegister(6);

            char *auxBuff = new char[size]; 
            int readBytes;
            if (fid == CONSOLE_INPUT) {
                char c;
                readBytes = 0;
                do  {
                    c = synchConsole->GetChar();
                    auxBuff[readBytes] = c;
                    readBytes++;
                } while (readBytes < size && c != '\n');
                WriteBufferToUser(auxBuff, buffAddr, readBytes);
                delete[] auxBuff;
                machine->WriteRegister(2,readBytes);
                break;
            }



            OpenFile *f = currentThread->fdTable->Get((int) fid);
            if (f == nullptr) {
                DEBUG('e', "Fd invalido\n");
                delete []auxBuff;
                machine->WriteRegister(2,SC_ERROR);
                break;
            }
            if(!f){
                machine->WriteRegister(2,SC_ERROR);
                break;
            }

            readBytes = f->Read(auxBuff,size);
            DEBUG('e',"%d readed bytes\n",readBytes);
            
            if (readBytes > 0) {
                WriteBufferToUser(auxBuff, buffAddr, readBytes);
                break;
            } 
                
            delete[] auxBuff;
            machine->WriteRegister(2, readBytes);

            break;
        }
        case SC_WRITE: {
            int buffAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFileId fid = machine->ReadRegister(6);
            
            char *auxBuff = new char[size]; 
            int writeBytes;
            if (fid == CONSOLE_OUTPUT) {
                char c;
                DEBUG('e', "Size = %d\n", size);
                ReadBufferFromUser(buffAddr, auxBuff, size);
                //DEBUG('e', "User buffer : %s\n",auxBuff);    
                for(writeBytes = 0; writeBytes < size; writeBytes++) {
                    c = auxBuff[writeBytes];
                    synchConsole->PutChar(c);
                }
                delete []auxBuff;
                machine->WriteRegister(2,writeBytes);
                break;
            }

            OpenFile *f = currentThread->fdTable->Get((int) fid);
            if (f == nullptr) {
                DEBUG('e', "Fd invalido\n");
                delete []auxBuff;
                machine->WriteRegister(2,SC_ERROR);
                break;
            }

            ReadBufferFromUser(buffAddr, auxBuff, size);
            writeBytes = f->Write(auxBuff,size);

            delete []auxBuff;
            machine->WriteRegister(2, writeBytes);

            break;
        }
        case SC_EXIT: {
            int exitStatus = machine->ReadRegister(4);
            currentThread->SetExitStatus(exitStatus);
            
            pTLock->Acquire();
            int alive = 0;
            for (int i = 0; i < Table<Thread*>::SIZE; i++) {
                if (processTable->Get(i) != nullptr) {
                    alive++;
                }
            }
            pTLock->Release();
            stats->Debug();
            if (alive > 1) {
                currentThread->Finish();
            }
            // Si no hay procesos activos terminamos
            else { 
                DEBUG('e', "No more processes in the scheduler. Halting.\n");
                interrupt->Halt();
            }
            break;
        }
        case SC_EXEC: {
            int filenameAddr = machine->ReadRegister(4);
            int argAddr      = machine->ReadRegister(5);
            DEBUG('e',"argAddr:%d\n",argAddr);
            char** argv;
            if(argAddr){
                argv = SaveArgs(argAddr);
            }else {argv = nullptr;}
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null");
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                break;
            }
            
            DEBUG('e', "Filename : %s\n", filename);
            OpenFile *executable = fileSystem->Open(filename);
            if (executable == nullptr) {
                machine->WriteRegister(2,SC_ERROR);
                break;
            }

            AddressSpace *space = new AddressSpace(executable);
            filename[FILE_NAME_MAX_LEN] = '\0';
            char* name = strdup(filename);
            Thread* newThread = new Thread(name,true);
            newThread->space = space;

            pTLock->Acquire();
            SpaceId pid = processTable->Add(newThread);
            newThread->SetPid(pid);
            pTLock->Release();
            machine->WriteRegister(2,pid);
           
            newThread->Fork(ExecProcess,(void*)argv);
            break;
        }
        case SC_JOIN: {
            int pid = machine->ReadRegister(4);
            if (pid < 0 || Table<Thread*>::SIZE < (unsigned)pid ) {
                DEBUG('e', "Pid invalido\n");
                machine->WriteRegister(2,SC_ERROR);
                break;
            }
            
            pTLock->Acquire();
            Thread* hijo = processTable->Get(pid);
            const char* sonName = hijo->GetName();
            pTLock->Release();
            if(!hijo){
                machine->WriteRegister(2,SC_ERROR);
                break;
            }
            
            if(!hijo->IsJoinable()) {
                fprintf(stderr, "Thread not joinable\n");
                machine->WriteRegister(2,SC_ERROR);
                break;
            }
            int exitStatus = hijo->Join();
            
            pTLock->Acquire();
            processTable->Remove(pid);
            free((char*)sonName);
            pTLock->Release();
            
            machine->WriteRegister(2,exitStatus);
            break;
        }
        case SC_GETPT: {
            int buffAdr = machine->ReadRegister(4);
            DEBUG('e', "Address : &d\n",buffAdr);
            int pf = writeProcessDataToUser(buffAdr);
            machine->WriteRegister(2,pf);
            break;
        }
        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}

char* GetSwapName(int pid) {
    char _pid[3];
    sprintf(_pid, "%d", pid);
    char _swapName [10] = "SWAP.";
    strcat(_swapName,_pid);
    char *swapName = strdup(_swapName);
    return swapName;
}


#ifdef SWAP
// Retorna el indice del coremap que se libero
static
unsigned SendToSwap(TranslationEntry table) {
    // accedemos a la vpn
    // con el coremap accedemos al dueño
    // en el dueño actualizamos los dirty y use
    // escribimos el dato en swap
    
    // Copiamos los datos y accedemos a la memoria fisica y al owner
    uint32_t vpn = table.virtualPage;
    uint32_t physAddr = table.physicalPage;
    unsigned idx = coreMap->PhysAdrrToIdx(physAddr); 
    Thread* owner = coreMap->GetPage(idx)->owner;
    
    owner->space->GetPageTable()[vpn].dirty = table.dirty;
    owner->space->GetPageTable()[vpn].use = table.use; 
    owner->space->shadowTable[vpn].isInSwap = true;
    
    // Obtenemos el nombre del swap
    char* name = GetSwapName(owner->GetPid());
    OpenFile* swapFile = fileSystem->Open(name);
    free(name);
    char buff[PAGE_SIZE];
    // Copiamos los datos de memoria principal a un buffer y escribimos eso en el
    // sector correspondiente del archivo
    memcpy(buff,&machine->mainMemory[physAddr * PAGE_SIZE],PAGE_SIZE);
    swapFile->WriteAt(buff,PAGE_SIZE,PAGE_SIZE*vpn);
    return idx;
}
#endif



static void
PageFaultHandler(ExceptionType _et) {
    unsigned badAddrs = (unsigned) machine->ReadRegister(BAD_VADDR_REG);
    unsigned vpn = (unsigned) badAddrs / PAGE_SIZE;

    static unsigned tlb_index = 0;
    stats->numPageFaults++;
    DEBUG('a', "valid state: %d",currentThread->space->GetPageTable()[vpn].valid);
    if (currentThread->space->GetPageTable()[vpn].physicalPage == -1) {
        TranslationEntry page;
        #ifdef SWAP
        if(currentThread->space->shadowTable[vpn].isInSwap) {
            
            char *swapName = GetSwapName(currentThread->GetPid());
            DEBUG('e', "SWAPNAME : %s\n", swapName);
            OpenFile* fd = fileSystem->Open(swapName);
            free(swapName);
            

            char buff [PAGE_SIZE];
            fd->ReadAt(buff,PAGE_SIZE,PAGE_SIZE*vpn);
            // Mandamos la pagina en el indice correspondiente a su swap
            // Actualiza tabla de procesos del hilo dueño
            unsigned idx = SendToSwap(machine->GetMMU()->tlb[tlb_index]); 
            // Actualizamos el coreMap 
            coreMap->GetPage(idx)->owner = currentThread;
            coreMap->GetPage(idx)->vaddrs = vpn;
            memcpy(&machine->mainMemory[idx*PAGE_SIZE], buff, PAGE_SIZE);
        }
        else{
            currentThread->space->LoadPage(vpn);
        }
        #else
        currentThread->space->LoadPage(vpn);
        #endif
        page = currentThread->space->GetPageTable()[vpn];
        machine->GetMMU()->tlb[tlb_index] = page;
        
    }
    tlb_index = (tlb_index + 1) % TLB_SIZE;
}



/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
