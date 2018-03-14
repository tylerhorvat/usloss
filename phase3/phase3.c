#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
<<<<<<< HEAD
#include <sems.h>
#include <string.h>

/*------------------ Prototypes ----------------------*/
void checkForKernelMode(char *);
void nullsys3(USLOSS_Sysargs *);
void spawn(USLOSS_Sysargs *);
void wait(USLOSS_Sysargs *);
void terminate(USLOSS_Sysargs *);
void semCreate(USLOSS_Sysargs *);
int semCreateReal(int);
void semP(USLOSS_Sysargs *);
void semPReal(int);
void semV(USLOSS_Sysargs *);
void semVReal(int);
void semFree(USLOSS_Sysargs *);
int semFreeReal(int);
void getTimeOfDay(USLOSS_Sysargs *);
void cpuTime(USLOSS_Sysargs *);
void getPID(USLOSS_Sysargs *);
int spawnReal(char *, int(*)(char *), char *, int, int);
int spawnLaunch(char *);
int waitReal(int *);
void terminateReal(int);
void emptyProc3(int);
void initProc(int);
void setUserMode();
void initProcQueue(procQueue*, int);
void enqueue(procQueue*, procPtr3);
procPtr3 dequeue(procQueue*);
procPtr3 peek(procQueue*);
void removeChild(procQueue*, procPtr3);
extern int start3();
int getTime();


/*------------------ Global Variables ----------------*/
int debug3 = 0;

semaphore SemTable[MAXSEMS];
int numSems;
procStruct3 ProcTable3[MAXPROC];

int start2(char *arg)
{
    int pid;
    int status;

	// Check kernel mode here.
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
  systemCallVec[SYS_TERMINATE] = wait;
  systemCallVec[SYS_SEMCREATE] = semCreate;
  systemCallVec[SYS_SEMP] = semP;
  systemCallVec[SYS_SEMV] = semV;
  systemCallVec[SYS_SEMFREE] = semFree;
  systemCallVec[SYS_GETTIMEOFDAY] = getTimeOfDay;
  systemCallVec[SYS_CPUTIME] = cpuTime;
  systemCallVec[SYS_GETPID] = getPID;

  // initialize proc table
  for(i = 0; i < MAXPROC; i++)
  {
    emptyProc3(i);
  }

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
  if(debug3)
  {
    USLOSS_Console("Spawning start3...\n");
  }
  pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
  pid = waitReal(&status);

  if(debug3)
  {
    USLOSS_Console("Quitting start2...\n");
  }

  quit(pid);
  return -1;

} /* start2 */

<<<<<<< HEAD
/*initialize proc struct*/
void initProc(int pid) 
{
  checkForKernelMode("initProc()");

  int i = pid % MAXPROC;
  
  ProcTable3[i].pid = pid;
  ProcTable3[i].mboxID = MboxCreate(0, 0);
  ProcTable3[i].startFunc = NULL;
  ProcTable3[i].nextProc = NULL;
  initProcQueue(&ProcTable3[i].childQueue, CHILDREN);
}
/* end init proc */


/* empty proc struct */
void emptyProc3(int pid)
{
  checkForKernelMode("emptyProc()");

  int i = pid % MAXPROC;

  ProcTable3[i].pid = -1;
  ProcTable3[i].mboxID = -1;
  ProcTable3[i].startFunc = NULL;
  ProcTable3[i].nextProc = NULL;
}
/* end emptyproc */

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
  USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
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

/* remove child process from queue */
void removeChild(procQueue* q, procPtr3 child)
{
  if(q->head == NULL || q->type != CHILDREN)
    return;

  if(q->head == child)
  {
    dequeue(q);
    return;
  }

  procPtr3 previous = q->head;
  procPtr3 p = q->head->nextSibling;

  while(p != NULL)
  {
    if(p == child)
    {
      if(p == q->tail)
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

/*return head of given queue */
procPtr3 peek(procQueue* q)
{
  if(q->head == NULL)
  {
    return NULL;
  }

  return q->head;
}
/* end peek */

/* spaen */
void spawn(USLOSS_Sysargs *args)
{
  checkForKernelMode("spawn");

  int (*func)(char *) = args->arg1;
  char *arg = args->arg2;
  int stack_size = (int) ((long)args->arg3);
  int priority = (int) ((long)args->arg4);
  char *name = (char *)(args->arg5);

  if(debug3)
  {
    USLOSS_Console("spawn(): arges are: name = %s, stack_size: %d, priority: %d\n", name, stack_size, priority);
  }

  int pid = spawnReal(name, func, arg, stack_size, priority);
  int status = 0;

  if(debug3)
  {
    USLOSS_Console("spawn(): spawned pid %d\n", pid);
  }

  // terminate if zapped
  if(isZapped())
  {
    terminateReal(1);
  }

  // switch to user mode
  setUserMode();

  // switch back to kernel mode
  args->arg1 = (void *) ((long)pid);
  args->arg4 = (void *) ((long)status);
}
/* end spawn */

/* spawnReal */
int spawnReal(char *name, int (*func)(char *), char *arg, int stack_size, int priority)
{
  checkForKernelMode("spawnReal()");

  if(debug3)
  {
    USLOSS_Console("spawnReal(): forking process %s\n", name);
  }

  // fork process and get pid
  int pid = fork1(name, spawnLaunch, arg, stack_size, priority);

  if(debug3)
  {
    USLOSS_Console("spawnReal(): forked process name = %s, pid = %d\n");
  }

  // return -1 if fork failed
  if(pid < 0)
  {
    return -1;
  }

  //get child table entry
  procPtr3 child = &ProcTable3[pid % MAXPROC];
  enqueue(&ProcTable3[getpid() % MAXPROC].childQueue, child);

  // set up proc table if not done
  if(child->pid < 0)
  {
    if(debug3)
    {
      USLOSS_Console("spawnReal(): initialiing prooc table entry for pid %d\n", pid);
    }
    initProc(pid);
  }

  child->startFunc = func;
  child->previous = &ProcTable3[getpid() % MAXPROC];

  // unblcok the process so spawnlaunch can start it
  MboxCondSend(child->mboxID, 0, 0);
 
  return pid;
}
/* end spawnReal */

int spawnLaunch(char *startArg)
{
  checkForKernelMode("spawnLaunch()");

  if(debug3)
  {
    USLOSS_Console("spawnLaunch(): launced pid = %d\n", getpid());
  }

  // terminate if zapped
  if(isZapped())
  {
    terminateReal(1);
  }

  // get process info
  procPtr3 proc = &ProcTable3[getpid() % MAXPROC];

  // set up proc table if not done already
  if(proc->pid < 0)
  {
    if(debug3)
    {
      USLOSS_Console("spawnLaunch(): initializing proc table entry for pid %d\n", getpid());
    }
    initProc(getpid());

    // Blcok until
    MboxReceive(proc->mboxID, 0, 0); 
  }

  // switch to user mode
  setUserMode();

  if(debug3)
  {
    USLOSS_Console("spawnLaunch(): starting proces %d\n", proc->pid);
  }

  // call the function to start the process
  int status = proc->startFunc(startArg);

  if(debug3)
  {
    USLOSS_Console("spawnLaunch(): terminating process %d with status %d\n", proc->pid, status);
  }

  Terminate(status);
 
  return 0;
}
/* end spawnLaunch */

/* wait */
void wait(USLOSS_Sysargs *args)
{
  checkForKernelMode("wait()");

  int *status = args->arg2;
  int pid = waitReal(status);

  if(debug3)
  {
    USLOSS_Console("wait(): joined with child pid = %d, status = %d\n", pid, *status);
  }

  args->arg1 = (void *) ((long) pid);
  args->arg2 = (void *) ((long) *status);
  args->arg4 = (void *) ((long) 0);

  // terminate if zapped
  if(isZapped())
  {
    terminateReal(1);
  }

  //swith back to user mode
  setUserMode();
}
/* end wait */

/* waitReal */
int waitReal(int *status)
{
  checkForKernelMode("waitReal()");

  if(debug3)
  {
    USLOSS_Console("in waitReal\n");
  }

  int pid = join(status);
  return pid;
}
/* end waitReal */

/* terminate */
void terminate(USLOSS_Sysargs *args)
{
  checkForKernelMode("terminate()");
  
  int status = (int) ((long) args->arg1);
  terminateReal(status);

  //switch back to user mode
  setUserMode();
}
/* end terminate */

void terminateReal(int status)
{
  checkForKernelMode("terminateReal()");

  if(debug3)
  {
    USLOSS_Console("terminateReal(): terminating pid %d, status = %d\n", getpid(), status);
  }

  // zap children
  procPtr3 proc = &ProcTable3[getpid() % MAXPROC];
  while(proc->childQueue.size > 0)
  {
    procPtr3 child = dequeue(&proc->childQueue);
    zap(child->pid);
  }

  // remove self from partens list
  removeChild((&proc->previous->childQueue), proc);
  quit(status);
}
/* end terminateReal */

/* semCreat */
void semCreate(USLOSS_Sysargs *args)
{
  checkForKernelMode("semCreate()");

  int value = (long) args->arg1;

  if(value < 0 || numSems == MAXSEMS)
  {
    args->arg4 = (void *) (long) -1;
  }
  else
  {
    numSems++;
    int handle = semCreateReal(value);
    args->arg1 = (void*) (long) handle;
    args->arg4 = 0;
  }

  if(isZapped())
  {
    terminateReal(0);
  }
  else
  {
    setUserMode();
  }
}
/* end semCreate */

/* semCreateReal */
int semCreateReal(int value)
{
  checkForKernelMode("semCreateReal()");

  int i;
  int priv_mBoxID = MboxCreate(value, 0);
  int mutex_mBoxID = MboxCreate(1, 0);

  MboxSend(mutex_mBoxID, NULL, 0);

  for(i = 0; i < MAXSEMS; i++)
  {
    if(SemTable[i].id == -1)
    {
      SemTable[i].id = i;
      SemTable[i].value = value;
      SemTable[i].startingValue = value;
      SemTable[i].priv_mBoxID = priv_mBoxID;
      SemTable[i].mutex_mBoxID = mutex_mBoxID;
      initProcQueue(&SemTable[i].blockedProcs, BLOCKED);
      break;
    }
  }

  int j;
  for(j = 0; j < value; j++)
  {
    MboxSend(priv_mBoxID, NULL, 0);
  }

  MboxReceive(mutex_mBoxID, NULL, 0);

  return SemTable[i].id;
}
/* end semCreateReal */

void semP(USLOSS_Sysargs *args)
{
  checkForKernelMode("semP()");
 
  int handle = (long) args->arg1;

  if(handle < 0)
  {
    args->arg4 = (void *) (long) -1;
  }
  else
  {
    args->arg4 = 0;
    semPReal(handle);
  }

  if(isZapped())
  {
    terminateReal(0);
  }
  else
  {
    setUserMode();
  }
}
/* end semP */

void semPReal(int handle)
{
  checkForKernelMode("semPReal()");

  // get mutex on this semaphore
  MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);

  // block if value is 0
  if(SemTable[handle].value == 0)
  {
    enqueue(&SemTable[handle].blockedProcs, &ProcTable3[getpid() % MAXPROC]);
    MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);

    int result = MboxReceive(SemTable[handle].priv_mBoxID, NULL, 0);

    if(SemTable[handle].id < 0)
    {
      terminateReal(0);
    }

    MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);

    if(result < 0)
    {
      USLOSS_Console("semP(): bad receive\n");
    }
  }
  else
  {
    SemTable[handle].value -= 1;

    int result = MboxReceive(SemTable[handle].priv_mBoxID, NULL, 0);

    if(result < 0)
    {
      USLOSS_Console("semP(): bad receive\n");
    }

  }
  MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);
}
/* end semPReal */

void semV(USLOSS_Sysargs *args)
{
  checkForKernelMode("semV()");

  int handle = (long) args->arg1;

  if(handle < 0)
  {
    args->arg4 = (void *) (long) -1;
  }
  else
  {
    args->arg4 = 0;
  }

  semVReal(handle);

  if(isZapped())
  {
    terminateReal(0);
  }
  else
  {
    setUserMode();
  }
}
/* end semV */

void semVReal(int handle)
{
  checkForKernelMode("semVReal()");

  // unblock blocked processes
  if(SemTable[handle].blockedProcs.size > 0)
  {
    dequeue(&SemTable[handle].blockedProcs);

    MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);

    MboxSend(SemTable[handle].priv_mBoxID, NULL, 0);

    MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);
  }
}
/* end semVReal */

void semFree(USLOSS_Sysargs *args)
{
  checkForKernelMode("semFree()");

  int handle = (long) args->arg1;

  if(handle < 0)
  {
    args->arg4 = (void *) (long) -1;
  }
  else
  {
    args->arg4 = 0;
    int value = semFreeReal(handle);
    args->arg4 = (void *) (long) value;
  }

  if(isZapped())
  {
    terminateReal(0);
  }
  else
  {
    setUserMode();
  }
}
/* end semFree */

int semFreeReal(int handle)
{
  checkForKernelMode("semFreeReal()");

  int mutexID = SemTable[handle].mutex_mBoxID;
  MboxSend(mutexID, NULL, 0);

  int privID = SemTable[handle].priv_mBoxID;

  SemTable[handle].id = -1;
  SemTable[handle].value = -1;
  SemTable[handle].startingValue = -1;
  SemTable[handle].priv_mBoxID = -1;
  SemTable[handle].mutex_mBoxID = -1;
  numSems--;

  // terminate procs waiting on this semaphore
  if(SemTable[handle].blockedProcs.size > 0)
  {
    while(SemTable[handle].blockedProcs.size > 0)
    {
      dequeue(&SemTable[handle].blockedProcs);
      int result = MboxSend(privID, NULL, 0);
      if(result < 0)
      {
        USLOSS_Console("semFreeREal(): send error\n");
      }
    }
    MboxReceive(mutexID, NULL, 0);
    return 1;
  }
  else
  {
    MboxReceive(mutexID, NULL, 0);
    return 0;
  }
}
/*end semFreeReal */

void getTimeOfDay(USLOSS_Sysargs *args)
{
  checkForKernelMode("getTimeOfDay()");
  *((int *)(args->arg1)) = getTime();
}
/* end getTime ofDay */

int getTime() 
{
  int result, unit = 0, status;

  result = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, unit, &status);

  if(result == USLOSS_DEV_INVALID)
  {
    USLOSS_Console("clock device invalid.\n");
    USLOSS_Halt(1);
  }

  return status;
}

void cpuTime(USLOSS_Sysargs *args)
{
  checkForKernelMode("cpuTime()");
  *((int *)(args->arg1)) = readtime();
}
/* end cpuTime */

void getPID(USLOSS_Sysargs *args)
{
  checkForKernelMode("getPID()");
  *((int *)(args->arg1)) = getpid();
}
/* end getPID */

void nullsys3(USLOSS_Sysargs *args)
{
  USLOSS_Console("nullsys3(): Invalid syscall %d. Terminating...\n", args->number);
  terminateReal(1);
}
/* end nullsys3 */