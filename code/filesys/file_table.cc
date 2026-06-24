#include "./file_table.hh"
#include "../lib/utility.hh"
#include "../threads/lock.hh"

FileTable::FileTable(unsigned numDirEntries) {
	internalLock = new Lock("File table lock");
	size = numDirEntries;
	arr = new FileTableEntry[size];
	for (unsigned i = 0; i < size; i++) {
		arr[i].count         = 0;
		arr[i].inodeSector	 = -1;
		arr[i].iNodeLock     = nullptr;
		arr[i].deletePending = false;
		arr[i].inUse 				 = false;
	}
}

FileTable::~FileTable() {
	for (unsigned i = 0; i < size; i++) {
		if (arr[i].iNodeLock) {
			delete arr[i].iNodeLock;
		}
	}
	delete []arr;
	delete internalLock;
}

FileTableEntry*
FileTable::Acquire(int sectorInodo) {

	internalLock->Acquire();
	unsigned idx = FindEntry(sectorInodo);

	if (idx != (unsigned) -1) {
		// Si esta marcado para borrar no accedemos
		if (arr[idx].deletePending) {
			internalLock->Release();
			DEBUG('f', "Destruccion inminente... Abortando... \n"); // msj dramatico
			return nullptr;
		}
		// Caso contrario sumamos en uno los accesos
		arr[idx].count++;
		internalLock->Release();
		return &arr[idx];
	}

	for (unsigned k = 2; k < size; k++) {
		if (!arr[k].inUse) {
			arr[k].inodeSector	 = sectorInodo;
			arr[k].count         = 1;
			arr[k].iNodeLock     = new Lock("iNodeLock");
			arr[k].deletePending = false;
			arr[k].inUse 				 = true;
			internalLock->Release();
			return &arr[k];
		}
	}
	internalLock->Release();
	// Muchos archivos
	DEBUG('f', "Demasiados archivos abiertos...\n");
	return nullptr;
}

bool
FileTable::Release(FileTableEntry* entry) {
	internalLock->Acquire();
	bool isOpen = false;
	
	entry->count--;

	if (entry->count <= 0) {
		isOpen = entry->deletePending;

		delete entry->iNodeLock;
		entry->iNodeLock = nullptr;
		entry->inodeSector = -1;
		entry->inUse = false;
		entry->deletePending = false;
	}

	internalLock->Release();
	return isOpen;
}

bool
FileTable::MarkAsDeleted(int sectorInodo) {
	internalLock->Acquire();
	unsigned idx = FindEntry(sectorInodo);

	if (idx == (unsigned)-1) { // Si nadie lo tiene abierto
		internalLock->Release();
		return true;
	}
	// Si alguno lo tiene marcamos para borrar
	arr[idx].deletePending = true;
	internalLock->Release();
	return false;
}


unsigned
FileTable::FindEntry(int sector) {
	for(unsigned i = 0; i < size; i++) {
		if(arr[i].inUse && arr[i].inodeSector == sector) {
			return i;
		}
	}
	return (unsigned) -1;
}