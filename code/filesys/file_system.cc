/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

using namespace std;
/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    fsLock = new Lock("fsLock");
    lockDirArr = new Lock("lockDirArr");
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        dirLocks[i] = nullptr;
    }

    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader();
        FileHeader *dirH    = new FileHeader();

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.
        FileTableEntry* freeMapFileEntry = fileTable->Acquire(FREE_MAP_SECTOR);
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR,freeMapFileEntry);
        FileTableEntry* DirectoryEntry = fileTable->Acquire(DIRECTORY_SECTOR);        
        directoryFile = new OpenFile(DIRECTORY_SECTOR,DirectoryEntry);

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();
        }
        
        delete freeMap;
        delete dir;
        delete mapH;
        delete dirH;
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        FileTableEntry* freeMapFileEntry = fileTable->Acquire(FREE_MAP_SECTOR);
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR,freeMapFileEntry);
        FileTableEntry* DirectoryEntry = fileTable->Acquire(DIRECTORY_SECTOR);        
        directoryFile = new OpenFile(DIRECTORY_SECTOR,DirectoryEntry);
    }
}

FileSystem::~FileSystem()
{
    // De estos dos ahora se encarga la fileTable
    delete freeMapFile;
    delete directoryFile;
    delete fsLock;
    delete lockDirArr;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        if (dirLocks[i]) {
            delete dirLocks[i];
        }
    }
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *_name, unsigned initialSize)
{
    ASSERT(_name != nullptr);
    ASSERT(initialSize < MAX_FILE_SIZE);

    DEBUG('f', "Creating file %s, size %u\n", _name, initialSize);
    
    char* nameCopy = strdup(_name);
    
    pair <int,char*> res = ResolvePath(nameCopy);
    int dirSector = res.first;
    char* name = res.second;
    
    OpenFile* dirFile = new OpenFile(dirSector);
    
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    
    GetDirLock(dirSector)->AcquireWrite();
    
    dir->FetchFrom(dirFile);

    bool success;

    if (dir->Find(name) != -1) {
        success = false;  // File is already in directory.
    } else {
        fsLock->Acquire();
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            success = false;  // No free block for file header.
            fsLock->Release();
        } else if (!dir->Add(name, sector)) {
            success = false;  // No space in directory.
            fsLock->Release();
        } else {
            FileHeader *h = new FileHeader();
            success = h->Allocate(freeMap, initialSize);
              // Fails if no space on disk for data.
            if (success) {
                // Everything worked, flush all changes back to disk.
                h->WriteBack(sector);
                dir->WriteBack(dirFile);
                freeMap->WriteBack(freeMapFile);
            }
            fsLock->Release();
            delete h;
        }
        delete freeMap;
    }
    delete dir;
    delete dirFile;
    free(nameCopy);
    free(name);

    GetDirLock(dirSector)->ReleaseWrite();
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *_name)
{
    ASSERT(_name != nullptr);
    
    char *nameCopy = strdup(_name);
    pair<int,char*> res = ResolvePath(nameCopy);
    int dirSector = res.first;
    char* name = res.second;
    
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile  *openFile = nullptr;
    OpenFile* dirFile = new OpenFile(dirSector);
    
    DEBUG('f', "Opening file %s\n", name);
    GetDirLock(dirSector)->AcquireRead();
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    //ASSERT(sector != -1);
    delete dir;
    delete dirFile;
    free(name);
    free(nameCopy);
    
    
    if (sector >= 0) {
        FileTableEntry* tableEntry = fileTable->Acquire(sector);
        if (!tableEntry) {
            DEBUG('f', "tableEntry llena");
            GetDirLock(dirSector)->ReleaseRead();
            return nullptr;
        }
        else {
            openFile = new OpenFile(sector,tableEntry);  // `name` was found in directory.
        }
    }
    GetDirLock(dirSector)->ReleaseRead();
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *_name)
{
    ASSERT(_name != nullptr);

    
    
    char *nameCopy = strdup(_name);
    pair<int,char*> res = ResolvePath(nameCopy);
    int dirSector = res.first;
    char* name = res.second;
    OpenFile* dirFile = new OpenFile(dirSector);
    
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    
    GetDirLock(dirSector)->AcquireWrite();
    
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    if (sector == -1) {
       delete dir;
       return false;  // file not found
    }
    // Inicialmente borramos el directorio y despues
    // se vera quien elimina los datos del disco
    dir->Remove(name);
    dir->WriteBack(dirFile);    // Flush to disk.
    delete dir;
    delete dirFile;
    free(name);
    free(nameCopy);
    if (fileTable->MarkAsDeleted(sector)) {
        DeletePhysicalSector(sector);
    }

    GetDirLock(dirSector)->ReleaseWrite();

    return true;
}

void
FileSystem::DeletePhysicalSector(int sector) {
    FileHeader *fileH = new FileHeader();
    fileH->FetchFrom(sector);
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    fsLock->Acquire();
    freeMap->FetchFrom(freeMapFile);
    
    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.
    
    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    fsLock->Release();
    delete fileH;
    delete freeMap;
}

/// List all the files in the file system directory.
void
FileSystem::List()
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile* dirFile = new OpenFile(currentDirSector);
    dir->FetchFrom(dirFile);
    DEBUG('e', "Listando archivos del sector %d",currentDirSector);
    dir->List();
    delete dirFile;
    delete dir;
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader();
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader();
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader();
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader();
    FileHeader *dirH    = new FileHeader();
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}

OpenFile*
FileSystem::GetFreeMapFile() {
    return freeMapFile;
}


OpenFile*
FileSystem::GetDirFile() {
    return directoryFile;
}

// Esto lo que dice es que si hay algo que no se reconoce
// lo asuma como parte de la libreria estandar
using namespace std;
#include <vector>
#include <stdlib.h>
vector<char*> parse(char* _buff,char* div) {
    char *buff = strdup(_buff);
    vector<char*> tokensArr;
    char* token = strtok(buff,div);
    while (token != nullptr) {
        tokensArr.push_back(strdup(token));
        token = strtok(nullptr,div);
    }
    free(buff);
    return tokensArr;
}

pair<int,char*>
FileSystem::ResolvePath(char* path) {
    
    int dirSector = currentDirSector;
    if (path[0] == '/') {
        dirSector = DIRECTORY_SECTOR;
    }
    GetDirLock(dirSector)->AcquireRead();
    
    char div[] = "/";
    vector<char*> tokensArr = parse(path, div);
    
    if (tokensArr.empty()) {
        GetDirLock(dirSector)->ReleaseRead();
        return {-1,nullptr};
    }
    
    for (size_t i = 0; i < tokensArr.size() - 1; i++) {
        Directory* dir = new Directory(1); // fetchFrom pone el valor adecuado
        OpenFile* dirFile = new OpenFile(dirSector);
        dir->FetchFrom(dirFile);
        DirectoryEntry* entry = dir->FindEntry(tokensArr[i]);

        if (entry == nullptr || !entry->isDirectory) {
            DEBUG('f', "Error en el path");
            delete dirFile;
            delete dir;
            for (char* token : tokensArr) {
                free(token);
            }
            GetDirLock(dirSector)->ReleaseRead();
            return {-1,nullptr};
        }
        int newSector = entry->sector;
        delete dir;
        delete dirFile;
        GetDirLock(newSector)->AcquireRead();
        GetDirLock(dirSector)->ReleaseRead();
        dirSector = newSector;

        if (dirSector == -1) {
            GetDirLock(dirSector)->ReleaseRead();
            return {-1,nullptr};
        }
    }
    
    char* name = strdup(tokensArr.back());
    
    for (char* token : tokensArr) {
        free(token);
    }
    DEBUG('f', "dirSector : %u name: %s", dirSector,name);
    GetDirLock(dirSector)->ReleaseRead();
    return {dirSector,name};
}

int
FileSystem::ChangeDir(char* path) {

    if (strcmp(path, "/") == 0) {
        currentDirSector = DIRECTORY_SECTOR;
        return 0;
    }

    std::pair<int,char*> result = ResolvePath(path);
    int dirSector = result.first;
    char* name = result.second;

    if (name == nullptr || strlen(name) == 0) {
        currentDirSector = dirSector;
        free(name);
        return 0;
    }

    Directory *dir = new Directory(1);
    OpenFile* dirFile = new OpenFile(dirSector);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);


    int oldSector = currentDirSector;
    DEBUG('e',"Cambiando del sector %d a %d\n", oldSector, sector);

    if (sector == -1){
        DEBUG('f', "Error en el path\n");
        delete dir;
        delete dirFile;
        free(name);
        return -1;
    }

    DirectoryEntry* entry = dir->FindEntry(name);

    if (!entry->isDirectory) {
        DEBUG('f', "No es un directorio\n");
        delete dir;
        delete dirFile;
        free(name);
        return -1;
    }
    currentDirSector = sector;
    delete dir;
    delete dirFile;
    free(name);
    return 0;
}

bool
FileSystem::CreateDir(const char* _name) {
    ASSERT(_name != nullptr);
    
    DEBUG('f', "Creating dir %s\n", _name);
    
    char* nameCopy = strdup(_name);
    
    pair <int,char*> res = ResolvePath(nameCopy);
    int fatherDirSector = res.first;
    char* name = res.second;
    
    OpenFile* fatherDirFile = new OpenFile(fatherDirSector);
    
    Directory *fatherDir = new Directory(1);
    GetDirLock(fatherDirSector)->AcquireWrite();
    fatherDir->FetchFrom(fatherDirFile);
    
    bool success;
    if (fatherDir->Find(name) != -1) {
        success = false;
    }
    else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        fsLock->Acquire();
        freeMap->FetchFrom(freeMapFile);
        int newDirSector = freeMap->Find();

        if (newDirSector == -1) {
            success = false;
            fsLock->Release();
        }

        else if (!fatherDir->Add(name,newDirSector,true)) {
            success = false;
            fsLock->Release();
        }

        else {
            DEBUG('e',"Sectores libres: %d\n", freeMap->CountClear());
            DEBUG('e',"newDirSector: %d\n", newDirSector);
            FileHeader *h = new FileHeader;
            success = h->Allocate(freeMap, sizeof(DirectoryEntry) * NUM_DIR_ENTRIES);
            
            if (debug.IsEnabled('e')) {
                DEBUG('e',"numSectors=%d numBytes=%d\n", h->GetRaw()->numSectors, h->GetRaw()->numBytes);
                for (unsigned i = 0; i < h->GetRaw()->numSectors; i++) {
                    DEBUG('e',"dataSectors[%d] = %d\n", i, h->GetRaw()->dataSectors[i]);
                }
            }
            
            if (success) {
                DEBUG('e', "Entre a success\n");
                freeMap->WriteBack(freeMapFile);
                h->WriteBack(newDirSector);
                
                Directory* newDir = new Directory(NUM_DIR_ENTRIES);
                OpenFile* newDirFile = new OpenFile(newDirSector);
                
                DEBUG('f',"CreateDir: '%s' guardado en sector=%d del padre=%d\n", name, newDirSector, fatherDirSector);
                
                newDir->WriteBack(newDirFile);
                fatherDir->WriteBack(fatherDirFile);
                
                DEBUG('e', "Write backs finalizados\n");
                delete newDir;
                delete newDirFile;
            }
            fsLock->Release();
            delete h;
        }
        delete freeMap;
    }

    
    delete fatherDir;
    delete fatherDirFile;
    free(name);
    free(nameCopy);
    GetDirLock(fatherDirSector)->ReleaseWrite();
    return success;
}

RWLock*
FileSystem::GetDirLock(int sector) {
    lockDirArr->Acquire();
    if(!dirLocks[sector]) {
        dirLocks[sector] = new RWLock();
    }
    lockDirArr->Release();
    return dirLocks[sector];
}