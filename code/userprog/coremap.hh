#ifndef NACHOS_USERPROG_COREMAP__HH
#define NACHOS_USERPROG_COREMAP__HH

#include <stdint.h>
#include "../machine/mmu.hh"

class Thread;

typedef struct _coreMapEntry {
	
	struct Thread* owner;
	
	uint32_t vpn = -1;
	
	// Indica si la pagina esta libre
	bool isFree     = 1;

	// Indica si se puede mover de la memoria o no
	bool isPinned   = 0; 
	
} CoreMapEntry;

class CoreMap {
public:
	CoreMap(unsigned numPhysPages);

	~CoreMap();

	// Ocupa un frame libre
	// Si no existe deberia swapear
	// Devuelve su indice en la tabla
	unsigned FindPage(Thread* owner, uint32_t vpn);

	// Devuelve un numero de marco
	int PickVictim();

	// Marca una pagina como libre
	void FreePage(uint32_t physAddrs);

	// Funciones para sincronizar Syscalls
	// Tienen el objetivo de evitar que una pagina sea enviada
	// Al area de swap mientras accede a memoria
	void PinPage(uint32_t physAddrs);
	void UnPinPage(uint32_t physAddrs);
 
// Funciones para simplificar calculos

	uint32_t PhysAdrrToIdx(uint32_t physAddr);
	uint32_t IdxToPhysAddr(uint32_t idx);
	CoreMapEntry* GetPage(unsigned idx);
	
	void SendToSwap(unsigned pfn);
	void UpdateCoreMap(uint32_t vpn,unsigned idx);


	unsigned GetFreePages();
private:
	unsigned size;
	CoreMapEntry *arr;

};

#endif