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

extern void mbox_create(USLOSS_Sysargs *args);
extern void mbox_release(USLOSS_Sysargs *args);
extern void mbox_send(USLOSS_Sysargs *args);
extern void mbox_receive(USLOSS_Sysargs *args);
extern void mbox_condsend(USLOSS_Sysargs *args);
extern void mbox_condreceive(USLOSS_Sysargs *args);

VmStats  vmStats;
FTE *frameTable = NULL;
int pagerPids[MAXPAGERS];
int faultM;
int clockHand = 0, track = 0, sector = 0, vmInitialized = 0;
void *vmRegion = NULL;
int clockMutex;
FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
Process processes[MAXPROC];



static int Pager(char *buf);
static void FaultHandler(int type, void * offset);
static void vmInit(USLOSS_Sysargs *args);
static void vmDestroy(USLOSS_Sysargs *args);

void printPageTable(int pid);
void switchToUser();
void *vmInitReal(int, int, int, int);
void vmDestroyReal();


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

    // Initialize other structures as needed
    for(int i =0; i < MAXPAGERS; i++)
        pagerPids[i] = -1;

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
    CheckMode();

    args->arg1 = vmInitReal((long)args->arg1, (long)args->arg2, (long)args->arg3, (long)args->arg4);

    args->arg4 = args->arg1 < 0 ? args->arg1 : (void *)(long)0;
    switchToUser();

} /* vmInit */

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
vmDestroy(USLOSS_Sysargs *args)
{
    CheckMode();

    vmDestroyReal();
    switchToUser();
} /* vmDestroy */

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

    /* check for invalid inputs */
    if (mappings < 1 || pages < 1 || frames < 1 || pagers < 1) {
        return ((void *)(long)-1);
    }
    if (pagers > MAXPAGERS) {
        return ((void *)(long)-1);
    }
    if (mappings != pages) { 
        return ((void *)(long)-1);
    }


    /* initialize MMU */
    status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
    if (status != USLOSS_MMU_OK) 
    {
        USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
        abort();
    }
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

    //initialize faults
    for (int i = 0; i < MAXPROC; i++)
    {
        faults[i].pid = -1;
        faults[i].address = NULL;
        faults[i].reply = MboxCreate(1,0);
    }

    //Initialize frame table.
    frameTable = (FTE*)malloc(frames * sizeof(FTE));
    for (int i = 0; i < frames; i++)
    {
        frameTable[i].pid = -1;
        frameTable[i].state = UNUSED;
        frameTable[i].page = -1;
    }

    //initialize page table.
    for (int i = 0; i < MAXPROC; i++)
    {
        processes[i].numPages = pages;
        processes[i].pageTable = NULL;
    }

    //create the fault mailbox or semaphore
    faultM = MboxCreate(pagers, sizeof(FaultMsg));
    clockMutex = semcreateReal(1);
    
    //fork pagers
    for (int i = 0; i < pagers; i++)
    {
        char arg[5];
        sprintf(arg, "%d", i);
        pagerPids[i] = fork1("Pager", Pager, arg, 8 * USLOSS_MIN_STACK, PAGER_PRIORITY);
    }

    //Set up disk blocks
    int diskBlocks, sectorSize, trackSize;
    diskSizeReal(1, &sectorSize, &trackSize, &diskBlocks);

    /*
     * Zero out, then initialize, the vmStats structure
     */
    memset((char *) &vmStats, 0, sizeof(VmStats));
    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.freeFrames = frames;
    vmStats.diskBlocks = diskBlocks * 2;
    vmStats.freeDiskBlocks = diskBlocks * 2;
    vmStats.switches = 0;
    vmStats.faults = 0;
    vmStats.new = 0;
    vmStats.pageIns = 0;
    vmStats.pageOuts = 0;
    vmStats.replaced = 0;

    vmInitialized = 1;

    return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */

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
 *      Stats are printed to the USLOSS_Console.
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
    vmRegion = NULL;
    vmInitialized = 0;

    int errorCode;
    CheckMode();
    errorCode = USLOSS_MmuDone();
    if (errorCode != USLOSS_MMU_OK)
    {
        USLOSS_Console("ERROR: Turn off MMU failed! Exiting....\n");
        USLOSS_Halt(1);        
    }

    //kill pagers
    int status;
    for(int i = 0; i < MAXPAGERS; i++)
    {
        if (pagerPids[i] != -1)
        {
            MboxSend(faultM, NULL, 0);
            zap(pagerPids[i]);
            join(&status);
        }
    }

    // release the faults message and falut mailbox.
    for (int i = 0; i < MAXPROC; i++) {
        MboxRelease(faults[i].reply);
    }
    MboxRelease(faultM);

    //print stats
    PrintStats();
} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler --
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
             void * offset  /* Offset within VM region */)
{
    int result;
    assert(type == USLOSS_MMU_INT);
    result = USLOSS_MmuGetCause();
    assert(result == USLOSS_MMU_FAULT);
    vmStats.faults++;


    int pid = getpid();
    faults[pid % MAXPROC].pid = pid;
    faults[pid % MAXPROC].address = (void*)((long)(processes[pid % MAXPROC].pageTable) + (long)offset);
    //block the current process
    MboxSend(faultM, &pid, sizeof(int));
    MboxReceive(faults[pid % MAXPROC].reply, NULL, 0);
} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager --
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
    FaultMsg *fault = NULL;
    Process *proc = NULL;
    PTE* new = NULL;
    PTE* old = NULL;
    int newNum = 0, oldNum, frame = 0, pid = 0, result = -1, access = -1;
    char buff[USLOSS_MmuPageSize()];

    while(1) 
    {
        //get pid
        MboxReceive(faultM, &pid, sizeof(int));
        //get fault message
        fault = &faults[pid%MAXPROC];
        if(isZapped())
            break;
        proc = &processes[pid%MAXPROC];
        //calc new page number
        newNum = ((long)fault->address - (long)proc->pageTable) / USLOSS_MmuPageSize();
        //get new page
        new =  &(proc->pageTable[newNum]);
        
        
        sempReal(clockMutex);
        if (vmStats.freeFrames > 0)
        {
            for(int i = 0; i < vmStats.frames; i++)
            {
                if (frameTable[i].state == UNUSED)
                {
                    frame = i;
                    break;
                }
            }
        }
        else
        {
            frame = -1;
            while(frame == -1)
            {
                result = USLOSS_MmuGetAccess(clockHand, &access);
                if ((access & USLOSS_MMU_REF) == 0)
                    frame = clockHand;
                else
                    result = USLOSS_MmuSetAccess(clockHand, access & 0x2);
                clockHand = (clockHand + 1) % vmStats.frames;
            }
        }
        semvReal(clockMutex);

        result = USLOSS_MmuGetAccess(frame, &access);
        result = USLOSS_MmuSetAccess(frame, access & 0x2);

       
        if (frameTable[frame].state == UNUSED)
            vmStats.freeFrames--;

        else
        {
            result = USLOSS_MmuGetAccess(frame, &access);
            //update old page
            oldNum = frameTable[frame].page;
            old = &processes[frameTable[frame].pid].pageTable[oldNum];
            old->frame = -1;
            old->state = CORE;
            

            result = USLOSS_MmuMap(TAG, 0, frame, USLOSS_MMU_PROT_RW);
            if (result != USLOSS_MMU_OK)
            {
                USLOSS_Console("Pager():\t mmu map failed1 in %d, %d, %d\n", 0, frame, result);
                USLOSS_Halt(1);                       
            }
            memcpy(&buff, vmRegion, USLOSS_MmuPageSize());
            
            if(access >= 2)
            {
                result = USLOSS_MmuUnmap(frame, oldNum);
                
                if (old->diskBlock == UNUSED)
                {
                    //USLOSS_Console("\nchild(%d) use disk block %d\n\n", pid, frameTable[frame].pid);
                    old->diskBlock++;
                    old->track = track / 2;
                    old->sector = sector % 2 == 0 ? 0 : USLOSS_MmuPageSize()/USLOSS_DISK_SECTOR_SIZE;
                    track++;
                    sector++;
                }
                diskWriteReal (SWAP, old->track, old->sector, USLOSS_MmuPageSize()/USLOSS_DISK_SECTOR_SIZE, &buff);
                vmStats.pageOuts++;
            }
            result = USLOSS_MmuUnmap(TAG, 0);
        }
        
        if(new->state == UNUSED)
        {
            result = USLOSS_MmuMap(TAG, 0, frame, USLOSS_MMU_PROT_RW);
            if (result != USLOSS_MMU_OK)
            {
                USLOSS_Console("Pager():\t mmu map failed2 in %d, %d, %d\n", 0, frame, result);
                USLOSS_Halt(1);                       
            }
            memset(vmRegion, '\0', USLOSS_MmuPageSize());
            vmStats.new++;
            result = USLOSS_MmuUnmap(TAG, 0);
            result = USLOSS_MmuSetAccess(frame, 1);
        }
        else if(new->diskBlock != UNUSED && new != old)
        {
            diskReadReal (SWAP, new->track, new->sector, USLOSS_MmuPageSize()/USLOSS_DISK_SECTOR_SIZE, &buff);
            result = USLOSS_MmuMap(TAG, 0, frame, USLOSS_MMU_PROT_RW);
            if (result != USLOSS_MMU_OK)
            {
                USLOSS_Console("Pager():\t mmu map failed3 in %d, %d, %d\n", 0, frame, result);
                USLOSS_Halt(1);                       
            }
            memcpy(vmRegion, &buff, USLOSS_MmuPageSize());
            vmStats.pageIns++;
            result = USLOSS_MmuUnmap(TAG, 0);          
        }

        // update frame table
        frameTable[frame].pid = pid;
        frameTable[frame].state = FRAME;
        frameTable[frame].page = newNum;

        // unpdate page table
        proc->pageTable[newNum].state = FRAME;
        proc->pageTable[newNum].frame = frame;
        MboxSend(fault->reply, NULL, 0);
    }
    return 0;
} /* Pager */

/*
 *----------------------------------------------------------------------
 *
 * switchToUser --
 *
 * change to user mode
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
void
switchToUser(){
    int result = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
    if(result != USLOSS_DEV_OK) 
    {
        USLOSS_Console("ERROR: USLOSS_PsrSet failed! Halting....\n");
        USLOSS_Halt(1);
    }
} /* switchToUser */