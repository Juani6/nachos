#ifndef SYNCHCONSOLE_HH
#define SYNCHCONSOLE_HH


#include "threads/lock.hh"
#include "threads/semaphore.hh"
#include "machine/console.hh"

class SynchConsole {
public:
    SynchConsole();

    ~SynchConsole();

    void PutChar(char ch);

    char GetChar();
private:
    static void ReadAvail(void*arg);
    static void WriteDone(void*arg);
    Lock *writeLock;
    Lock *readLock;
    Semaphore* readAvail;
    Semaphore* writeDone;
    Console *console;
};



#endif