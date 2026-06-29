/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
    ASSERT(freeMap != nullptr);

    if (fileSize > MAX_FILE_SIZE) {
        return false;
    }

    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);
    
    if (freeMap->CountClear() < raw.numSectors) {
        return false;  // Not enough space.
    }
    
    unsigned fstLvl[NUMBER_POINTERS];
    unsigned sndLvl[NUMBER_POINTERS];

    if (raw.numSectors > NUM_DIRECT) {
        raw.indirection = freeMap->Find();
        memset(fstLvl,0,SECTOR_SIZE);
        synchDisk->WriteSector(raw.indirection, (char*)fstLvl);
    }

    unsigned logicalBlock;
    for (unsigned i = 0; i < raw.numSectors; i++) {
        if (i < NUM_DIRECT) {
            raw.dataSectors[i] = freeMap->Find();
        }
        else {
            // Calculamos las direcciones en cada nivel de indireccion
            logicalBlock = i - NUM_DIRECT;
            unsigned indirectionNumber = logicalBlock / NUMBER_POINTERS;
            unsigned indirectionOffset = logicalBlock % NUMBER_POINTERS;
            //Traemos el primer nivel
            synchDisk->ReadSector(raw.indirection,(char*)fstLvl);
            // Si el nivel que buscamos no existe lo creamos
            if (fstLvl[indirectionNumber] == 0) {
                fstLvl[indirectionNumber] = freeMap->Find();
                memset(sndLvl,0,SECTOR_SIZE);
                synchDisk->WriteSector(fstLvl[indirectionNumber],(char*)sndLvl);
                synchDisk->WriteSector(raw.indirection, (char*)fstLvl);
            }
            // Allocamos el sector final
            synchDisk->ReadSector(fstLvl[indirectionNumber],(char*) sndLvl);
            sndLvl[indirectionOffset] = freeMap->Find();
            synchDisk->WriteSector(fstLvl[indirectionNumber],(char*) sndLvl);

        }

    }
    return true;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);
    unsigned fstLvl[NUMBER_POINTERS];
    unsigned sndLvl[NUMBER_POINTERS];
    for (unsigned i = 0; i < raw.numSectors; i++) {
        if (i < NUM_DIRECT) {
            freeMap->Clear(raw.dataSectors[i]);
        } else {
            unsigned logicalBlock = i - NUM_DIRECT;
            unsigned indirectionNumber = logicalBlock / NUMBER_POINTERS;
            unsigned indirectionOffset = logicalBlock % NUMBER_POINTERS;

            synchDisk->ReadSector(raw.indirection, (char*)fstLvl);
            synchDisk->ReadSector(fstLvl[indirectionNumber], (char*)sndLvl);
            freeMap->Clear(sndLvl[indirectionOffset]);

            // liberar bloque de 2do nivel al terminar sus entradas
            if (indirectionOffset == NUMBER_POINTERS - 1 
                || i == raw.numSectors - 1) {
                freeMap->Clear(fstLvl[indirectionNumber]);
            }
        }
    }

    // liberar bloque raíz de indirección
    if (raw.numSectors > NUM_DIRECT) {
        freeMap->Clear(raw.indirection);
    }

}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    synchDisk->WriteSector(sector, (char *) &raw);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    unsigned logicalBlock = offset / SECTOR_SIZE;
    if (logicalBlock < NUM_DIRECT) {
        return raw.dataSectors[logicalBlock];
    }
    unsigned dataSector = GetIndirectionSector(logicalBlock);

    return dataSector;
    
}

unsigned
FileHeader::GetIndirectionSector(unsigned logicalBlock) {
    logicalBlock -= NUM_DIRECT;
    unsigned indirectionNumber = logicalBlock / NUMBER_POINTERS;
    unsigned indirectionOffset = logicalBlock % NUMBER_POINTERS;

    unsigned firstLevel[NUMBER_POINTERS]; 
    synchDisk->ReadSector(raw.indirection,(char*) firstLevel);
    ASSERT(firstLevel[indirectionNumber] != 0);
    unsigned secondLevel[NUMBER_POINTERS];
    synchDisk->ReadSector(firstLevel[indirectionNumber], (char*) secondLevel);
    ASSERT(secondLevel[indirectionOffset] != 0);
    return secondLevel[indirectionOffset];

}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr) {
        printf("File header:\n");
    } else {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    for (unsigned i = 0; i < raw.numSectors; i++) {
        printf("%u ", ByteToSector(i * SECTOR_SIZE));
    }
    printf("\n");

    for (unsigned i = 0; i < raw.numSectors; i++) {
        unsigned sector = ByteToSector(i * SECTOR_SIZE);
        printf("    contents of block %u:\n", sector);
        synchDisk->ReadSector(sector, data);

        unsigned bytesToPrint = SECTOR_SIZE;

        if (i == raw.numSectors - 1) {
            bytesToPrint = raw.numBytes % SECTOR_SIZE;
            if (bytesToPrint == 0) {
                bytesToPrint = SECTOR_SIZE;
            }
        }

        for (unsigned j = 0; j < bytesToPrint; j++) {
            if (isprint(data[j])) {
                printf("%c", data[j]);
            } else {
                printf("\\%X", (unsigned char) data[j]);
            }
        }
        printf("\n");
    }
    delete [] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}


bool
FileHeader::Extend(unsigned newSize) {
    Bitmap* freeMap = new Bitmap(NUM_SECTORS);
    //->fsLock->Acquire();
    freeMap->FetchFrom(fileSystem->GetFreeMapFile());

    
    unsigned newNumSector = DivRoundUp(newSize, SECTOR_SIZE);
    unsigned oldNumSectors = raw.numSectors;
    unsigned diffSector = newNumSector - oldNumSectors; // Cantidad de sectores a allocar 
    
    //DEBUG('f', "nNS = % u, oNS = %u, dS = %u", newNumSector, oldNumSectors, diffSector);
    if (newNumSector <= oldNumSectors) {
        raw.numBytes = newSize;
        delete freeMap;
      //  fileSystem->fsLock->Release();
        return true;
    }
    if (freeMap->CountClear() < diffSector) {
        //fileSystem->fsLock->Release();
        return false;  // Not enough space.
    }
    // Una vez verificado que hay espacio disponible modificamos los datos del hdr
    raw.numBytes = newSize;
    raw.numSectors = newNumSector;

    unsigned fstLvl[NUMBER_POINTERS];
    unsigned sndLvl[NUMBER_POINTERS];

    if (raw.indirection == 0 && newNumSector > NUM_DIRECT) {
        raw.indirection = freeMap->Find();
        memset(fstLvl,0,SECTOR_SIZE);
        synchDisk->WriteSector(raw.indirection, (char*)fstLvl);
    }

    unsigned logicalBlock;
    for (unsigned i = oldNumSectors; i < newNumSector; i++) {
        if (i < NUM_DIRECT) {
            raw.dataSectors[i] = freeMap->Find();
        }
        else {
            // Calculamos las direcciones en cada nivel de indireccion
            logicalBlock = i - NUM_DIRECT;
            unsigned indirectionNumber = logicalBlock / NUMBER_POINTERS;
            unsigned indirectionOffset = logicalBlock % NUMBER_POINTERS;
            //Traemos el primer nivel
            synchDisk->ReadSector(raw.indirection,(char*)fstLvl);
            // Si el nivel que buscamos no existe lo creamos
            if (fstLvl[indirectionNumber] == 0) {
                fstLvl[indirectionNumber] = freeMap->Find();
                memset(sndLvl,0,SECTOR_SIZE);
                synchDisk->WriteSector(fstLvl[indirectionNumber],(char*)sndLvl);
                synchDisk->WriteSector(raw.indirection, (char*)fstLvl);
            }
            // Allocamos el sector final
            synchDisk->ReadSector(fstLvl[indirectionNumber],(char*) sndLvl);
            sndLvl[indirectionOffset] = freeMap->Find();
            synchDisk->WriteSector(fstLvl[indirectionNumber],(char*) sndLvl);

        }
    }
    
    freeMap->WriteBack(fileSystem->GetFreeMapFile());
    
    delete freeMap;
    //fileSystem->fsLock->Release();
    return true;
}
