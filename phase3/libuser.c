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
    sysArg.arg3 = (void *)stack_size;
    sysArg.arg4 = (void *) priority;
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

    USLOSS_Syscall(&sysArg);

    *pid = (int) sysArg.arg1;
    *status = (int) sysArg.arg2;
    return (int) ((long)sysArg.arg4);
    
} /* end of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate 
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Input:        arg1: termination code for the process.
 *
 */
void Terminate(int status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_TERMINATE;

    USLOSS_Syscall(&sysArg);

    status = (int) ((long)sysArg.arg1);
        
} /* end of Terminate */


/*
 *  Routine:  SemCreate
 *
 *  Description: Create a semaphore.
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
    sysArg.arg1 = (void *) value;

    USLOSS_Syscall(&sysArg);

	*semaphore = (int) sysArg.arg1;
	
    return (int) ((long)sysArg.arg4);   

} /* end of SemCreate */


/*
 *  Routine:  SemP
 *
 *  Description: "P" a semaphore.
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
    sysArg.arg1 = (void *) semaphore;	

    USLOSS_Syscall(&sysArg);
	
    return (int) sysArg.arg4;

} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description: "V" a semaphore.
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
    sysArg.arg1 = (void *) semaphore;
	
    USLOSS_Syscall(&sysArg);

    return (int) ((long)sysArg.arg4);

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

    sysArg.arg1 = (void *) semaphore;

    USLOSS_Syscall(&sysArg);
	
    return (int) ((long)sysArg.arg4);

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

    USLOSS_Syscall(&sysArg);

    *tod = (int) ((long)sysArg.arg1);

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

    USLOSS_Syscall(&sysArg);

    *cpu = (int) ((long)sysArg.arg1);

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

    USLOSS_Syscall(&sysArg);

    *pid = (int) ((long)sysArg.arg1);
	
} /* end of GetPID */

/* end libuser.c */
