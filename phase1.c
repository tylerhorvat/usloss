/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include "kernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------- Prototypes ----------------------------------- */

int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
// djf
void enableInterrupts();
void disableInterrupts();
void checkForKernelMode(char *);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
//static procPtr ReadyList;	// Video3.52

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

/* current process ID */
short pid = -1;
int numProcs = 0; 

/* djf globals */


/* the ready list, just ints as it's just the location of the ready process
 on the process table */

int readyList[MAXPROC];

/*the blocked list of processes, currently only used in join. Again only ints
 as the it only needs to know the location of */

int blockedList[MAXPROC];

/* current process ID */
//int pid = -1;

// Video5.44
//struct psrValues psrValue; 


/* void clock_Handler(int dev, void *arg) {
	int result;
	int status;
		
	result = USLOSS_Device_Input(USLOSS_ALARM_DEV, 0, &status);
	
	if(result == USLOSS_DEV_OK); //unsure about caps, mike just kind of scribbled it on the board
		//setup semaphores here! if result is okay p the alarm semaphore 

} */

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
	int i;
    int result; /* value returned by call to fork1() */
	
    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

	// Video5.55, need to add dupprocesses, need to print some fields in table
    for(i = 0; i < MAXPROC; i++) {
        ProcTable[i].nextProcPtr = NO_CURRENT_PROCESS;		/* procPtr nextProcPtr */
		ProcTable[i].childProcPtr = NO_CURRENT_PROCESS;		/* procPtr childProcPtr */
        ProcTable[i].nextSiblingPtr = NO_CURRENT_PROCESS;	/* procPtr nextSiblingPtr */
        //ProcTable[i].name = NULL;		/* char name[MAXNAME] */
        //ProcTable[i].startArg = NULL;	/* char startArg[MAXARG] */
		//USLOSS_Context state = NULL;	/* USLOSS_Context state */
        ProcTable[i].pid = -1;			/* short pid */
        ProcTable[i].priority = 0;		/* int priority */
										/* int (* startFunc) (char *) */
        ProcTable[i].stack = NULL;		/* char *stack */
        ProcTable[i].stackSize = USLOSS_MIN_STACK;		/* unsigned int stackSize */
        ProcTable[i].status = 0;		/* int status */
		// other fields, link list code for things inside a table
		}
		
	// Initialize the Ready list.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing ready list\n");
    for(i = 0; i < MAXPROC; i++)
		readyList[i] = 0;

	// Initialize the Blocked list.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing blocked list\n");
    for(i = 0; i < MAXPROC; i++)
		blockedList[i] = 0;


    // Initialize the clock interrupt handler.  Check Video5.50 if needed. will fix notes?
	// USLOSS_IntVec[USLOSS_CLOCK_INT] = clock_Handler;	//

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error %i, ", result);
            USLOSS_Console("halting...\n");
			USLOSS_Halt(1);
		}
    }
  
    // start the test process
    /*if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error %i, ", result);
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

	// Video3, call dispatcher for start1 and not for sentinel
	dispatcher();
	
    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");*/
	
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
   Returns - the process id of the created child or
				-1 if no child could be created or 
				if priority is not between max and min priority or
				-2 if stack size is too small
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{	
	int procSlot = -1;
	void *stack;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // If not in kernal mode, error and halt
    /*if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("fork1(): Not in kernel mode.  Halting...\n");
        USLOSS_Halt(1);
    }*/
    checkForKernelMode("fork1()");

	// If not in kernal mode, error and halt, test change to user mode.
	// Video5.34-46
	//struct psrBits z;
	//z.curMode = 0;
	//z.curIntEnable = 1;	

	/* psrValue.integerPart = USLOSS_PsrGet();
	if (psrValue.bits.curMode == 0) {
        USLOSS_Console("fork1(): Not in kernel mode.  Halting...\n");
        USLOSS_Halt(1);
    } */
	
	// Disable interrupts
	// Video3, need to disable interrupts somewhere
	// Video5.29, enable interrtups when returns to test code
    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): disable interrupts\n");
	disableInterrupts();

	// If name is too long, error and halt 
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
	
    // If stack size is too small, error and return -2
    if (stacksize < USLOSS_MIN_STACK){
		USLOSS_Console("fork1(): Stack size too small.\n");
        return -2;
	}

	// If Process table is full or inputs are out of range or missing, error and return -1 
    if ((numProcs == MAXPROC) || 		// Process table full
		(priority < MAXPRIORITY) ||		// Priority is too low
		(priority > MINPRIORITY + 1) ||	// Priority is too high
        (name == NULL) || 				// No process name
		(startFunc == NULL)) {			// No start funtion
		USLOSS_Console("fork1(): Process Table full or input errors.\n");
        return -1;
	}	

	// What is the next PID?
    // fill-in entry in process table */
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): Argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);
	ProcTable[procSlot].stackSize = stacksize; 

	// Allocate stack
	stack = malloc(sizeof(stacksize));

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    //if (DEBUG && debugflag)
    //    USLOSS_Console("fork1(): initialize context\n");

	// Enable interrupts
    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): enable interrupts\n");
    
    enableInterrupts();

    USLOSS_Console("pass 1\n");
 	
    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    USLOSS_Console("pass 2\n");

    // for future phase(s)
    //p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...
    // Video5.33, do not call dispatcher for sentenal
    dispatcher();
    
    USLOSS_Console("pass 3\n");
    // djf
    numProcs++; 
    
    return procSlot;

    //return -1;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* void dumpProcesses(void){
	int i;
	for(i = 0; i < MAXPROC; i++){
		if(ProcTable[i].status != QUIT){
			char info[200];
			char *status;
			//I forgot which values matched to which status so I guessed.
			if(ProcTable[i].status == READY)
				status = "ready";
			else if(ProcTable[i].status == RUNNING)
				status = "running";
			else if(ProcTable[i].status == BLOCKED){
				status = "blocked";
			}
			procPtr *child = ProcTable[i].childProcPtr;
			int childCount = 0;
			while(child != NULL){
				{
					childCount++;
					child = ProcTable[i].childProcPtr.nextSiblingPtr; 
				}
			}
			//Prints to the string.
			snprintf(info, sizeof info, 
				"Process Name : %s\nPID: %d\nParent's PID: %d\nPriority: %d\nProcess status: %s\nNumber of Children: %d\nTime Consumed: %d\n\n"
				, ProcTable[i].name, i, ProcTable[i].parent, procTable[i].priority, status, childCount, procTable[i].cycles);
			//Just sort of modeled the stub in usloss.h, not sure if "..." is needed or denotes something else
			USLOSS_Console(info);
		}
	}
} */

   
 int getpid (void){
		return pid; 		
 }
 
 
 /* ------------------------------------------------------------------------
   Name - zap
   Purpose - to zap a process with the passed in PID
   Parameters - int pid
   Returns - 0 and -1 depending on PID status 
   Side Effects - changes the zapped variable in the passed in PID 
   ------------------------------------------------------------------------ */
 /* int zap(int pid){
	
 if(ProcTable[pid].pid == Current.pid || ProcTable[pid].pid == -1) {
		USLOSS_Console("zap(): current process tried to kill itself or attempted to zap a nonexistant process\n");
		USLOSS_Halt(1)
	}
	ProcTable[pid].zapped = 1; 
	while(ProcTable[pid].status != QUIT);
	
	if(Current.zapped == 1)
		return -1;
	else if(ProcTable[pid].status == QUIT)
		return 0; 	
 }
   
 int isZapped(void) {
	if(Current.zapped == 1)
		return 1;
	else
		return 0; 
 } */
 
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
    USLOSS_Console("launch 1\n");

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

/* 	childNode *p = ProcTable[pid].childList;
	
	blockedList[pid] = pid;

	while(ProcTable[pid].sem->value == 0); */

	// djf otherwise the program is placed on a blocked list so the dispatcher won't select it
    // setting the flag back, and returning the PID of the process that quit or kill saved earlier     	

    checkForKernelMode("join()");
    disableInterrupts();

    if (DEBUG && debugflag)
        USLOSS_Console("join(): In join, pid=%d\n", Current->pid);

    //check if has children
    



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

  /*   // sets it's status so join can retrieve it later 
    ProcTable[pid].status = status; 
	//removeProcess(pid);     

	dispatcher();  */	
}  /* quit */
 

/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context state of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)	// djf should be called in numerious places, quit etc.
{
    // procPtr nextProcess = NULL;
    // p1_switch(Current->pid, nextProcess->pid);
	// video5.32,add to ready list, dispatcher decides who to run next

	/*running up to the max priority, and checking each piority. */
    int currentPriority = 6;
    int currentPID = 0;
	int i;
    USLOSS_Console("dispatcher 1\n");

    for(i = 0; i < MAXPROC; i++) {
        if(ProcTable[i].status == 1 && ProcTable[i].priority < currentPriority ) {
            if(blockedList[i] == 0) {
                currentPriority = ProcTable[i].priority;
                currentPID = i;
            }
        }
    }

    USLOSS_Console("dispatcher 2\n");
    
    USLOSS_ContextSwitch(&ProcTable[pid].state, &ProcTable[i].state);
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
    while (1) {
        checkDeadlock();
        USLOSS_WaitInt();
    }

    return 0;	
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
	// Video5.105 Is anyone else running
	// If no processes running, terminate normal, if process running, terminate abnormal
    //USLOSS_Halt(1);	// Normal termination
    //USLOSS_Halt(0);	// Abnormal termination
	
} /* checkDeadlock */


/*
 * Disables the interrupts.  
 */
void disableInterrupts()
{
    int result;
	
    // If in kernal mode, then disable interrupts, else error and halt
    if (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet())
        result = USLOSS_PsrSet(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT);
    else
    {
        USLOSS_Console("disableInterrupts(): Not in kernel mode.  Halting...\n");
        USLOSS_Halt(1);	
    }

    if (result == USLOSS_ERR_INVALID_PSR) {
    }
	
} /* disableInterrupts */


/*
 * Enable the interrupts.
 */
void enableInterrupts()
{
    int result;

    // If in kernal mode, then enable interrupts, else error and halt
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
		result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    else {
        USLOSS_Console("enableInterrupts(): Not in kernel mode.  Halting...\n");
        USLOSS_Halt(1);	
    }	

    USLOSS_Console("enableInterrupts 1\n");

    if (result == USLOSS_ERR_INVALID_PSR) {
    }
	
} /* enableInterrupts */

void checkForKernelMode(char *func) {

   if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
      USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n", func, Current->pid);
   }

}
/* Video3 exampled
		int *(*z)(int, float, double)
		int *b(int xray, float yoke, double zibra)
		z = b;		
		if (strcmp(x, y) == 0)
		int xray = start1("Hello")
*/		
