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

static void syscall_SC_HALT() {
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();

}

static void syscall_SC_CREATE() {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
            machine->WriteRegister(2,SC_ERROR);
            return;
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);
            if(fileSystem->Create(filename,1000)) 
                machine->WriteRegister(2,0);
            else
                machine->WriteRegister(2,SC_ERROR);
}

static void syscall_SC_OPEN() {

            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) { 
                DEBUG('e', "Error: address to filename string is null.\n");
                return;
            }
            
            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2,SC_ERROR);
                return;
            }
            
            OpenFile *f = fileSystem->Open(filename);
            if (f == nullptr) {
                machine->WriteRegister(2,SC_ERROR);
                return;
            }
            
            OpenFileId fd = currentThread->fdTable->Add(f);
            if (fd == -1){
                DEBUG('e', "Error: fdTable full\n");
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            machine->WriteRegister(2,(int) fd);

}

static void syscall_SC_CLOSE() {
    int fid = machine->ReadRegister(4);
            DEBUG('e', "`Close` requested for id %u.\n", fid);
            
            if (!currentThread->fdTable->HasKey(fid)) {
                machine->WriteRegister(2,SC_ERROR);
                return;
            }
            else { 
                machine->WriteRegister(2,0);
            }
            
            OpenFile *file = currentThread->fdTable->Remove(fid); 
            delete file;
}

static void syscall_SC_REMOVE() {
     int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null");
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            bool err = fileSystem->Remove(filename);
            DEBUG('e', "Intentando eliminar %s [%s] \n",filename, err ? "true" : "false");
            if (err) {
                machine->WriteRegister(2,0);
            }
            else {
                machine->WriteRegister(2,SC_ERROR);
            }
}

static void syscall_SC_READ() {
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
                return;
            }



            OpenFile *f = currentThread->fdTable->Get((int) fid);
            if (f == nullptr) {
                DEBUG('e', "Fd invalido\n");
                delete []auxBuff;
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            readBytes = f->Read(auxBuff,size);
            DEBUG('e',"%d readed bytes\n",readBytes);
            
            if (readBytes > 0) {
                WriteBufferToUser(auxBuff, buffAddr, readBytes);
                return;
            } 
                
            delete[] auxBuff;
            machine->WriteRegister(2, readBytes);

}

static void syscall_SC_WRITE() {
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
        return;
    }

    OpenFile *f = currentThread->fdTable->Get((int) fid);
    if (f == nullptr) {
        DEBUG('e', "Fd invalido\n");
        delete []auxBuff;
        machine->WriteRegister(2,SC_ERROR);
        return;
    }

    ReadBufferFromUser(buffAddr, auxBuff, size);
    writeBytes = f->Write(auxBuff,size);
    delete []auxBuff;
    machine->WriteRegister(2, writeBytes);
}

static void syscall_SC_EXIT() {
    int exitStatus = machine->ReadRegister(4);
    currentThread->SetExitStatus(exitStatus);
    
    pTLock->Acquire();
    int alive = 0;
    for (unsigned i = 0; i < Table<Thread*>::SIZE; i++) {
        if (processTable->Get((unsigned)i) != nullptr) {
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
}

static void syscall_SC_EXEC() {

            int filenameAddr = machine->ReadRegister(4);
            int argAddr      = machine->ReadRegister(5);
            DEBUG('e',"argAddr:%d\n",argAddr);
            char** argv;
            if (argAddr) {
                argv = SaveArgs(argAddr);
            } 
            else {
                argv = nullptr;
            }
            
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null");
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                return;
            }
            
            DEBUG('e', "Filename : %s\n", filename);
            OpenFile *executable = fileSystem->Open(filename);
            if (executable == nullptr) {
                machine->WriteRegister(2,SC_ERROR);
                return;
            }

            filename[FILE_NAME_MAX_LEN] = '\0';
            char* name = strdup(filename);
            Thread* newThread = new Thread(name,true);
            
            pTLock->Acquire();
            SpaceId pid = processTable->Add(newThread);
            pTLock->Release();


            AddressSpace *space = new AddressSpace(executable,pid, newThread);
            newThread->space = space;
            
            machine->WriteRegister(2,pid);
           
            newThread->Fork(ExecProcess,(void*)argv);
}

static void syscall_SC_JOIN() {
    int pid = machine->ReadRegister(4);
            if (pid < 0 || Table<Thread*>::SIZE < (unsigned)pid ) {
                DEBUG('e', "Pid invalido\n");
                machine->WriteRegister(2,SC_ERROR);
                return;
            }
            
            pTLock->Acquire();
            Thread* hijo = processTable->Get(pid);
            pTLock->Release();
            if(!hijo){
                machine->WriteRegister(2,SC_ERROR);
                return;
            }
            const char* sonName = hijo->GetName();
            
            if(!hijo->IsJoinable()) {
                fprintf(stderr, "Thread not joinable\n");
                machine->WriteRegister(2,SC_ERROR);
                return;
            }
            int exitStatus = hijo->Join();
            
            pTLock->Acquire();
            processTable->Remove(pid);
            free((char*)sonName);
            pTLock->Release();
            
            machine->WriteRegister(2,exitStatus);
}


/*
El objetivo de esta syscall es proporcionar al userland 
la capacidad de acceder a una copia de la tabla de procesos 
*/
static void syscall_SC_GETPT() {
    int buffAdr = machine->ReadRegister(4);
    DEBUG('e', "Address : &d\n",buffAdr);
    int pf = writeProcessDataToUser(buffAdr);
    machine->WriteRegister(2,pf);
}

static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT: {
            syscall_SC_HALT();
            break;
        }
        case SC_CREATE: {
            syscall_SC_CREATE();
            break;
        }
        case SC_OPEN: {
            syscall_SC_OPEN();
            break;
        }
        case SC_CLOSE: {
            syscall_SC_CLOSE();
            break;
        }
        case SC_REMOVE: {
            syscall_SC_REMOVE();
            break;
        }
        case SC_READ: {
            syscall_SC_READ();
            break;
        }
        case SC_WRITE: {
            syscall_SC_WRITE();
            break;
        }
        case SC_EXIT: {
            syscall_SC_EXIT();
            break;
        }
        case SC_EXEC: {
            syscall_SC_EXEC();
            break;
        }
        case SC_JOIN: {
            syscall_SC_JOIN();
            break;
        }
        case SC_GETPT: {
            syscall_SC_GETPT();
            break;
        }
        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}

#ifdef SWAP
static void LoadFromSwap(Thread* owner, unsigned vpn) {
    if(owner->space->shadowTable[vpn].isInSwap) {
            stats->numSwapIn++;
            mMapLock->Acquire();
            unsigned fpn = coreMap->FindPage(owner, vpn);
            mMapLock->Release();
            owner->space->GetPageTable()[vpn].physicalPage = fpn;
            
            OpenFile* fd = owner->space->GetSwapFile();
            fd->ReadAt(&machine->mainMemory[coreMap->IdxToPhysAddr(fpn)],PAGE_SIZE,PAGE_SIZE*vpn);
            owner->space->GetPageTable()[vpn].valid = true;
            owner->space->GetPageTable()[vpn].dirty = false;
            owner->space->GetPageTable()[vpn].use   = true;
            owner->space->shadowTable[vpn].isInSwap = false;
            coreMap->UnPinPage(fpn);
        }
        #ifdef DEMAND_LOADING
        else{
            owner->space->LoadPage(vpn);
        } 
        #else 
        #endif
}
#endif

static void
PageFaultHandler(ExceptionType _et) {
    unsigned badAddrs = (unsigned) machine->ReadRegister(BAD_VADDR_REG);
    DEBUG('e', "Stack reg :%X\n", machine->ReadRegister(STACK_REG));
    DEBUG('e', "badaddrs %X\n", badAddrs);
    
    unsigned vpn = (unsigned) badAddrs / PAGE_SIZE;
    Thread* owner = currentThread;
    ASSERT(owner != nullptr);
    ASSERT(owner->space != nullptr);  // si esto falla, el thread terminó
    stats->numPageFaults++;
    
    
    static unsigned tlb_index = 0;

    DEBUG('e', "[VPN]: %d\n",vpn);
    DEBUG('e', "SwapIn: owner=%s vpn=%X\n",owner->GetName(), vpn);
    if (!owner->space->GetPageTable()[vpn].valid) {
        #ifdef SWAP
        LoadFromSwap(owner,vpn);
        #else
        owner->space->LoadPage(vpn);
        #endif
        
    }
    TranslationEntry page = owner->space->GetPageTable()[vpn];
    
    if (machine->GetMMU()->tlb[tlb_index].valid) {
        unsigned tlbPage = machine->GetMMU()->tlb[tlb_index].virtualPage;
        
        if (tlbPage < owner->space->GetNumberPages()) {
            owner->space->GetPageTable()[tlbPage].dirty = machine->GetMMU()->tlb[tlb_index].dirty;
            owner->space->GetPageTable()[tlbPage].use   = machine->GetMMU()->tlb[tlb_index].use;
    }
        machine->GetMMU()->tlb[tlb_index].valid = false;
    }
    
    page.valid = true;
    machine->GetMMU()->tlb[tlb_index] = page;
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
