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
#include <stdio.h>



void 
AddressSpace::InitSwapFile() {
    shadowTable = new ShadowTable[numPages];
    
    char *swapName = CreateSwapName();
    ASSERT(fileSystem->Create(swapName, numPages * PAGE_SIZE));
    swapFile = fileSystem->Open(swapName);
    free(swapName);
    /* if (numPages <= coreMap->GetFreePages()) {
        fprintf(stderr, "NO HAY PAGINAS\n");
    } */
}

void
AddressSpace::InitPageTableOnDemand() {

    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
       
        pageTable[i].physicalPage  = -1;
        pageTable[i].valid        = false;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
#ifdef SWAP
        NotInSwap(i);
        shadowTable[i].vpn = i;
#endif
    }

}

void
AddressSpace:: InitPageTableNaive() {

// First, set up the translation.
    int pfn;
    
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;

        mMapLock->Acquire();
#ifdef SWAP
        pfn = coreMap->FindPage(owner,i);
        DEBUG('A', "Physical page number: %d\n", pfn);
        
        ASSERT(pfn != -1);
        
        pageTable[i].physicalPage = (unsigned) pfn;
        coreMap->UnPinPage(pfn);

        NotInSwap(i);
        shadowTable[i].vpn = i;    
#else
        pfn = memoryMap->Find();
        ASSERT(pfn != -1);
        pageTable[i].physicalPage = (unsigned) pfn;
#endif
        mMapLock->Release();

        pageTable[i].valid        = false;
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
        if (pageTable[i].physicalPage != (unsigned)-1) {
            DEBUG('A', "Limpiando página física: %u\n", pageTable[i].physicalPage);
            memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
        }
    }
}

void
AddressSpace::InitLoadSegments() {
    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t initDataSize = exe->GetInitDataSize();
    
    if (codeSize > 0) {
        uint32_t virtualAddr = exe->GetCodeAddr();
        DEBUG('A', "Initializing code segment, at 0x%X, size %u\n", virtualAddr, codeSize);
        ExeRead(virtualAddr,codeSize,pageTable,exe,CODE, owner);
    }
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe->GetInitDataAddr();
        DEBUG('A', "Initializing data segment, at 0x%X, size %u\n",virtualAddr, initDataSize);
        ExeRead(virtualAddr,initDataSize,pageTable,exe,DATA, owner);           
    }
    for (unsigned i = 0; i < numPages; i++) {
        if (pageTable[i].physicalPage != (unsigned) -1) {
            pageTable[i].valid = true;
        }
    }
}



/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file,unsigned _pid, Thread* _owner)
{
    pid = _pid;
    _executable_file = executable_file;
    ASSERT(executable_file != nullptr);
    
    exe = new Executable(executable_file);
    ASSERT(exe->CheckMagic());
    
    owner = _owner == nullptr ? currentThread : _owner;
    owner->space = this;
    unsigned size = exe->GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;
    pageTable = new TranslationEntry[numPages];
    DEBUG('A', "Initializing address space, num pages %u, size %u\n",numPages, size);
    
#ifdef SWAP
    InitSwapFile();
#else
    ASSERT(numPages <= memoryMap->CountClear());    
#endif

#ifdef DEMAND_LOADING // ifdef DEMANG LOADING 
    InitPageTableOnDemand();
#else 
    InitPageTableNaive();
    InitLoadSegments();
#endif
}



/*
Funcion diseñada para cargar paginas on demand
*/
void
AddressSpace::LoadPage(unsigned vpn) {
    DEBUG('A', ">>> ENTRANDO A LOADPAGE CON VPN: %u <<<\n", vpn);
    stats->numDemand++;
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t dataSize = exe->GetInitDataSize();

    mMapLock->Acquire();
#ifdef SWAP
    int pfn = coreMap->FindPage(owner,vpn);
    ASSERT(pfn != -1);
    pageTable[vpn].physicalPage = (unsigned) pfn;
    coreMap->UnPinPage(pfn);
#else
    int pfn = memoryMap->Find();
    ASSERT(pfn != -1);
    pageTable[vpn].physicalPage = (unsigned) pfn;
#endif
    mMapLock->Release();

    unsigned segOffset = vpn * PAGE_SIZE;
    uint32_t tamBinario = codeSize + dataSize;
    
    memset(&machine->mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
    
    if (segOffset >= tamBinario) { // (vpn * PAGE_SIZE > codeSize + dataSize) 
        #ifdef SWAP
        coreMap->UnPinPage(pfn);
        #endif
        
        pageTable[vpn].valid = true;
        return;
    }
    // Si estamos en espacio de TEXT
    if (codeSize > 0 && segOffset < codeSize) {
        uint32_t codeRemaining = codeSize - segOffset;
        uint32_t codeToRead = (uint32_t)PAGE_SIZE < codeRemaining ? PAGE_SIZE : codeRemaining;
        uint32_t physAddr = pageTable[vpn].physicalPage * PAGE_SIZE;
        if (codeToRead > 0 && segOffset < codeSize) {
            exe->ReadCodeBlock(&machine->mainMemory[physAddr],codeToRead,segOffset);
        }
    }
    // Existe el data, se encuentra en el binario y en particular sobre el data
    if (dataSize > 0 && segOffset < tamBinario && segOffset + PAGE_SIZE > codeSize) {
        // si la pagina es hibrida, es decir tiene data y codigo calculamos donde arranca el codigo
        uint32_t dataOffsetPagina = segOffset < codeSize ? codeSize - segOffset : 0;
        // si la pagina es data pura calculamos el offset
        uint32_t dataFileOffset = segOffset > codeSize ? segOffset - codeSize : 0;
        
        
        uint32_t dataRemaining = dataSize - dataFileOffset; // Cuanto queda por leer
        
        uint32_t availableSpace = PAGE_SIZE - dataOffsetPagina; // Espacio disponible en la pagina
        uint32_t dataToRead = availableSpace < dataRemaining ? availableSpace : dataRemaining;
        
        uint32_t physAddr = pageTable[vpn].physicalPage * PAGE_SIZE + dataOffsetPagina;
        if (dataToRead > 0 && dataFileOffset < dataSize) {
            exe->ReadDataBlock(&machine->mainMemory[physAddr],dataToRead,dataFileOffset);
        }
    
    }        
    #ifdef SWAP
    coreMap->UnPinPage(pfn);
    #endif
    
    pageTable[vpn].valid = true;
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{   
    unsigned fpn;
    for (unsigned i = 0; i < numPages; i++) {
        fpn = pageTable[i].physicalPage;
        if (fpn != (unsigned)-1) {
            #ifdef SWAP
            if (coreMap->GetPage(fpn)->owner == owner) {
                //mMapLock->Acquire();
                coreMap->FreePage( (uint32_t) fpn);
                //mMapLock->Release();

            }
            #else
            memoryMap->Clear(fpn);
            #endif
        }
    }
    #ifdef SWAP
    char * swapName = CreateSwapName();
    delete swapFile;
    fileSystem->Remove(swapName);
    free(swapName);
    delete []shadowTable;
    #endif 
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
    DEBUG('A', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

unsigned
AddressSpace::GetNumberPages() {
    return numPages;
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState(){}

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

void
AddressSpace::InSwap(unsigned vpn) {
    shadowTable[vpn].isInSwap = true;
}


void
AddressSpace::NotInSwap(unsigned vpn) {
    shadowTable[vpn].isInSwap = false;
}

bool
AddressSpace::IsInSwap(unsigned vpn) {
    return shadowTable[vpn].isInSwap;
}

TranslationEntry*
AddressSpace::GetPageTable() {
    return pageTable;
}

char*
AddressSpace::CreateSwapName() {
    char _swapName [32];
    snprintf(_swapName, sizeof(_swapName),"SWAP.%d", pid);
    char *swapName = strdup(_swapName);

    return swapName;
}

OpenFile*
AddressSpace::GetSwapFile() {
    return swapFile;
}



// Setea los bloques de Data o Code previo a ejecutar un proceso
// de manera naive (sin DEMAND_LOADING)
void ExeRead(uint32_t virtualAddr, uint32_t size,TranslationEntry* pageTable,Executable* exe,exeRead data,Thread* owner) {
    
    // Este ASSERT es por las dudas
    ASSERT(data == DATA || data == CODE);
    ASSERT(size != 0);
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
        #ifdef SWAP
        unsigned pfn =  pageTable[vpn].physicalPage;
        if (pfn == (unsigned)-1) {
            mMapLock->Acquire();
            pfn = coreMap->FindPage(owner,vpn);
            mMapLock->Release();
            pageTable[vpn].physicalPage = pfn;
        }
        else {
            coreMap->PinPage(pfn);
        }
        #endif
            physAddr = pageTable[vpn].physicalPage * PAGE_SIZE + offset;
        
        DEBUG('A', "Accediendo %u %u\n",vpn, offset);

        DEBUG('A', "ReadBlock: bytesRe|ad=%u sizeToRead=%u size=%u\n", bytesRead, sizeToRead, size);
        if (data == CODE)
            exe->ReadCodeBlock(&mainMemory[physAddr], sizeToRead,bytesRead);
        if (data == DATA)
            exe->ReadDataBlock(&mainMemory[physAddr], sizeToRead,bytesRead);
        bytesRead += sizeToRead;
        #ifdef SWAP
            coreMap->UnPinPage(pfn);
        #endif
        }
}

