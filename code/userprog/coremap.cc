#include "coremap.hh"
#include "../lib/assert.hh"
#include <random>
#include "threads/thread.hh"
#include <string.h>
#include <system.hh>


CoreMap::CoreMap(unsigned numPhysPages) {
	arr = new CoreMapEntry[numPhysPages];
	size = numPhysPages;
	victimIdx = size-1;
}

CoreMap::~CoreMap() {
	delete []arr;
}

uint32_t
CoreMap::PhysAdrrToIdx(uint32_t physAddr) {
	return physAddr / PAGE_SIZE;
}

uint32_t
CoreMap::IdxToPhysAddr(uint32_t idx) {
	return idx * PAGE_SIZE;
}

unsigned
CoreMap::FindPage(Thread* owner, uint32_t _vpn) {
	
	mMapLock->Acquire();
	unsigned i = 0;
	
	// Buscamos una pagina que este libre y NO este pinneada 
	while (i < size && !arr[i].isFree) {
		i++;
	}
	unsigned idx = i;
	
	if (i == size) {
		do {
			idx = PickVictim();
		} while (arr[idx].isPinned);
		PinPage(idx);
		mMapLock->Release();
		SendToSwap(idx);
		mMapLock->Acquire();
	} 
	else {
		PinPage(idx);
	}
	arr[idx].isFree = 0;
	arr[idx].owner = owner;
	arr[idx].vpn = _vpn;
	DEBUG('A', "Physical page number: %d\n", idx);
	mMapLock->Release();
	return idx;
}

void
CoreMap::FreePage(uint32_t physAddrs) {
	memset(&machine->mainMemory[physAddrs * PAGE_SIZE], 0, PAGE_SIZE);
	arr[physAddrs].isFree = 1;
	arr[physAddrs].owner = nullptr;
	arr[physAddrs].vpn = -1;
	arr[physAddrs].isPinned = 0;
}

void
CoreMap::PinPage(uint32_t physAddrs) {
	arr[physAddrs].isPinned = true;
}

void
CoreMap::UnPinPage(uint32_t physAddrs) {
	arr[physAddrs].isPinned = false;
}

unsigned
CoreMap::PickVictim(){
#ifdef PRPOLICY_FIFO

	victimIdx = (victimIdx+1) % size;
	return victimIdx;

#endif
#ifdef PRPOLICY_CLOCK

	unsigned ogIdx;
	victimIdx = (victimIdx+1) % size;
	if (victimIdx == 0){
		ogIdx = size;
	}
	else{
		ogIdx = victimIdx;
	}
	
	//CASO (0,0)
	
	for(;(ogIdx-1) != victimIdx; victimIdx = (victimIdx+1) % size){
		Thread* own = arr[victimIdx].owner;
		unsigned idx = arr[victimIdx].vpn;
		//NO SE QUE TAN BIEN ESTEN LOS INDICES DE ES -victimIdx en la Tabla del Thread-
		if(!own->space->GetPageTable()[idx].use && !own->space->GetPageTable()[idx].dirty){
			return victimIdx;
		}
	}

	victimIdx = (victimIdx+1) % size;

	// CASO (0,1) 
	
	for(;(ogIdx-1) != victimIdx ; victimIdx = (victimIdx+1) % size){
		Thread* own = arr[victimIdx].owner;
		unsigned idx = arr[victimIdx].vpn;
		if (!own->space->GetPageTable()[idx].use && own->space->GetPageTable()[idx].dirty){
			return victimIdx;
		}
		else {
			own->space->GetPageTable()[idx].use = false;
		}
	}
	
	victimIdx = (victimIdx+1) % size;

	//CASO (1,0)

	for(;ogIdx-1 != victimIdx ; victimIdx = (victimIdx +1) % size){
		Thread* own = arr[victimIdx].owner;
		unsigned idx = arr[victimIdx].vpn;
		if (!own->space->GetPageTable()[idx].dirty){
			return victimIdx;
		}
	}

	victimIdx = (victimIdx +1) % size;

	// CASO (1,1)

	for(;;victimIdx = (victimIdx +1) % size){
		Thread* own = arr[victimIdx].owner;
		unsigned idx = arr[victimIdx].vpn;
		if(own->space->GetPageTable()[idx].dirty){
			return victimIdx;
		}
	}
	//Este caso NO DEBERIA PASAR : )
	ASSERT(false);
#else 
return rand() % size;
#endif
}

CoreMapEntry*
CoreMap::GetPage(unsigned idx) {
	ASSERT(idx < size);
	return arr +idx;
}

unsigned
CoreMap::GetFreePages() {
	unsigned cantFree = 0;
	for (unsigned i = 0 ; i < size; i++) {
		if (arr[i].isFree) {
			cantFree++;
		}
	}
	return cantFree;	
}

/*

La idea de esta funcion es chequear que en caso de que tengamos que mandar 
a swap una pagina fisica y esta este siendo utilizada en la tlb
guarde el estado de la pagina y la invalide antes de swapearla

*/
static
void CheckTLB(unsigned pfn, Thread* owner) {
	TranslationEntry* tlb = machine->GetMMU()->tlb;
	for (unsigned i = 0 ; i < TLB_SIZE; i++) {

		if(tlb[i].valid && tlb[i].physicalPage == pfn) {
			uint32_t vpn = tlb[i].virtualPage;
			if (vpn < owner->space->GetNumberPages() ) {
				owner->space->GetPageTable()[vpn].dirty = tlb[i].dirty;
				owner->space->GetPageTable()[vpn].use   = tlb[i].use;
			}
		
			tlb[i].valid = false;
		}
	}
}



/* Enviar la pagina numero pfn a swap*/
void
CoreMap::SendToSwap(unsigned pfn) {
	
    ASSERT(arr[pfn].owner != nullptr);
		ASSERT(arr[pfn].vpn != (uint32_t)-1);
		ASSERT(arr[pfn].vpn < arr[pfn].owner->space->GetNumberPages());
    // Copiamos los datos y accedemos a la memoria fisica y al owner
    Thread* owner = arr[pfn].owner;
		TranslationEntry* pageTable = owner->space->GetPageTable();
		uint32_t vpn = arr[pfn].vpn;
		

		if (owner == nullptr) return; // Esto no deberia pasar

    if (vpn >= owner->space->GetNumberPages()) {
        DEBUG('e', "ERROR: Intento de SwapOut de VPN inválida %u en marco %u\n", vpn, pfn);
        arr[pfn].isFree = true; // Liberamos el marco corrupto
        return;
    }
				
		owner->space->InSwap(vpn);
		pageTable[vpn].valid = false;
		pageTable[vpn].physicalPage = (unsigned)-1;
    CheckTLB(pfn,owner);

    // Obtenemos el archivo de swap
    OpenFile* swapFile = owner->space->GetSwapFile();
    
    char buff[PAGE_SIZE];
    uint32_t physAddr = IdxToPhysAddr(pfn);
    // Copiamos los datos de memoria principal a un buffer y escribimos eso en el
    // sector correspondiente del archivo
    memcpy(buff,&machine->mainMemory[physAddr],PAGE_SIZE);
		swapFile->WriteAt(buff, PAGE_SIZE, PAGE_SIZE * vpn);

    stats->numSwapOuts++;
}




void 
CoreMap::UpdateCoreMap(uint32_t vpn,unsigned idx){
	arr[idx].owner = currentThread;
	arr[idx].vpn = vpn;
}