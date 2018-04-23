#include <usloss.h>
#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <providedPrototypes.h>
#include <vm.h>
#include <string.h>

static int Pager(char *buf);
static void vmInit(USLOSS_Sysargs *args);
static void vmDestroy(USLOSS_Sysargs *args);
static void FaultHandler(int, void *);

void printPageTable(int pid);
void *vmInitReal(int, int, int, int);
void vmDestroyReal();
void setUserMode();

extern void mbox_create(USLOSS_Sysargs *args_ptr);
extern void mbox_release(USLOSS_Sysargs *args_ptr);
extern void mbox_send(USLOSS_Sysargs *args_ptr);
extern void mbox_receive(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive(USLOSS_Sysargs *args_ptr);
extern int diskSizeReal(int, int*, int*, int*);


VmStats vmStats;           //for reporting virtual memory statistics
int vmStatSem;             //semaphore for mutual exclustion on vmStat
int vmInitialized = 0;     //boolean, whether the virtual memory is initialized
void *vmRegion;            //address of virtual memory region
int diskTableSize;         //number of entries in a disk table
int numPagers;             //total number of pager processes
int *pagerPids;            //process ID for each pager
int pagerMbox;             //mailbox where pagers wait for faults
int clockHand;             //fram pointed to in the clock algo
int clockSem;              //semaphore for mutual exclusion of clockHand
int frameSem;              //semaphore for mutual exclustion of frame table
FTE *frameTable;           //frame table accessed by the pagers
DTE *diskTable;            //location of pages on disk
Process processes[MAXPROC];//process table for phase5
FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
int debug5 = 0;
int oldpid = -1;
int oldcause = -1;


/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

	if(debug5)
		USLOSS_Console("start4\n");

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy; 

    // Initialize the phase 5 process table

    // Initialize other structures as needed

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(USLOSS_Sysargs *args)
{
    void *result;

    CheckMode();

    int mappings = (long) args->arg1;
    int pages = (long) args->arg2;
    int frames = (long) args->arg3;
    int pagers = (long) args->arg4;

    result = vmInitReal(mappings, pages, frames, pagers);

    if((int) (long) result < 0)
        args->arg4 = result;
    else
        args->arg4 = (void *) ((long) 0);
   
    //setUserMode();
    args->arg1 = result;
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
    int status;
    int dummy;

    CheckMode();

    // Initialize MMU
	status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
    if (status != USLOSS_MMU_OK) {
        USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
        abort();
    }
	
    // Initialize Fault Handler interrupt
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

    /*
    * Initialize page tables.
    */

    for(int i = 0; i < MAXPROC; i++)
    {
        processes[i].pid = -1;
        processes[i].vm = 0;
        processes[i].numPages = pages;
		processes[i].frames = frames;	//djf

        faults[i].pid = -1;
        faults[i].replyMbox = MboxCreate(1, 0);
        faults[i].addr = NULL;
    }


    //disk table
    int sectorSize, sectorsInTrack, numTracksOnDisk;
    diskSizeReal(1, &sectorSize, &sectorsInTrack, &numTracksOnDisk);

    diskTable = malloc(sectorsInTrack * numTracksOnDisk * sizeof(DTE));
    diskTableSize = sectorsInTrack * numTracksOnDisk;

    int trackNum = 0;
    for(int i = 0; i < diskTableSize; i +=2)
    {
        diskTable[i].track = trackNum;
        diskTable[i+1].track = trackNum++;
        diskTable[i].sector = 0;
        diskTable[i+1].sector = sectorsInTrack/2;
        diskTable[i].state = UNUSED;
        diskTable[i+1].state = UNUSED;
    }

    //frame tables
    frameTable = malloc(frames * sizeof(FTE));
    for(int i = 0; i < frames; i++)
    {
        frameTable[i].state = UNUSED;
        frameTable[i].ref = UNREFERENCED;
        frameTable[i].dirty = CLEAN;
        frameTable[i].pid = -1;
        frameTable[i].page = NULL;
    }

    frameSem = semcreateReal(1);                        //mutual exclustion for frameTable

    //clock hand
    clockHand = 0;
    clockSem = semcreateReal(1);                        //mutual exclustion for clockhand
    

    // Fork the pagers.
    numPagers = pagers;
    pagerPids = malloc(pagers * sizeof(int));
    char buf[100];
    pagerMbox = MboxCreate(pagers, sizeof(int));
    for(int i = 0; i < numPagers; i++)
    {
        pagerPids[i] = fork1("pagerProcess", Pager, buf, USLOSS_MIN_STACK, PAGER_PRIORITY);
    }

    vmInitialized = 1;

	
    // Zero out, then initialize, the vmStats structure
    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.diskBlocks = numTracksOnDisk*2;
    vmStats.freeFrames = frames;
    vmStats.freeDiskBlocks = numTracksOnDisk*2;
    vmStats.faults = 0;
    vmStats.switches = 0;
    vmStats.new = 0;
    vmStats.pageIns = 0;
    vmStats.pageOuts = 0;
    vmStats.replaced = 0;
   
    vmStatSem = semcreateReal(1);

    vmRegion = USLOSS_MmuRegion(&dummy);
	
	if(debug5)
		USLOSS_Console("vmInitReal:MmuRegion vmRegion %d\n", vmRegion);

    return vmRegion;
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */
static void
vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
   CheckMode();
   
   vmDestroyReal();
   
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{
    int status;
	
    CheckMode();

    status = USLOSS_MmuDone();
    if (status != USLOSS_MMU_OK) {
        USLOSS_Console("vmDestroyReal: MmuDone Error %d\n", status);
        //abort();
    }
	
    /*
    * Kill the pagers here.
    */
 
    //Print vm statistics.
    PrintStats();


} /* vmDestroyReal */


/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int type /* MMU_INT */,
             void * arg  /* Offset within VM region */)
{
    int cause;
	
    assert(type == USLOSS_MMU_INT);
    cause = USLOSS_MmuGetCause();
    assert(cause == USLOSS_MMU_FAULT);
    //vmStats.faults++; 
	
	//if(debug5)
		//USLOSS_Console("FaultHandler: type %d, offset %d, cause %d\n", type, arg, cause);

	int pid = getpid();	
	
    //faultMbox = MboxCreate(faults, sizeof(faults));	
    // Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the reply.
	//faults[pid % MAXPROC].pid = pid;
	//faults[pid % MAXPROC].addr = arg;
	//faults[pid % MAXPROC].replyMbox = faultMbox;
	//MboxSend(faultMbox, NULL, 0);	

	if ((pid != oldpid) && (cause != oldcause)) {
		USLOSS_Console("FaultHandler: pid %d, oldpid %d, cause %d, oldcause %d\n", pid, oldpid, cause, oldcause);
		vmStats.faults++;
	}
	
	oldpid = pid;
	oldcause = cause;

	if(debug5)
		USLOSS_Console("FaultHandler: pid %d, faults %d, cause %d\n", pid, vmStats.faults, cause);
	
} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
//    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
//    }
    return 0;
} /* Pager */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


void setUserMode() 
{
    int status;

    status = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
    if (status != USLOSS_DEV_OK) {
        USLOSS_Console("setUserMode: PsrSet Error %d\n", status);
        abort();
    }
}

