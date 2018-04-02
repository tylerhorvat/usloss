#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <stdlib.h> /* needed for atoi() */
#include <providedPrototypes.h>
#include <sems.h>


/*------------------ Prototypes ----------------------*/
int ClockDriver(char *);
void sleep(USLOSS_Sysargs *);
int sleepReal(int seconds);
int Disk0Driver(char *);
int Disk1Driver(char *);
void diskRead(USLOSS_Sysargs *);
void diskWrite(USLOSS_Sysargs *);
int Term0Driver(char *);
int Term1Driver(char *);
int Term2Driver(char *);
int Term3Driver(char *);
void termRead(USLOSS_Sysargs *);
void termWrite(USLOSS_Sysargs *);
void setUserMode();
void checkForKernelMode(char *);

void emptyProc(int);
void initProc(int);
void initProcQueue(procQueue*, int);
procPtr dequeue(procQueue*);
procPtr peek(procQueue*);
void removeChild(procQueue*, procPtr);



/*------------------ Global Variables ----------------*/
int debug3 = 0;
int debug4 = 1;
int numSems;
semaphore SemTable[MAXSEMS];
procStruct4 ProcTable4[MAXPROC];

int semRunning;


void start3(void)
{
    //char	name[128];
    //char    termbuf[10];
    int		i;
    int		clockPID;
	int		disk0PID;
	int		disk1PID;
	int		term0PID;
	int		term1PID;
	int		term2PID;
	int		term3PID;	
    int		start4PID;
	int		waitPID;
    int		status;
    /*
     * Check kernel mode here.
     */
	checkForKernelMode("start3():\n");

	systemCallVec[SYS_SLEEP] = sleep;
	systemCallVec[SYS_DISKREAD] = diskRead;
	systemCallVec[SYS_DISKWRITE] = diskWrite;
	systemCallVec[SYS_TERMREAD] = termRead;	
	systemCallVec[SYS_TERMWRITE] = termWrite;	

	// initialize proc table
	for(i = 0; i < MAXPROC; i++)
	{
		emptyProc(i);
	}

	/*
	// init sem table
	for(i = 0; i < MAXSEMS; i++)
	{
		SemTable[i].id = -1;
		SemTable[i].value = -1;
		SemTable[i].startingValue = -1;
		SemTable[i].priv_mBoxID = -1;
		SemTable[i].mutex_mBoxID = -1;
	}
	numSems = 0;
	*/
	
    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
	
    semRunning = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) 
	{
		USLOSS_Console("start3(): Can't create clock driver\n");
		USLOSS_Halt(1);
    }
	
	if (debug4)
		USLOSS_Console("start3(): clockPID = %d\n", clockPID);

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
	disk0PID = fork1("Disk0 driver", Disk0Driver, NULL, USLOSS_MIN_STACK, 2);
	disk1PID = fork1("Disk1 driver", Disk1Driver, NULL, USLOSS_MIN_STACK, 2);

	if (debug4)
	{
		USLOSS_Console("start3(): disk0PID = %d\n", disk0PID);
		USLOSS_Console("start3(): disk1PID = %d\n", disk1PID);
	}

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */
	term0PID = fork1("Term0 driver", Term0Driver, NULL, USLOSS_MIN_STACK, 2);
	term1PID = fork1("Term1 driver", Term1Driver, NULL, USLOSS_MIN_STACK, 2);
	term2PID = fork1("Term2 driver", Term2Driver, NULL, USLOSS_MIN_STACK, 2);
	term3PID = fork1("Term3 driver", Term3Driver, NULL, USLOSS_MIN_STACK, 2);

	if (debug4)
	{
		USLOSS_Console("start3(): term0PID = %d\n", term0PID);
		USLOSS_Console("start3(): term1PID = %d\n", term1PID);
		USLOSS_Console("start3(): term2PID = %d\n", term2PID);
		USLOSS_Console("start3(): term3PID = %d\n", term3PID);
	}

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    start4PID = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);

    waitPID = waitReal(&status);

	if (debug4)
		USLOSS_Console("start3(): start4PID = %d, waitPID = %d\n", start4PID, waitPID);

	
    // Zap the device drivers
    zap(clockPID);  // clock driver
	//zap(disk0PID);
	//zap(term0PID);

    // eventually, at the end:
    quit(0);
    
}

int ClockDriver(char *arg)
{
    int IntResult;
    int waitResult;    
	int status;
	int currentTime;

	if (debug4)
		USLOSS_Console("ClockDriver(), pid = %d\n", getpid());

    // Let the parent know we are running
    semvReal(semRunning);
	
	// Enable Interrupts
    IntResult = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

	gettimeofdayReal(&currentTime);
	
	if (debug4)
		USLOSS_Console("ClockDriver(), currentTime = %d\n", currentTime);
	
	
    // Infinite loop until we are zap'd
    while(! isZapped()) 
	{
		waitResult = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
		
		if (waitResult != 0) 
			return 0;
	
		USLOSS_Console("ClockDriver(): status = %d\n", status);
	
	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */
	
    }

	
	
	if (debug4)
		USLOSS_Console("ClockDriver(): IntResult = %d, waitResult = %d\n", IntResult, waitResult);
	
	return 1;
}


/* sleep */
void sleep(USLOSS_Sysargs *args)
{
	checkForKernelMode("sleep()");
  
	int seconds = (int) ((long) args->arg1);
	
	if(debug4)
		USLOSS_Console("sleep(): seconds = %d\n", seconds);
	
	int status = sleepReal(seconds);
	
	args->arg4 = (void *) ((long)status);

	if(debug4)
		USLOSS_Console("sleep(): status = %d\n", status);

	//switch back to user mode
	setUserMode();
}
/* end sleep */


/* sleepReal */
int sleepReal(int seconds)
{
	int sleepPID;
	
	sleepPID = getpid();

	if(debug4)
		USLOSS_Console("sleepReal(): sleepPID = %d\n", sleepPID);

	
    //numSems++;
    //int handle = semCreateReal(value);




	
	if(seconds < 0)
		return -1;

	return 0;
}
/* end sleepReal */


/* Disk0Driver */
int Disk0Driver(char *arg)
{

	if (debug4)
		USLOSS_Console("Disk0Driver(), pid = %d\n", getpid());

    return 0;
}
/* end Disk0Driver */

/* Disk1Driver */
int Disk1Driver(char *arg)
{

	if (debug4)
		USLOSS_Console("Disk1Driver(), pid = %d\n", getpid());

    return 0;
}
/* end Disk1Driver */


/* diskRead */
void diskRead(USLOSS_Sysargs *args)
{
	checkForKernelMode("diskRead()");
	
	if (debug4)
		USLOSS_Console("diskRead(), pid = %d\n", getpid());
	
	setUserMode();
}
/* end diskRead */

/* diskWrite */
void diskWrite(USLOSS_Sysargs *args)
{
	checkForKernelMode("diskWrite()");

	if (debug4)
		USLOSS_Console("diskWrite(), pid = %d\n", getpid());
	
	setUserMode();
}
/* end diskWrite */

/* Term0Driver */
int Term0Driver(char *arg)
{

	if (debug4)
		USLOSS_Console("Term0Driver(), pid = %d\n", getpid());

    return 0;
}
/* end Term0Driver */

/* Term1Driver */
int Term1Driver(char *arg)
{

	if (debug4)
		USLOSS_Console("Term1Driver(), pid = %d\n", getpid());

    return 0;
}
/* end Term1Driver */

/* Term2Driver */
int Term2Driver(char *arg)
{

	if (debug4)
		USLOSS_Console("Term2Driver(), pid = %d\n", getpid());

    return 0;
}
/* end Term2Driver */

/* Term3Driver */
int Term3Driver(char *arg)
{

	if (debug4)
		USLOSS_Console("Term3Driver(), pid = %d\n", getpid());

    return 0;
}
/* end Term3Driver */


/* termRead */
void termRead(USLOSS_Sysargs *args)
{
	checkForKernelMode("termRead()");

	if (debug4)
		USLOSS_Console("termRead(), pid = %d\n", getpid());
	
	setUserMode();
}
/* end termRead */

/* termWrite */
void termWrite(USLOSS_Sysargs *args)
{
	checkForKernelMode("termWrite()");

	if (debug4)
		USLOSS_Console("termWrite(), pid = %d\n", getpid());
	
	setUserMode();
}
/* end termWrite */


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

  if(result) 
  {
  }
}
/* end set user mode */


/*initialize proc struct*/
void initProc(int pid) 
{
  checkForKernelMode("initProc()");

  if(debug3)
  {
    USLOSS_Console("Initializing process %d\n", pid);
  }

  int i = pid % MAXPROC;
  
  ProcTable4[i].pid = pid;
  ProcTable4[i].mboxID = MboxCreate(0, 0);
  ProcTable4[i].startFunc = NULL;
  ProcTable4[i].nextProc = NULL;
  initProcQueue(&ProcTable4[i].childQueue, CHILDREN);
}
/* end init proc */


/* empty proc struct */
void emptyProc(int pid)
{
  checkForKernelMode("emptyProc()");

  int i = pid % MAXPROC;

  ProcTable4[i].pid = -1;
  ProcTable4[i].mboxID = -1;
  ProcTable4[i].startFunc = NULL;
  ProcTable4[i].nextProc = NULL;
}
/* end emptyproc */

/*return head of given queue */
procPtr peek(procQueue* q)
{
  if(debug3)
  {
    USLOSS_Console("in peek()\n");
  }

  if(q->head == NULL)
  {
    return NULL;
  }

  return q->head;
}
/* end peek */

/* remove child process from queue */
void removeChild(procQueue* q, procPtr child)
{
  if(debug3)
  {
    USLOSS_Console("In removeChild()\n");
  }

if (q->head == NULL || q->type != CHILDREN)
    return;

  if (q->head == child) {
    dequeue(q);
    return;
  }

  procPtr previous = q->head;
  procPtr p = q->head->nextSibling;

  while (p != NULL) {
    if (p == child) {
      if (p == q->tail)
        q->tail = previous;
      else
        previous->nextSibling = p->nextSibling->nextSibling;
      q->size--;
    }
    previous = p;
    p = p->nextSibling;
  }
}
/* end removeChild */


void initProcQueue(procQueue* q, int type)
{
  if(debug3)
  {
    USLOSS_Console("in initProcQueue()\n");
  }

  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  q->type = type;
}

/* end init queue */
void initQueue(procQueue* q, int type)
{
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  q->type = type;
}
/* end init queue */

/* enqueue */
void enqueue(procQueue *q, procPtr p)
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

procPtr dequeue(procQueue* q)
{
  if(debug3)
  {
    USLOSS_Console("In dequeue()\n");
  }

  procPtr temp = q->head;

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




