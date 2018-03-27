#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <stdlib.h> /* needed for atoi() */

int  semRunning;

int  ClockDriver(char *);
void sleep(USLOSS_Sysargs *);
int  sleepReal(int seconds)
int  DiskDriver(char *);
void setUserMode();
void checkForKernelMode(char *);

void start3(void)
{
    char	name[128];
    char    termbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;
    /*
     * Check kernel mode here.
     */
	checkForKernelMode("start2");
    /*
     * Data structure initialization as needed...
     */
	int i;
	for(i = 0; i < USLOSS_MAX_SYSCALLS; i++)
	{
    systemCallVec[i] = nullsys3;
	}
	systemCallVec[SYS_SPAWN] = spawn;
	systemCallVec[SYS_WAIT] = wait;
	systemCallVec[SYS_TERMINATE] = terminate;
	systemCallVec[SYS_SEMCREATE] = semCreate;
	systemCallVec[SYS_SEMP] = semP;
	systemCallVec[SYS_SEMV] = semV;
	systemCallVec[SYS_SEMFREE] = semFree;
	systemCallVec[SYS_GETTIMEOFDAY] = getTimeOfDay;
	systemCallVec[SYS_CPUTIME] = cpuTime;
	systemCallVec[SYS_GETPID] = getPID;
	systemCallVec[SYS_SLEEP] = sleep;
	systemCallVec[SYS_DISKREAD] = diskRead;
	systemCallVec[SYS_DISKWRITE] = diskwrite;
	systemCallVec[SYS_TERMREAD] = termRead;	
	systemCallVec[SYS_TERMWRITE] = termWrite;	
	
    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    semRunning = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
	USLOSS_Console("start3(): Can't create clock driver\n");
	USLOSS_Halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "semRunning" once it is running.
     */

    sempReal(semRunning);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
    }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver

    // eventually, at the end:
    quit(0);
    
}

int ClockDriver(char *arg)
{
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(semRunning);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while(! isZapped()) {
	result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
	if (result != 0) {
	    return 0;
	}
	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */
    }
}

int DiskDriver(char *arg)
{
    return 0;
}

/* sleep */
void sleep(USLOSS_Sysargs *args)
{
  checkForKernelMode("sleep()");
  
  int seconds = (int) ((long) args->arg1);
  int status = sleepReal(seconds);

  if(debug4)
  {
    USLOSS_Console("sleep(): sleep for %d seconds, status = %d\n", seconds, status);
  }

  //switch back to user mode
  setUserMode();
}
/* end sleep */


/* sleepReal */
int sleepReal(int seconds)
{
  checkForKernelMode("sleepReal()");

  if(debug4)
  {
    USLOSS_Console("in sleepReal()\n");
  }

  if(seconds < 0)
  {
	return -1;
  }
  else
  {
    return 0;
  }	

}
/* sleepReal */


/* check for kernel mode */
void checkForKernelMode(char *name)
{
  if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0)
    {
      USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n",
                        name, getpid());
      USLOSS_Halt(1);
    }
}
/* end check for kernel mode */

/* set user mode */
void setUserMode()
{
  int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);

  if(result) {
  }
}
/* end set user mode */

void initQueue(procQueue* q, int type)
{
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  q->type = type;
}
/* end init queue */

/* enqueue */
void enqueue(procQueue *q, procPtr3 p)
{
  if(debug3)
  {
    USLOSS_Console("In enqueue()\n");
  }

  if(q->head == NULL && q->tail == NULL)
  {
    q->head = q->tail = p;
  }
  else
  {
    if(q->type == BLOCKED)
      q->tail->nextProc = p;
    else if(q->type == CHILDREN)
      q->tail->nextSibling = p;
    q->tail = p;
  }
  q->size++;
}
/* end enqueue */

procPtr3 dequeue(procQueue* q)
{
  if(debug3)
  {
    USLOSS_Console("In dequeue()\n");
  }

  procPtr3 temp = q->head;

  if(q->head == NULL)
  {
    return NULL;
  }

  if(q->head == q->tail)
  {
    q->head = q->tail = NULL;
  }
  else
  {
    if(q->type == BLOCKED)
    {
      q->head = q->head->nextProc;
    }
    else if(q->type == CHILDREN)
    {
      q->head = q->head->nextSibling;
    }
  }
  q->size--;
  
  return temp;
}
/* end dequeue */


