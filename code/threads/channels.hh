
#ifndef __NACHOS_THREAD_CHANNELS__
#define __NACHOS_THREAD_CHANNELS__

#include "semaphore.hh"
#include "lock.hh"


class Channel {
public:
    Channel(const char* debugName);
    ~Channel();

    void Write(int value);
    int Read();

private:
    // Debugging uses
    const char* name;
    // Semaphores to send the signals between the information between the threads
    Semaphore* writer;
    Semaphore* reader;
    Semaphore* consumed; 

    // Message to send
    int msg;

};

#endif