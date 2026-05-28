/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "threads/system.hh"

#include <stdlib.h>
#include <string.h>

enum exeRead {DATA, CODE};
static void ExeRead(uint32_t virtualAddr,unsigned segOffset, uint32_t size,TranslationEntry* pageTable,Executable* exe,exeRead data);
/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    _executable_file = executable_file;
    ASSERT(executable_file != nullptr);
    
    exe = new Executable(executable_file);
    ASSERT(exe->CheckMagic());
    
    
    unsigned size = exe->GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;
    
    // How big is address space?
    #ifndef SWAP
    ASSERT(numPages <= memoryMap->CountClear());
    #endif
    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",numPages, size);
    
    #ifndef DEMAND_LOADING
    // First, set up the translation.
    int phisPage;
    pageTable = new TranslationEntry[numPages];
    #ifdef SWAP
    shadowTable = new ShadowTable[numPages];
    #endif
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        // For now, virtual page number = physical page number.
            mMapLock->Acquire();
#ifndef SWAP
        phisPage = memoryMap->Find();
        ASSERT(phisPage != -1);
        pageTable[i].physicalPage = (unsigned) phisPage;
#else
        phisPage = coreMap->FindPage(currentThread,i);
        DEBUG('a', "Physical page number: %d\n", phisPage);
        ASSERT(phisPage != -1);
        pageTable[i].physicalPage = (unsigned) phisPage;
        shadowTable[i].isInSwap = false;
        shadowTable[i].vpn = i;
#endif
    mMapLock->Release();

        pageTable[i].valid        = true;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        // If the code segment was entirely on a separate page, we could
        // set its pages to be read-only.
    }
    
    char *mainMemory = machine->mainMemory;
    
    // Zero out the entire address space, to zero the unitialized data
    // segment and the stack segment.
    for (unsigned i = 0; i < numPages; i++) {
        DEBUG('a', "%u\n", pageTable[i].physicalPage * PAGE_SIZE);
        memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
    }
    
    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t initDataSize = exe->GetInitDataSize();
    
    if (codeSize > 0) {
        uint32_t virtualAddr = exe->GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
            virtualAddr, codeSize);
            ExeRead(virtualAddr,0,codeSize,pageTable,exe,CODE);
        }
        if (initDataSize > 0) {
            uint32_t virtualAddr = exe->GetInitDataAddr();
            DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
                virtualAddr, initDataSize);
                ExeRead(virtualAddr,0,initDataSize,pageTable,exe,DATA);
                
            }
    #else

    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
       
       pageTable[i].physicalPage  = -1;
        pageTable[i].valid        = false;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
    }
    
    #endif
    DEBUG('a', "Constructor end: _exe=%p codeSize=%u\n", exe, exe->GetCodeSize());
}


void
AddressSpace::LoadPage(unsigned vpn) {
    DEBUG('a', ">>> ENTRANDO A LOADPAGE CON VPN: %u <<<\n", vpn);
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t dataSize = exe->GetInitDataSize();

    mMapLock->Acquire();
#ifndef SWAP
    int phisPage = memoryMap->Find();
    ASSERT(phisPage != -1);
    pageTable[vpn].physicalPage = (unsigned) phisPage;
#else
    int phisPage = coreMap->FindPage(currentThread,vpn);
    DEBUG('a', "Physical page number: %d\n", phisPage);
    ASSERT(phisPage != -1);
    pageTable[vpn].physicalPage = (unsigned) phisPage;
#endif
    mMapLock->Release();

    unsigned segOffset = vpn * PAGE_SIZE;
    uint32_t tamBinario = codeSize + dataSize;
    if (segOffset >= tamBinario) { // (vpn * PAGE_SIZE > codeSize + dataSize) 
        memset(&machine->mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
    }
    else {
        uint32_t bytesToReadFromDisk = PAGE_SIZE;
        if (segOffset + PAGE_SIZE > tamBinario) {
            // La pagina es hibrida de datos entre .Data y .BSS/Stack
            bytesToReadFromDisk = tamBinario - segOffset;
        }
        if (bytesToReadFromDisk < (uint32_t) PAGE_SIZE) {
            // Escribimos los ceros que faltan
            uint32_t restoStack = PAGE_SIZE - bytesToReadFromDisk;
            uint32_t physOffset = pageTable[vpn].physicalPage * PAGE_SIZE + bytesToReadFromDisk;
            memset(&machine->mainMemory[physOffset],0,restoStack);
        }
        
        // Si estamos en espacio de TEXT
        if (codeSize > 0 && segOffset < codeSize) {
            //uint32_t virtualAddr = exe->GetCodeAddr();

            uint32_t codeRemaining = codeSize - segOffset;
            uint32_t codeToRead = (uint32_t)PAGE_SIZE < codeRemaining ? PAGE_SIZE : codeRemaining;

            uint32_t physAddr = pageTable[vpn].physicalPage * PAGE_SIZE;
            if (codeToRead > 0 && segOffset < codeSize) {
                exe->ReadCodeBlock(&machine->mainMemory[physAddr],codeToRead,segOffset);
            }
        }
        // Existe el data, se encuentra en el binario y en particular sobre el data
        if (dataSize > 0 && segOffset < tamBinario && segOffset + PAGE_SIZE > codeSize) {
            //uint32_t virtualAddr = exe->GetInitDataAddr();
            // si la pagina es hibrida, es decir tiene data y codigo calculamos donde arranca el codigo
            uint32_t dataOffsetPagina = segOffset < codeSize ? codeSize - segOffset : 0;
            // si la pagina es data pura calculamos el offset
            uint32_t dataFileOffset = segOffset > codeSize ? segOffset - codeSize : 0;
            
            // Cuanto queda por leer
            uint32_t dataRemaining = dataSize - dataFileOffset;
            // Espacio disponible en la pagina
            uint32_t availableSpace = PAGE_SIZE - dataOffsetPagina;
            // Tomamos el mas chico entre lo que resta de pagina y los datos
            uint32_t dataToRead = availableSpace < dataRemaining ? availableSpace : dataRemaining;
            
            uint32_t physAddr = pageTable[vpn].physicalPage * PAGE_SIZE + dataOffsetPagina;
            if (dataToRead > 0 && dataFileOffset < dataSize) {
                exe->ReadDataBlock(&machine->mainMemory[physAddr],dataToRead,dataFileOffset);
            }

        
        }        
    }
    
    pageTable[vpn].valid = true;
}

//Setea los bloques de Data o Code previo a ejecutar un proceso
static void ExeRead(uint32_t virtualAddr,unsigned segOffset, uint32_t size,TranslationEntry* pageTable,Executable* exe,exeRead data) {
    
    // Este ASSERT es por las dudas
    ASSERT(data == DATA || data == CODE);

    char *mainMemory = machine->mainMemory;
    unsigned physAddr;
    uint32_t vpn;
    uint32_t offset;
    
    uint32_t bytesRead = 0;
    uint32_t spaceInPage = 0;
    uint32_t bytesRemaining = 0;
    uint32_t sizeToRead = 0;

    while (bytesRead < size) {
        vpn    = (unsigned) (virtualAddr + bytesRead) / PAGE_SIZE;
        offset = (unsigned) (virtualAddr + bytesRead) % PAGE_SIZE;
            
        bytesRemaining = size - bytesRead;

        spaceInPage = PAGE_SIZE - offset;

        // Si los bytes que restan son menos que el espacio en pagina escribo eso.
        sizeToRead = bytesRemaining < spaceInPage ? bytesRemaining : spaceInPage; 
            
        physAddr = pageTable[vpn].physicalPage * PAGE_SIZE + offset;
        DEBUG('a', "Accediendo %u %u\n",vpn, offset);

        DEBUG('a', "ReadBlock: segOffset=%u bytesRe|ad=%u sizeToRead=%u size=%u\n", segOffset, bytesRead, sizeToRead, size);
        if (data == CODE)
            exe->ReadCodeBlock(&mainMemory[physAddr], sizeToRead,segOffset+bytesRead);
        if (data == DATA)
            exe->ReadDataBlock(&mainMemory[physAddr], sizeToRead,segOffset+bytesRead);
        bytesRead += sizeToRead;
        }
}


/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{   
    for (unsigned i = 0; i < numPages; i++) {
        if (pageTable[i].physicalPage != (unsigned)-1) {
            #ifndef SWAP
            memoryMap->Clear(pageTable[i].physicalPage);
            #else
            coreMap->FreePage((uint32_t)pageTable[i].physicalPage);
            #endif
        }
    }
    delete [] pageTable;
    delete exe;
    delete _executable_file;
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    #ifndef USE_TLB
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
    #else
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        machine->GetMMU()->tlb[i].valid = false;
    }
    #endif
}

TranslationEntry*
AddressSpace::GetPageTable() {
    return pageTable;
}