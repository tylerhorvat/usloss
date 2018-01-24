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
void illegalInstructionHandler(int dev, void *arg);

int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
void enableInterrupts();
void disableInterrupts();
void checkForKernelMode(char *);
void initializeProcQueue(procQueue *, int);
void cleanProc(int);
void enqueue(procQueue* q, procPtr p);
procPtr dequeue(procQueue *);
procPtr peek(procQueue *);
int block(int);
void removeChild(procQueue *, procPtr);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
procQueue ReadyList[SENTINELPRIORITY];

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

/* current process ID */
int numProcs; 



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
    //test if in kernel mode, halt if in user mode
    checkForKernelMode("startup()\n");

    int i;
    int result; /* value returned by call to fork1() */
	
    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    for (i = 0; i < MAXPROC; i++) {
        cleanProc(i);
    }

    numProcs = 0;
    Current = &ProcTable[MAXPROC-1];
		
    // Initialize the Ready list.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing ready list\n");

    // Initialize the illegalInstruction interrupt handler
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = illegalInstructionHandler;

    // Initialize the clock interrupt handler.  Check Video5.50 if needed. will fix notes?

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error %i, ", result);
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }

    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, halting...\n");
        USLOSS_Halt(1);
    }
	
    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");
	
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
int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority)
{	
    int procSlot = -1;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    checkForKernelMode("fork1()");
    disableInterrupts();

    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        USLOSS_Console("fork1(): Stack size too small.\n");
        return -2;
    }

    // Is there room in the process table? what is the next pid?
    if (numProcs >= MAXPROC) {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): No empty slot on the process table.\n");
        return -1;
    }

    // find an empty slot in the process table
    procSlot = nextPid % MAXPROC;
    while (ProcTable[procSlot].status != EMPTY) {
        nextPid++;
        procSlot = nextPid % MAXPROC;
    }

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process pid %d in slot %d, slot status %d\n", nextPid, procSlot, ProcTable[procSlot].status);

    //fill-in entry in process table
    if (strlen(name) >= (MAXNAME - 1)) {
        USLOSS_Console("fork1(): Process name is too long. Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if (arg == NULL)
        ProcTable[procSlot].startArg[0] = '\0';
    else if (strlen(arg) >= (MAXARG - 1)) {
        USLOSS_Console("fork1(): argument too long. Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    // allocate the stack
    ProcTable[procSlot].stack = (char *) malloc(stacksize);
    ProcTable[procSlot].stackSize = stacksize;

    // error check malloc
    if (ProcTable[procSlot].stack == NULL) {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): Malloc failed. Halting...\n");
        USLOSS_Halt(1);
    }

    // set the process id
    ProcTable[procSlot].pid = nextPid++;

    // set the process priority
    ProcTable[procSlot].priority = priority;

    //increment number of processes
    numProcs++;

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)

    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    USLOSS_Console("pid: %d\n", Current->pid);

    // More stuff to do here...
    // add process to parent's (current's) list of children, if parent exists
    if (Current->pid > -1) {

        enqueue(&Current->childQueue, &ProcTable[procSlot]);
        ProcTable[procSlot].parentPtr = Current; //set parent pointer
    }

    //add process to the appropriate ready list
    enqueue(&ReadyList[priority-1], &ProcTable[procSlot]);
    ProcTable[procSlot].status = READY; // set status to READY

    // Video5.33, do not call dispatcher for sentenal
    if(startFunc != sentinel) {
        dispatcher();
    }
    
    //enable interrupts for the parent
    enableInterrupts();

    return ProcTable[procSlot].pid; //return child's pid

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

   
 /*int getpid (void){
		return pid; 		
 }*/
 
 
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
    // test if in kernel mode; halt if in user mode
    checkForKernelMode("launch()");
    disableInterrupts();

    int result;
 
    if(DEBUG && debugflag)
        USLOSS_Console("launch(): starting current process: %d\n\n", Current->pid);

    // enable interrupts
    enableInterrupts();    

    // Call the funciton passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if(DEBUG && debugflag)
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
    //test if in kernel mode, halt if in user mode
    checkForKernelMode("join()");
    disableInterrupts();

    if (DEBUG && debugflag)
        USLOSS_Console("join(): In join, pid=%d\n", Current->pid);

    //check if has children
    if(Current->childQueue.size == 0 && Current->deadChildQueue.size == 0)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("join(): No children\n");
        return -2;
    }   

    // if current has no dead children, block self and wait
    if(Current->deadChildQueue.size == 0)
    {
        if(DEBUG && debugflag)
            USLOSS_Console("join(): pid %d blocked at priority %d \n\n", Current->pid, Current->priority - 1);
        block(JBLOCKED);
    }

    if(DEBUG && debugflag)
        USLOSS_Console("join(): pid %d unblocked, dead child queue size = %d\n", Current->pid, Current->deadChildQueue.size);

    USLOSS_Console("join() 1\n");

    // get the earliest dead child
    procPtr child = dequeue(&Current->deadChildQueue);
    USLOSS_Console("join() 1.1\n");
    int childPid = child->pid;
    USLOSS_Console("join() 1.2\n");
    *status = child->quitStatus;

    USLOSS_Console("join() 2\n");

    if(DEBUG && debugflag)
        USLOSS_Console("join(): got child pid = %d, quit status = %d\n\n", childPid, *status);

    USLOSS_Console("join() 3\n");

    // cleanup mess
    cleanProc(childPid);

    if(Current->zapQueue.size != 0)
    {
        childPid = -1;
    }

    USLOSS_Console("join() 4\n");

    enableInterrupts();
    return childPid;

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
{
    // test if in kernel mode, halt if in user mode
    checkForKernelMode("quit()");
    disableInterrupts();
 
    if(DEBUG && debugflag)
        USLOSS_Console("quit(): quitting process pid=%d, parent is %d\n", Current->pid, Current->parentPtr->pid);

    // print error message and halt if process with active children calls quit
    // loop through children to find if any are active
    procPtr childPtr = peek(&Current->childQueue);
    while(childPtr != NULL)
    {
        if(childPtr->status != QUIT)
        {
            USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
            USLOSS_Halt(1);
        }
        childPtr = childPtr->nextSiblingPtr;
    }

    Current->status = QUIT;
    Current->quitStatus = status;
    dequeue(&ReadyList[Current->priority-1]);
    if(Current->parentPtr != NULL)
    {
        removeChild(&Current->parentPtr->childQueue, Current);
        enqueue(&Current->parentPtr->deadChildQueue, Current);

        if(Current->parentPtr->status == JBLOCKED)
        {
            Current->parentPtr->status = READY;
            enqueue(&ReadyList[Current->parentPtr->priority-1], Current->parentPtr);
        }
    }

    // unblock processes that zap'd this process
    while(Current->zapQueue.size > 0)
    {
        procPtr zapper = dequeue(&Current->zapQueue);
        zapper->status = READY;
        enqueue(&ReadyList[zapper->priority-1], zapper);
    }

    // remove any dead children current has from the process table
    while(Current->deadChildQueue.size > 0)
    {
        procPtr child = dequeue(&Current->deadChildQueue);
        cleanProc(child->pid);
    }

    // delete current if it has no parent
    if(Current->parentPtr == NULL)
        cleanProc(Current->pid);

    p1_quit(Current->pid);

    dispatcher(); //call dispatcher to run next process

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
void dispatcher(void)
{
    //test if in kernel mode; halt if in user mode
    checkForKernelMode("dispatcher()");
    disableInterrupts();

    procPtr nextProcess = NULL;

    // current is still running, remove it from ready list and put it back on the end
    if (Current->status == RUNNING) {
        Current->status = READY;
        dequeue(&ReadyList[Current->priority-1]);
        enqueue(&ReadyList[Current->priority-1], Current);
    }

    // Find the highest priority non-empty process queue
    int i;
    for(i = 0; i < SENTINELPRIORITY; i++)
    {
        if(ReadyList[i].size > 0)
        {
            nextProcess = peek(&ReadyList[i]);
            break;
        }
    }

    // Print message and return if the ready list is empty
    if (nextProcess == NULL)
    {
        if(DEBUG && debugflag)
            USLOSS_Console("dispatcher(): ready list is empty!\n");
        return;
    }

    if(DEBUG && debugflag)
        USLOSS_Console("dispatcher(): next process is %s\n\n", nextProcess->name);

    // update current
    procPtr old = Current;
    Current = nextProcess;
    Current->status = RUNNING; // set status to running
 
    // set slice time and time started
    if(old != Current) 
    {
        if(old->pid > -1)
            //old->cpuTime += USLOSS_DeviceInput(USLOSS_CLOCK_MS) - old->timeStarted;

        Current->sliceTime = 0;
        //Current->timeStarted = USLOSS_DeviceInput(USLOSS_CLOCK_MS);
    }

    p1_switch(old->pid, Current->pid);
    enableInterrupts();
    USLOSS_ContextSwitch(&old->state, &Current->state);

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
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
        USLOSS_Halt(1);
    }
    else 
    {
        //in kernel mode
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
    }
	
} /* disableInterrupts */


/*
 * Enable the interrupts.
 */
void enableInterrupts()
{
    //turn interrupts on iff in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        USLOSS_Console("Kernel Error: Not in kernel mode, mau not enable interrupts\n");
        USLOSS_Halt(1);
    }
    else
    {
        USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
    }	
} /* enableInterrupts */

/*function to check if in kernel mode, halts if not*/
void checkForKernelMode(char *func) 
{
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n", 
        func, Current->pid);
    }

}

void cleanProc(int pid) 
{
    checkForKernelMode("cleanProc()");
    disableInterrupts();

    int i = pid % MAXPROC;

    ProcTable[i].status = EMPTY;            //set status to be open
    ProcTable[i].pid = -1;                  //set pid to -1 to show it has not been assigned
    ProcTable[i].nextProcPtr = NULL;        //set pointers to null
    ProcTable[i].nextSiblingPtr = NULL;
    ProcTable[i].nextDeadSibling = NULL;
    ProcTable[i].startFunc = NULL;
    ProcTable[i].priority = -1;
    ProcTable[i].stack = NULL;
    ProcTable[i].stackSize =-1;
    ProcTable[i].parentPtr = NULL;

    initializeProcQueue(&ProcTable[i].childQueue, CHILDREN);
    initializeProcQueue(&ProcTable[i].deadChildQueue, DEADCHILDREN);
    initializeProcQueue(&ProcTable[i].zapQueue, ZAP);
    
    ProcTable[i].zapped = 0;
    ProcTable[i].timeStarted = -1;
    ProcTable[i].cpuTime = 0;
    ProcTable[i].sliceTime = 0;
    ProcTable[i].name[0] = 0;

    numProcs--;
    enableInterrupts();
}

void initializeProcQueue(procQueue *p, int type) 
{
    p->head = NULL;
    p->tail = NULL;
    p->size = 0;
    p->type = type;
}

/* illegalInstructionHandler */
void illegalInstructionHandler(int dev, void *arg)
{
    if(DEBUG && debugflag)
        USLOSS_Console("illegalInstructionHandler() called\n");

} /* illegalInstructionHandler */

/* add the procPtr to the back of the given queue) */
void enqueue(procQueue *q, procPtr p)
{
    if(q->head == NULL && q->tail == NULL) {
        q->head = q->tail = p;
    }
    else
    {
        if(q->type == READYLIST)
            q->tail->nextProcPtr = p;
        else if(q->type == CHILDREN)
            q->tail->nextSiblingPtr = p;
        else if(q->type == ZAP)
            q->tail->nextDeadSibling = p;
      
        q->tail = p;
    }
    q->size++;
}

/* Remove and return front of queue */
procPtr dequeue(procQueue *q)
{
    procPtr temp = q->head;
    if(q->head == NULL) 
    {
        return NULL;
    }
    else 
    {
        if(q->type == READYLIST)
            q->head = q->head->nextProcPtr;
        else if(q->type == CHILDREN)
            q->head = q->head->nextSiblingPtr;
        else if(q->type == ZAP)
            q->head = q->head->nextZapPtr;
        else
            q->head = q->head->nextDeadSibling;
    }    
    q->size--;
    return temp;
}

/* Return the head of the given queue */
procPtr peek(procQueue *q) 
{
    if(q->head == NULL)
        return NULL;

    return q->head;
}

/* block the new status */
int block(int newStatus) 
{
    if(DEBUG && debugflag)
        USLOSS_Console("block(): called\n");

    // test if in kernel mode, halt if in user mode
    checkForKernelMode("block()");
    disableInterrupts();

    Current->status = newStatus;
    dequeue(&ReadyList[(Current->priority - 1)]);
    dispatcher();

    if(Current->zapQueue.size > 0)
    {
        return -1;
    }

    return 0;
}

/* Remove the child process from the queue */
void removeChild(procQueue *q, procPtr child)
{
    if(q->head == NULL || q->type != CHILDREN)
        return;

    if(q->head == child)
    {
        dequeue(q);
        return;
    }

    procPtr prev = q->head;
    procPtr p = q->head->nextSiblingPtr;

    while(p != NULL)
    {
        if(p == child)
        {
            if(p == q->tail)
                q->tail = prev;
            else
                prev->nextSiblingPtr = p->nextSiblingPtr->nextSiblingPtr;
            q->size--;
        }
        prev = p;
        p = p->nextSiblingPtr;
    }
}












		


























