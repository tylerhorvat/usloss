/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <libuser.h>

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}


/*
 *  Routine:  Spawn
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *                PFV func      -- pointer to the function to fork
 *                void *arg     -- argument to function
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                int  *pid     -- pointer to output value
 *                (output value: process id of the forked process)
 *
 *  Input:        arg1: address of the function to spawn.
 *                arg2: parameter passed to spawned function.
 *                arg3: stack size (in bytes).
 *                arg4: priority.
 *                arg5: character string containing process’s name.
 *
 *  Output:       arg1: the PID of the newly created process; -1 if a process could not be created.
 *                arg4: -1 if illegal values are given as input; 0 otherwise.
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Spawn(char *name, int (*func)(char *), char *arg, int stack_size, 
    int priority, int *pid)   
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SPAWN;
    sysArg.arg1 = (void *) func;
    sysArg.arg2 = arg;
    sysArg.arg3 = (void *) ((long) stack_size);
    sysArg.arg4 = (void *) ((long) priority);
    sysArg.arg5 = name;

    USLOSS_Syscall(&sysArg);

    *pid = (int) ((long)sysArg.arg1);
    return (int) ((long)sysArg.arg4);
} /* end of Spawn */


/*
 *  Routine:  Wait
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Output:       arg1: process id of the terminating child.
 *                arg2: the termination code of the child.
 * 
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Wait(int *pid, int *status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_WAIT;
    sysArg.arg2 = status;

    USLOSS_Syscall(&sysArg);

    pid = (int *) sysArg.arg1;
    status = (int *) sysArg.arg2;
    return (int) ((long)sysArg.arg4);
    
} /* end of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate 
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the completion status of the process
 *
 *  Input:        arg1: termination code for the process.
 *
 */
void Terminate(int status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_TERMINATE;
    sysArg.arg1 = (void *) ((long) status);

    USLOSS_Syscall(&sysArg);

    //status = (int) ((long)sysArg.arg1);
        
} /* end of Terminate */


/*
 *  Routine:  SemCreate
 *
 *  Description:  Create a semaphore.
 *
 *  Arguments:
 *
 *  Input:        arg1: initial semaphore value.
 *
 *  Output:       arg1: semaphore handle to be used in subsequent semaphore system calls.
 *                arg4: -1 if initial value is negative or no semaphores are available; 0 otherwise.
 *
 */
int SemCreate(int value, int *semaphore)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SEMCREATE;
    sysArg.arg1 = (void *) ((long) value);

    USLOSS_Syscall(&sysArg);

    *semaphore = (int) ((long) sysArg.arg1);
	
    return (long) sysArg.arg4;   

} /* end of SemCreate */


/*
 *  Routine:  SemP
 *
 *  Description:  "P" a semaphore.
 *
 *  Arguments:
 *
 *  Input:        arg1: semaphore handle.
 *
 *  Output:       arg4: -1 if semaphore handle is invalid, 0 otherwise.
 *
 */
int SemP(int semaphore)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SEMP;
    sysArg.arg1 = (void *) ((long) semaphore);	

    USLOSS_Syscall(&sysArg);
	
    return (long) sysArg.arg4;

} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description:  "V" a semaphore.
 *
 *  Arguments:
 *
 *  Input:        arg1: semaphore handle.
 *
 *  Output:       arg4: -1 if semaphore handle is invalid, 0 otherwise.
 *
 */
int SemV(int semaphore)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SEMV;
    sysArg.arg1 = (void *) ((long) semaphore);
	
    USLOSS_Syscall(&sysArg);

    return (long) sysArg.arg4;

} /* end of SemV */


/*
 *  Routine:  SemFree
 *
 *  Description:  Free a semaphore.
 *
 *  Arguments:
 *
 *  Input:        arg1: semaphore handle.
 *
 *  Output:       arg4: -1 if semaphore handle is invalid, 1 if there 
 *                were processes blocked on the semaphore, 0 otherwise.
 *
 */
int SemFree(int semaphore)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SEMFREE;

    sysArg.arg1 = (void *) ((long) semaphore);

    USLOSS_Syscall(&sysArg);
	
    return (long) sysArg.arg4;

} /* end of SemFree */


/*
 *  Routine:  GetTimeofDay
 *
 *  Description:  This is the call entry point for getting the time of day.
 *
 *  Arguments:
 *
 *  Output:       arg1: the time of day.
 *
 */
void GetTimeofDay(int *tod)                           
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;

    sysArg.number = SYS_GETTIMEOFDAY;
    sysArg.arg1 = tod;

    USLOSS_Syscall(&sysArg);

    //*tod = (int) ((long)sysArg.arg1);

} /* end of GetTimeofDay */


/*
 *  Routine:  CPUTime
 *
 *  Description:  This is the call entry point for the process' CPU time.
 *
 *  Arguments:
 *
 *  Output:       arg1: the CPU time used by the currently running process.
 *
 */
void CPUTime(int *cpu)                           
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_CPUTIME;
    sysArg.arg1 = cpu;

    USLOSS_Syscall(&sysArg);

    //*cpu = (int) ((long)sysArg.arg1);

} /* end of CPUTime */


/*
 *  Routine:  GetPID
 *
 *  Description:  This is the call entry point for the process' PID.
 *
 *  Arguments:
 *
 *  Output:       arg1: the process ID.
 *
 */
void GetPID(int *pid)                           
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_GETPID;
    sysArg.arg1 = pid;

    USLOSS_Syscall(&sysArg);

    //*pid = (int) ((long)sysArg.arg1);
	
} /* end of GetPID */


/*
 *  Routine:  Sleep
 *
 *  Description:  Delays the calling process for the specified number of seconds (sleep).
 *
 *  Input:  
 *			arg1: number of seconds to delay the process
 *
 *  Output:  
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *
 */
int  Sleep(int seconds)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) ((long) seconds);

    USLOSS_Syscall(&sysArg);
	
    return (long) sysArg.arg4;	
}


/*
 *  Routine:  DiskRead
 *
 *  Description:  Reads one or more sectors from a disk (diskRead).
 *
 *  Input:
 *			arg1: the memory address to which to transfer
 *			arg2: number of sectors to read
 *			arg3: the starting disk track number
 *			arg4: the starting disk sector number
 *			arg5: the unit number of the disk from which to read
 *			
 *  Output:       
 *			arg1: 0 if transfer was successful; the disk status register otherwise.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 *	Note:  The arg4 result is only set to -1 if any of the input parameters are 
 *         obviously invalid, e.g. the starting sector is negative.
 * 
 */
int  DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) ((long) sectors);
	sysArg.arg3 = (void *) ((long) track);
	sysArg.arg4 = (void *) ((long) first);
    sysArg.arg5 = (void *) ((long) unit);		
	
    USLOSS_Syscall(&sysArg);
    status = (int *) sysArg.arg1;
    return (long) sysArg.arg4;
}


/*
 *  Routine:  DiskWrite
 *
 *  Description:  Writes one or more sectors to the disk (diskWrite).
 *
 *  Input:
 *			arg1: the memory address from which to transfer.
 *			arg2: number of sectors to write.
 *			arg3: the starting disk track number.
 *			arg4: the starting disk sector number.
 *			arg5: the unit number of the disk to write.
 *			
 *  Output:       
 *			arg1: 0 if transfer was successful; the disk status register otherwise.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 *	Note:  The arg4 result is only set to -1 if any of the input parameters are 
 *         obviously invalid, e.g. the starting sector is negative.
 * 
 */
int  DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) ((long) sectors);
	sysArg.arg3 = (void *) ((long) track);
	sysArg.arg4 = (void *) ((long) first);
    sysArg.arg5 = (void *) ((long) unit);		
	
    USLOSS_Syscall(&sysArg);
    status = (int *) sysArg.arg1;
    return (long) sysArg.arg4;
}


/*
 *  Routine:  DiskSize
 *
 *  Description:  Returns information about the size of the disk (diskSize).
 *
 *  Input:
 *			arg1: the unit number of the disk
 *			
 *  Output:       
 *			arg1: size of a sector, in bytes
 *			arg2: number of sectors in a track
 *			arg3: number of tracks in the disk
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *
 */
int  DiskSize(int unit, int *sector, int *track, int *disk)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) ((long) unit);
	
    USLOSS_Syscall(&sysArg);
    sector = (int *) sysArg.arg1;
    track = (int *) sysArg.arg2;
	disk = (int *) sysArg.arg3;
    return (long) sysArg.arg4;
}


/*
 *  Routine:  TermRead
 *
 *  Description:  Read a line from a terminal (termRead).
 *
 *  Input:
 *			arg1: address of the user’s line buffer.
 *			arg2: maximum size of the buffer.
 *			arg3: the unit number of the terminal from which to read.
 *			
 *  Output:       
 *			arg2: number of characters read.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 */
int  TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void *) ((long) bsize);
	sysArg.arg3 = (void *) ((long) unit_id);		
	
    USLOSS_Syscall(&sysArg);
    nread = (int *) sysArg.arg1;
    return (long) sysArg.arg4;	
}


/*
 *  Routine:  TermWrite
 *
 *  Description:  Write a line to a terminal (termWrite).
 *
 *  Input:
 *			arg1: address of the user’s line buffer.
 *			arg2: number of characters to write.
 *			arg3: the unit number of the terminal to which to write.
 *			
 *  Output:       
 *			arg2: number of characters written.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 */
int  TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void *) ((long) bsize);
	sysArg.arg3 = (void *) ((long) unit_id);		
	
    USLOSS_Syscall(&sysArg);
    nwrite = (int *) sysArg.arg1;
    return (long) sysArg.arg4;	
}


/* end libuser.c */
