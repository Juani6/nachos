#ifndef NACHOS_FILESYS_FILETABLE__HH
#define NACHOS_FILESYS_FILETABLE__HH

/*
Necesitamos los i-nodos y
un lock por entrada

*/

const unsigned MAX_OPEN_FILES = 32;

class Lock;


typedef struct _FileTableEntry
{
	int inodeSector;

	// cuenta la cantidad de hilos que tienen acceso al archivo
	int count; 
	Lock* iNodeLock;

	bool inUse;
	bool deletePending;
} FileTableEntry;


class FileTable {
public: 
	FileTable(unsigned numDirEntries);
	~FileTable();

	FileTableEntry* Acquire(int sectorInodo);
	bool Release(FileTableEntry* entry);
	bool MarkAsDeleted(int sectorInodo);
private:
	unsigned size;
	FileTableEntry *arr;
	Lock* internalLock;
	unsigned FindEntry(int sector);
};


#endif