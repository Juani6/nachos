#include "thread_test_new_scheduler.hh"

#include <stdio.h>
#include "lock.hh"
#include "condition.hh"
#include "system.hh"
#include <string.h>



void PriorityThread(void* i){
  int n = (int)(long) i;
  printf("Yo: %s imprimo: %d\n",currentThread->GetName(),n);
}

void ThreadNewScheduler(){
  Thread* t4 = new Thread("Hilo 3",0,3);
  Thread* t3 = new Thread("Hilo 5",0,5);
  Thread* t5 = new Thread("Hilo 1",1,1);
  Thread* t1 = new Thread("Hilo 9",0,9);
  Thread* t2 = new Thread("Hilo 7",0,7);
  
  t4->Fork(PriorityThread, (void *)3);
  t3->Fork(PriorityThread, (void *)5);
  t2->Fork(PriorityThread, (void *)7);
  t5->Fork(PriorityThread, (void *)1);
  t1->Fork(PriorityThread, (void *)9);
  scheduler->Print();
  t5->Join();

  currentThread->Yield();
}