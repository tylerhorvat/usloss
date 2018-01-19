/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
// djf
void enableInterrupts();
void disableInterrupts();


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;	//djf this is priority queue? need to insert in right place, video 3.52

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; /* value returned by call to fork1() */

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
	// djf
	int i;
    for(i = 0; i < MAXPROC; i++) {
        procTable[i].nextProcPtr = NO_CURRENT_PROCESS;		/* procPtr nextProcPtr */
		procTable[i].childProcPtr = NO_CURRENT_PROCESS;		/* procPtr childProcPtr */
        procTable[i].nextSiblingPtr = NO_CURRENT_PROCESS;	/* procPtr nextSiblingPtr */
        //procTable[i].name = NULL;		/* char name[MAXNAME] */
        //procTable[i].startArg = NULL;	/* char startArg[MAXARG] */
		USLOSS_Context state = 			/* USLOSS_Context state */
        procTable[i].pid = 0;			/* short pid */
        procTable[i].priority = 0;		/* int priority */
										/* int (* startFunc) (char *) */
        procTable[i].stack = NULL;		/* char *stack */
        procTable[i].stackSize = 0;		/* unsigned int stackSize */
        procTable[i].status = 0;		/* int status */
		}
		
	// Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    ReadyList = NULL;

    // Initialize the clock interrupt handler

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
			USLOSS_Halt(1);
		}
    }
  
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

	//djf call dispatcher for start 1 and not for sentinel, video 3
	
    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int procSlot = -1;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

	// Disable interrupts
	disableInterrupts();		// djf need to disable interrupts somewhere, video 3

    // Halt if in user mode
    if !(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) {					// if not in kernel mode 
        USLOSS_Console("fork1(): Not in kernel mode.  Halting...\n");	// error and halt
        USLOSS_Halt(1);
    }

	// Halt if name is too long 
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
	
    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK)	// 
        return -2;

    // Return if Process table is full or inputs are out of range or missing 
    if ((numProcs == MAXPROC) || 		// Process table full
		(priority < MAXPRIORITY) ||		// Priority is too low
		(priority > MINPRIORITY) ||		// Priority is too high
        (name == NULL) || 				// No process name
		(startFunc == NULL))			// No start funtion
        return -1;

	// What is the next PID?
    // fill-in entry in process table */
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)

    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    //p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...
    // djf
    return pid;

    //return -1;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
    enableInterrupts();		// djf, launch should now be done

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
	// djf join have any children quit, tell me about 1 child that quit, what if several have quit the one that quit first, need queue
	// block by taking off ready list need to add block on join but if not dont call status = called join when no child has quit 

	childNode *p = procTable[pid].childList;
	
	blockedList[pid] = pid;

	while(procTable[pid].sem->value == 0);

	// djf otherwise the program is placed on a blocked list so the dispatcher won't select it
    // setting the flag back, and returning the PID of the process that quit or kill saved earlier     	

    return -1;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{	// djf process that called quit will be removed from ready list and cleanup, video
    // p1_quit(Current->pid);

    /* sets it's status so join can retrieve it later */ 
    procTable[pid].status = status; 
	removeProcess(pid);     
	dispatcher(); 	

} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)	// djf should be called in numerious places, quit etc.
{
    procPtr nextProcess = NULL;

    // p1_switch(Current->pid, nextProcess->pid);
	
	// djf
	/*running up to the max priority, and checking each piority. */
    int currentPriority = 6;
    int currentPID = 0;
    
	int i;
    for(i = 0; i < MAXPROC; i++) {
        if(procTable[i].status == 1 && procTable[i].priority < currentPriority ) {
            if(blockedList[i] == 0) {
                currentPriority = procTable[i].priority;
                currentPID = i;
            }
        }
    }
    
    USLOSS_ContextSwitch(&procTable[pid].context, &procTable[i].context);
    pid = currentPID;	
	
	
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
} /* checkDeadlock */


/*
 * Disables the interrupts.  
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS

    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE_MASK)					// if in kernal mode
		USLOSS_PsrSet(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT_MASK);	// disable interrupts
	else {																// else error and halt
        USLOSS_Console("disableInterrupts(): Not in kernel mode.  Halting...\n");
        USLOSS_Halt(1);	
	}
	
} /* disableInterrupts */


/*
 * Enable the interrupts.
 */
void enableInterrupts()
{
    // turn the interrupts ON iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS

    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE_MASK)				// if in kernal mode
		USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);	// enable interrupts
	else {															// else error and halt
        USLOSS_Console("enableInterrupts(): Not in kernel mode.  Halting...\n");
        USLOSS_Halt(1);	
	}	
	
} /* enableInterrupts */

/* djf example, video 3
		int *(*zap)(int, float, double)
		int *bibble(int xray, float yoke, double zibra)
		zap = bibble;		
		if (strcmp(x, y) ==0)
		int xray = start1("Hello")
*/		
