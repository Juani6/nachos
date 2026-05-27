#include "coremap.hh"
#include "../lib/assert.hh"
#include <random>

extern Thread* currentThread;

CoreMap::CoreMap(unsigned numPhysPages) {
	arr = new CoreMapEntry[numPhysPages];
	size = numPhysPages;
}

CoreMap::~CoreMap() {
	delete []arr;
}

uint32_t
CoreMap::PhysAdrrToIdx(uint32_t physAddr) {
	return physAddr / size;
}

uint32_t
CoreMap::IdxToPhysAddr(uint32_t idx) {
	return idx * size;
}

unsigned
CoreMap::FindPage(Thread* owner, uint32_t vAddrs) {
	
	unsigned i = 0;

	// Buscamos una pagina que este libre y NO este pinneada 
	while (!arr[i].isFree && !arr[i].isPinned && i < size) {
		i++;
	}
	unsigned idx = i;

	if (i == size) {
		// to do
	} 
	
	arr[idx].isFree = 0;
	arr[idx].isPinned = 0;
	arr[idx].owner = currentThread;
	arr[idx].vaddrs = vAddrs;

	return idx;
}

void
CoreMap::FreePage(uint32_t physAddrs) {
	unsigned idx = PhysAdrrToIdx(physAddrs);
	arr[idx].isFree = 1;
	arr[idx].owner = nullptr;
	arr[idx].vaddrs = -1;
	arr[idx].isPinned = 0;
}

void
CoreMap::PinPage(uint32_t physAddrs) {
	unsigned idx = PhysAdrrToIdx(physAddrs);
	arr[idx].isPinned = true;
}

void
CoreMap::UnPinPage(uint32_t physAddrs) {
	unsigned idx = PhysAdrrToIdx(physAddrs);
	arr[idx].isPinned = false;
}

int
CoreMap::PickVictim(){
	return rand() % size;
	//to do
}
