#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>


int start2(char *arg)
{
    int pid;
    int status;
	
	if (DEBUG2 && debugflag3)
        USLOSS_Console("start2(): at beginning\n");

	// Check kernel mode here.
    checkForKernelMode("start2");

    disableInterrupts();

    /*
     * Data structure initialization as needed...
     */

	// Initialize all system call vectors to nullsys3 by default
	for (i = 0; i < MAXSYSCALLS; i++)
        systemCallVec[i] = nullsys3;
	 
	// Initialize 10 system call vectors supported by phase3
	systemCallVec[SYS_SPAWN] = spawn;
	systemCallVec[SYS_WAIT] = wait;
	systemCallVec[SYS_TERMINATE] = terminate;
	systemCallVec[SYS_SEMCREATE] = semcreate;
	systemCallVec[SYS_SEMP] = semp;
	systemCallVec[SYS_SEMV] = semv;
	systemCallVec[SYS_SEMFREE] = semfree;
	systemCallVec[SYS_GETTIMEOFDAY] = gettimeofday;
	systemCallVec[SYS_CPUTIME] = cputime;
	systemCallVec[SYS_GETPID] = getPid;
	
	
    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler (via the systemCallVec array);s
     * spawnReal is the function that contains the implementation and is
     * called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, start2 calls spawnReal(), since start2 is in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);

} /* start2 */


void nullsys3(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys3(): Invalid syscall %d. Terminating\n", sysArg.number);

    #terminate;
		
} /* nullsys3 */


void spawn(USLOSS_Sysargs *args)
{
	int pid;
	
    USLOSS_Console("spawn(): Syscall %d.\n", sysArg.number);

	if (sysArg.number == SYS_SPAWN)
	{
		// fork1(char *name, int(*func)(char *), char *arg, int stacksize, int priority);
		pid = fork1(sysArg.arg5, sysArg.arg1, sysArg.arg2, sysArg.arg3, sysArg.arg4);
		sysArg.arg1 = pid;
		sysArg.arg4 = 0;
	}
	else
	{
		sysArg.arg4 = -1;
	}
	
} /* spawn */


void wait(USLOSS_Sysargs *args)
{
    USLOSS_Console("wait(): Syscall %d.\n", sysArg.number);
		
} /* wait */


void terminate(USLOSS_Sysargs *args)
{
    USLOSS_Console("terminate(): Syscall %d.\n", sysArg.number);
		
} /* terminate */


void semcreate(USLOSS_Sysargs *args)
{
    USLOSS_Console("semcreate(): Syscall %d.\n", sysArg.number);
		
} /* semcreate */


void semp(USLOSS_Sysargs *args)
{
    USLOSS_Console("semp(): Syscall %d.\n", sysArg.number);
		
} /* semp */


void semv(USLOSS_Sysargs *args)
{
    USLOSS_Console("semv(): Syscall %d.\n", sysArg.number);
		
} /* semv */


void semfree(USLOSS_Sysargs *args)
{
    USLOSS_Console("semfree(): Syscall %d.\n", sysArg.number);
		
} /* semfree */


void gettimeofday(USLOSS_Sysargs *args)
{
    USLOSS_Console("gettimeofday(): Syscall %d.\n", sysArg.number);
		
} /* gettimeofday */


void cputime(USLOSS_Sysargs *args)
{
    USLOSS_Console("cputime(): Syscall %d.\n", sysArg.number);
		
} /* cputime */


void getPid(USLOSS_Sysargs *args)
{
    USLOSS_Console("getPid(): Syscall %d.\n", sysArg.number);
		
} /* getpid */





void checkForKernelMode(char * name) 
{
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0)
    {
        USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n",
                        name, getpid());
        USLOSS_Halt(1);
    }
}

void disableInterrupts()
{
    int result; 
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        //not in kernel mode
        if(DEBUG2 && debugflag2)
            USLOSS_Console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
        USLOSS_Halt(1);
    }
    else 
    {
        //in kernel mode
       result = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
	   
    }
	
    if (result == USLOSS_DEV_OK)
        return;
    else 
    {
        USLOSS_Console("PSR not valid\n");
        USLOSS_Halt(1);		
    }
    return;
}

void enableInterrupts()
{
    int result; 
    //turn interrupts on iff in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        if(DEBUG2 && debugflag2)
            USLOSS_Console("Kernel Error: Not in kernel mode, may not enable interrupts\n");
        USLOSS_Halt(1);
    }
    else
    {
       result = USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
    }	
    if (result == USLOSS_DEV_OK)
        return;
    else 
    {
        USLOSS_Console("PSR not valid\n");
        USLOSS_Halt(1);	
    }
    return;
}

void* dequeue(queue *q)
{
    void* temp = q->head;
    if (q->head == NULL)
        return NULL;
    if (q->head == q->tail)
        q->head = q->tail = NULL;
    else
    {
        if (q->type == SLOTQUEUE)
            q->head = ((slotPtr)(q->head))->nextSlotPtr;
        else if (q->type == PROCQUEUE)
            q->head = ((mboxProcPtr)(q->head))->nextMboxProc;
    }
    q->size--;
    return temp;
}

void initializeQueue(queue *q, int type) 
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    q->type = type;
}


void enqueue(queue *q, void *p)
{
    if (q->head == NULL && q->tail == NULL)
    {
       q->head = q->tail = p; 
    }
    else
    {
        if (q->type == SLOTQUEUE)
            ((slotPtr)(q->tail))->nextSlotPtr = p;
        else if (q->type == PROCQUEUE)
            ((mboxProcPtr)(q->tail))->nextMboxProc = p;
        q->tail = p;
    }
    q->size++;
}
