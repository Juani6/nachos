#include "synchconsole.hh"

SynchConsole::SynchConsole() {
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    writeLock = new Lock("STD_OUT lock");
    readLock  = new Lock("STD_IN Lock");
    console   = new Console(nullptr, nullptr, ReadAvail, WriteDone, this);
}

SynchConsole::~SynchConsole() {
    delete writeLock;
    delete readLock;
    delete readAvail;
    delete writeDone;
    delete console;
}

void
SynchConsole::PutChar(char ch) {
    
    writeLock->Acquire();
    console->PutChar(ch);
    writeDone->P();
    writeLock->Release();
    
}

char 
SynchConsole::GetChar() {

    readLock->Acquire();
    readAvail->P();
    char ch = console->GetChar();
    readLock->Release();
    return ch;
}

void
SynchConsole::ReadAvail(void *arg)
{   
    SynchConsole* sc = (SynchConsole* )arg;
    sc->readAvail->V();
}

void
SynchConsole::WriteDone(void *arg)
{
    SynchConsole* sc = (SynchConsole* )arg;
    sc->writeDone->V();
}