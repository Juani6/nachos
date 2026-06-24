/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_RAWFILEHEADER__HH
#define NACHOS_FILESYS_RAWFILEHEADER__HH


#include "machine/disk.hh"


const unsigned NUM_DIRECT = (SECTOR_SIZE - 3 * sizeof (int)) / sizeof (int);
static const unsigned MAX_DIRECT_SIZE = NUM_DIRECT * SECTOR_SIZE;

// Cantidad de punteros que entran en un sector
const unsigned NUMBER_POINTERS = SECTOR_SIZE / sizeof(int); 
static const unsigned INDIRECTION_SIZE = NUMBER_POINTERS * NUMBER_POINTERS * SECTOR_SIZE; 
const unsigned MAX_FILE_SIZE = MAX_DIRECT_SIZE + INDIRECTION_SIZE;

struct RawFileHeader {
    unsigned numBytes;  ///< Number of bytes in the file.
    unsigned numSectors;  ///< Number of data sectors in the file.
    unsigned dataSectors[NUM_DIRECT];  ///< Disk sector numbers for each data
                                       ///< block in the file.
    unsigned indirection; // Numero de sector segunda indireccion
};


#endif
