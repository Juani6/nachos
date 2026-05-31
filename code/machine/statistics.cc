/// Routines for managing statistics about Nachos performance.
///
/// DO NOT CHANGE -- these stats are maintained by the machine emulation.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "statistics.hh"
#include "lib/utility.hh"

#include <stdio.h>
#include "lib/debug.hh"

/// Initialize performance metrics to zero, at system startup.
Statistics::Statistics()
{
    totalTicks = idleTicks = systemTicks = userTicks = 0;
    numDiskReads = numDiskWrites = 0;
    numConsoleCharsRead = numConsoleCharsWritten = 0;
    numPageFaults = numTLBHits = numTLBMisses = 0;
    numSwapOuts = numSwapIn = 0;
    numDemand = 0;

#ifdef DFS_TICKS_FIX
    tickResets = 0;
#endif

}

/// Print performance metrics, when we have finished everything at system
/// shutdown.
void
Statistics::Print()
{
#ifdef DFS_TICKS_FIX
    if (tickResets != 0) {
        printf("WARNING: the tick counter was reset %lu times; the following"
               " statistics may be invalid.\n\n", tickResets);
    }
#endif
    printf("Ticks: total %lu, idle %lu, system %lu, user %lu\n",
           totalTicks, idleTicks, systemTicks, userTicks);
    printf("Disk I/O: reads %lu, writes %lu\n", numDiskReads, numDiskWrites);
    printf("Console I/O: reads %lu, writes %lu\n",
           numConsoleCharsRead, numConsoleCharsWritten);
    printf("Paging: faults %lu\n", numPageFaults);
    // Numero de hits menos los repetidos
    unsigned long trueHits = numTLBHits - numPageFaults; 
    printf("Hit Ratio : %.4f%%\n", (double) trueHits / (trueHits + numTLBMisses) * 100.0);
    printf("Swap in: %lu\n", numSwapIn);
    printf("Swap out: %lu\n", numSwapOuts);
    printf("Demand : %lu\n", numDemand);
}

void
Statistics::Debug()
{
#ifdef DFS_TICKS_FIX
    if (tickResets != 0) {
        printf("WARNING: the tick counter was reset %lu times; the following"
               " statistics may be invalid.\n\n", tickResets);
    }
#endif
    DEBUG('p',"Ticks: total %lu, idle %lu, system %lu, user %lu\n",
           totalTicks, idleTicks, systemTicks, userTicks);
    DEBUG('p',"Disk I/O: reads %lu, writes %lu\n", numDiskReads, numDiskWrites);
    DEBUG('p',"Console I/O: reads %lu, writes %lu\n",
           numConsoleCharsRead, numConsoleCharsWritten);
    DEBUG('p',"Paging: faults %lu\n", numPageFaults);
    // Numero de hits menos los repetidos
    unsigned long trueHits = numTLBHits - numPageFaults; 
    DEBUG('p',"Hit Ratio : %.4f%%\n", (double) trueHits / (trueHits + numTLBMisses) * 100.0);
    DEBUG('p',"Swap in: %lu\n", numSwapIn);
    DEBUG('p',"Swap out: %lu\n", numSwapOuts);
    DEBUG('p',"Demand : %lu\n", numDemand);
}