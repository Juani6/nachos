#include "coremap.hh"
#include "../lib/assert.hh"
#include <random>
#include "threads/thread.hh"
#include <string.h>
#include <system.hh>


CoreMap::CoreMap(unsigned numPhysPages) {
	arr = new CoreMapEntry[numPhysPages];
	size = numPhysPages;
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
	
	unsigned i = 0;

	// Buscamos una pagina que este libre y NO este pinneada 
	while (i < size && !arr[i].isFree) {
		i++;
	}
	unsigned idx = i;
	DEBUG('a', "idx : %d\n",idx);
	
	PinPage(idx);
	if (i == size) {
		UnPinPage(idx);
		do {
			idx = PickVictim();
			DEBUG('a', "Picking victim...\n");
		} while (arr[idx].isPinned);
		PinPage(idx);
		DEBUG('a',"Enviando la pagina %u a swap\n",idx);
		SendToSwap(idx);
	} 
	
	arr[idx].isFree = 0;
	arr[idx].isPinned = 0;
	arr[idx].owner = owner;
	arr[idx].vpn = _vpn;
	UnPinPage(idx);
	return idx;
}

void
CoreMap::FreePage(uint32_t physAddrs) {
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

int
CoreMap::PickVictim(){
	return rand() % size;
	//to do
}

CoreMapEntry*
CoreMap::GetPage(unsigned idx) {
	ASSERT(idx < size);
	return arr +idx;
}

/*

La idea de esta funcion es chequear que en caso de que tengamos que mandar 
a swap una pagina fisica y esta este siendo utilizada en la tlb
guarde el estado de la pagina y la invalide antes de swapearla

*/
static
void CheckTLB(unsigned pfn) {
	TranslationEntry* tlb = machine->GetMMU()->tlb;
	unsigned i = 0;
	bool found = false;
	while(!found && i < TLB_SIZE) {

		if(tlb[i].valid && tlb[i].physicalPage == pfn) {
			uint32_t vpn = tlb[i].virtualPage;
			currentThread->space->GetPageTable()[vpn].dirty = tlb[i].dirty;
			currentThread->space->GetPageTable()[vpn].use   = tlb[i].use;
		
			tlb[i].valid = false;
			found = true;
		}
		i++;
	}
}



/* Enviar la pagina numero pfn a swap*/
void
CoreMap::SendToSwap(unsigned pfn) {
    
    // Copiamos los datos y accedemos a la memoria fisica y al owner
    Thread* owner = arr[pfn].owner;
		TranslationEntry* pageTable = owner->space->GetPageTable();
		uint32_t vpn = arr[pfn].vpn;
		
    CheckTLB(pfn);

		owner->space->shadowTable[vpn].isInSwap = true;
    pageTable[vpn].valid = false;

    // Obtenemos el archivo de swap
    OpenFile* swapFile = owner->space->GetSwapFile();
    
    char buff[PAGE_SIZE];
    uint32_t physAddr = IdxToPhysAddr(pfn);
    // Copiamos los datos de memoria principal a un buffer y escribimos eso en el
    // sector correspondiente del archivo
    memcpy(buff,&machine->mainMemory[physAddr],PAGE_SIZE);
    swapFile->WriteAt(buff,PAGE_SIZE,PAGE_SIZE*vpn);
}




void 
CoreMap::UpdateCoreMap(uint32_t vpn,unsigned idx){
	arr[idx].owner = currentThread;
	arr[idx].vpn = vpn;
}