/*
 * This file contains the function definitions for the library interfaces
 * to the USLOSS system calls.
 */
#ifndef _LIBUSER_H
#define _LIBUSER_H


extern int TermRead(char *buff, int bsize, int unit, int *nread); 
extern int TermWrite(char *buff, int bsize, int unit, int *nwrite);    
extern int Spawn(char *name, int (*func)(void *), void *arg, int stack_size, 
		int priority, int *pid);   
extern int Wait(int *pid, int *status);
extern void Terminate(int status);
extern int Sleep(int seconds);                  
extern int DiskWrite(void *dbuff, int track, int first,int sectors,int unit, int *status)
           ;
extern int DiskRead(void *dbuff, int track, int first, int sectors,int unit, int *status)
           ;
extern int DiskSize(int unit, int *sector, int *track, int *disk);
extern void GetTimeOfDay(int *tod);                           
extern void CPUTime(int *cpu);                      
extern void GetPID(int *pid);         
extern void DumpProcesses(void);                
extern int SemCreate(char *name, int value, int *semaphore);
extern int SemP(int semaphore);
extern int SemV(int semaphore);
extern int SemFree(int semaphore);

#ifdef PHASE_3
extern int VmInit(int mappings, int pages, int frames, int pagers, void **region);
extern void VmDestroy(void);
/*
 * Phase 3 extra credit.
 */
extern int Protect(int page, int protection);
extern int Share(int pid, int source, int target);
extern int COW(int pid, int source, int target);
#endif

extern int MboxCreate(int numslots, int slotsize, int *mbox);
extern int MboxRelease(int mbox);
extern int MboxSend(int mbox, void *msg, int *size);                     
extern int MboxReceive(int mbox, void *msg, int *size);
extern int MboxCondSend(int mbox, void *msg, int *size);                     
extern int MboxCondReceive(int mbox, void *msg, int *size);

#endif

