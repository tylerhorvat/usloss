/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */

#include <usloss.h>
#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h> 
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <providedPrototypes.h>

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


 /*----------------------------------------------------------------------
 * start4
 *
 * Initializes the VM system call handlers. 
 *
 * Results: MMU return status
 *
 * Side effects: The MMU is initialized.
 *----------------------------------------------------------------------*/
int start4(char *arg){
    int pid;     // process ID
    int result;  // value returned from functions
    int status;  // value returned from wait

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

    /* fork Start5 */
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }

    /* Wait for start5 to terminate */
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }

    Terminate(0);
    return 0;
} /* start4 */

 /*----------------------------------------------------------------------
 * VmInit
 *
 * Stub for the VmInit system call.
 *
 * Results: None
 *
 * Side effects: VM system is initialized.
 *----------------------------------------------------------------------*/
static void vmInit(USLOSS_Sysargs *args) {
    void *result;  // value returned from vmInitReal

    CheckMode();

    /* get values from systemArgs struct */
    int mappings = ((int) (long) args->arg1);
    int pages = ((int) (long) args->arg2);
    int frames = ((int) (long) args->arg3);
    int pagers = ((int) (long) args->arg4);

    result = vmInitReal(mappings, pages, frames, pagers);

    /* parse values to return to user */
    if (((int) (long) result) < 0) {
        args->arg4 = result;
    } else {
        args->arg4 = ((void *) (long) 0);
    }

    args->arg1 = result;
} /* vmInit */

 /*----------------------------------------------------------------------
 * vmDestroy
 *
 * Stub for the VmDestroy system call.
 *
 * Results: None
 *
 * Side effects: VM system is cleaned up.
 *----------------------------------------------------------------------*/
static void vmDestroy(USLOSS_Sysargs *args){
   CheckMode();
   vmDestroyReal();
} /* vmDestroy */

 /*----------------------------------------------------------------------
 * vmInitReal
 *
 * Called by vmInit. Initializes the VM system by configuring the MMU and 
 * setting up the page tables.
 *
 * Results: Address of the VM region.
 *
 * Side effects: The MMU is initialized.
 *----------------------------------------------------------------------*/
void *vmInitReal(int mappings, int pages, int frames, int pagers){
    int status;                // value returned by MMU functions
    int numPagesInVmRegion;    // number of pages in vm region

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

    vmInitialized = 1; // mark system as virtual memory initialized

    /* initialize MMU */
    status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
    if (status != USLOSS_MMU_OK) {
        USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
        abort();
    }

    

    /* Zero out, then initialize, the vmStats structure */
    int sectorSize, sectorsInTrack, numTracksOnDisk;
    diskSizeReal(1, &sectorSize, &sectorsInTrack, &numTracksOnDisk);

    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.diskBlocks = numTracksOnDisk * 2;
    vmStats.freeFrames = frames;
    vmStats.freeDiskBlocks = numTracksOnDisk * 2;
    vmStats.faults = 0;
    vmStats.switches = 0;
    vmStats.new = 0;
    vmStats.pageIns = 0;
    vmStats.pageOuts = 0;
    vmStats.replaced = 0;

    vmStatSem = semcreateReal(1);  // mutal exclusion for vmStats

    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler; // handler for MMU fault

    /* initialize disk table */
    diskTable = malloc(sectorsInTrack * numTracksOnDisk * sizeof(DTE));
    diskTableSize = sectorsInTrack * numTracksOnDisk;

    int trackNum = 0;
    for (int i = 0; i < diskTableSize; i += 2) {
        diskTable[i].track = trackNum;
        diskTable[i + 1].track = trackNum++;
        diskTable[i].sector = 0;
        diskTable[i + 1].sector = sectorsInTrack / 2;
        diskTable[i].state = UNUSED;
        diskTable[i + 1].state = UNUSED;
    }


    /* Initialize page tables, and fault mbox */
    for (int i = 0; i < MAXPROC; i++) {
       processes[i].pid = -1;
       processes[i].vm = 0;
       processes[i].numPages = pages;

       faults[i].pid = -1;
       faults[i].replyMbox = MboxCreate(1, 0);
       faults[i].addr = NULL;
    }

    /* Initialize frame tables */
    frameTable = malloc(frames * sizeof(FTE));
    for (int i = 0; i < frames; i++) {
        frameTable[i].state = UNUSED;
        frameTable[i].ref = UNREFERENCED;
        frameTable[i].dirty = CLEAN;
        frameTable[i].pid = -1;
        frameTable[i].page = NULL;
    }   

    frameSem = semcreateReal(1);   // mutal exclusion for frameTable

    /* initialize clock hand for clock algorithm */
    clockHand = 0;
    clockSem = semcreateReal(1);   // mutal exclusion for clockHand

    /* Fork the pagers */
    numPagers = pagers;    // global for number of pagers in system
    pagerPids = malloc(pagers * sizeof(int));
    char buf[100];
    pagerMbox = MboxCreate(pagers, sizeof(int));
    for (int i = 0; i < numPagers; i++){
        pagerPids[i] = fork1("pagerProcess", Pager, buf, USLOSS_MIN_STACK, 
              PAGER_PRIORITY);
    }

    vmRegion = USLOSS_MmuRegion(&numPagesInVmRegion);
    return vmRegion;
} /* vmInitReal */


 /*----------------------------------------------------------------------
 * PrintStats
 *
 * Print out VM statistics.
 *
 * Results: None
 *
 * Side effects: Stuff is printed to the USLOSS_Console.
 *----------------------------------------------------------------------*/
void PrintStats(void){
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

 /*----------------------------------------------------------------------
 * vmDestroyReal
 *
 * Called by vmDestroy. Frees all of the global data structures
 *
 * Results: None
 *
 * Side effects: The MMU is turned off.
 *----------------------------------------------------------------------*/
void vmDestroyReal(void){

    vmInitialized = 0;    

    CheckMode();
    USLOSS_MmuDone();

    /* kill the pagers */
    int terminateMsg = -1;
    for (int i = 0; i < numPagers; i++) {
       MboxSend(pagerMbox, &terminateMsg, sizeof(int));
    }

    /* zap to wait or pagers to terminate */
    for (int i = 0; i < numPagers; i++) {
       zap(pagerPids[i]);
    }

    /* Print vm statistics */
    PrintStats();

    //TODO: free memory 
    //free framesTable and pager PID table

    
} /* vmDestroyReal */

 /*----------------------------------------------------------------------
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 * 
 * Parameters: type: int - USLOSS_MMU_INT
 *             arg:  void* - Offset within VM region
 *
 * Results: None
 *
 * Side effects: procTable and faultMsg changed. 
 *               The current process is blocked until the fault is handled.
 *----------------------------------------------------------------------
 */
static void FaultHandler(int  type, void *arg){
    int cause; // reason for the fault

    int offset = (int) (long) arg;          // offset within vm region of fault
    void *vmPlusOffset = vmRegion + offset; // address of fault

    assert(type == USLOSS_MMU_INT);
    cause = USLOSS_MmuGetCause();
    assert(cause == USLOSS_MMU_FAULT);

    sempReal(vmStatSem);
    vmStats.faults++;
    semvReal(vmStatSem);

    int pid;             // pid of process that caused the fault
    getPID_real(&pid);

    processes[pid % MAXPROC].pid = pid;
    processes[pid % MAXPROC].vm = 1;    // process is uses vm region

    /* fill in fault message */
    faults[pid % MAXPROC].pid = pid;
    faults[pid % MAXPROC].addr = vmPlusOffset;

    /* send pid to pager to process fault message */
    MboxSend(pagerMbox, &pid, sizeof(int));

    /* wait for pager to process fault message */
    MboxReceive(faults[pid % MAXPROC].replyMbox, NULL, 0);
} /* FaultHandler */

/*----------------------------------------------------------------------
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results: None
 *
 * Side effects: pageTable of pid, frameTable, clockHand, and MMU access
 *               bits changed.
 *-----------------------------------------------------------------------*/
static int Pager(char *buf) {
    int pid;

    while (1) {

        /* Wait for fault to occur to be sent by faulting process */
        MboxReceive(pagerMbox, &pid, sizeof(int));

        if (pid == -1 ) {
            break;
        }

        /* update frameTable to match MMU */
        int accessBit;

        for (int i = 0; i < vmStats.frames; i++) {
            int result = USLOSS_MmuGetAccess(i, &accessBit);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager_accessBit: USLOSS_MmuGetAccess Error: "
                        "%d\n", result);
            }
            frameTable[i].ref = accessBit & REFERENCED;
            frameTable[i].dirty = accessBit & DIRTY;
        }

        /* Look for free frame to store the page */
        int freeFrame = -1;
        int page = -1;

        for (int i = 0; i < vmStats.frames; i++) {
            if (frameTable[i].state == UNUSED){
                freeFrame = i;

                sempReal(vmStatSem);
                vmStats.freeFrames--;
                semvReal(vmStatSem);

                break;
            }
        }

        /*
         * If there are no free frames available use clock algorithm to
         * replace a page (perhaps write to disk) 
         */
        if (freeFrame == -1) {

            /* 
             * Find the first frame that is unreferenced. For every frame that 
             * is found that is referenced, change it to unreferenced.
             */
            for (int i = 0; i < vmStats.frames * 2; i++) {
                if (frameTable[clockHand].ref == UNREFERENCED) {
                    freeFrame = clockHand;

                	clockHand++;
                	if (clockHand == vmStats.frames) {
                	    clockHand = 0;
                	}

                    break;
                } else {
                    frameTable[clockHand].ref = UNREFERENCED;

                    USLOSS_MmuSetAccess(clockHand, 
                            frameTable[clockHand].dirty);

                    clockHand++;
                    if (clockHand == vmStats.frames) {
                        clockHand = 0;
                    }
                }
            }

            /* If the frame is dirty, write the frame to disk */
            if (frameTable[freeFrame].dirty == DIRTY) {

                /* page to be written */
                page = frameTable[freeFrame].page->pageNum;

                /* memory to write the page to */
                void * addr = vmRegion + (page * USLOSS_MmuPageSize());

                /* used by pager to store the page before writing to disk */
                char buffer[USLOSS_MmuPageSize()];

                int diskLocation;    // location on disk to store the page
                int firstUnused = 1; // first available disk block

                /* find location on swap disk for page */
                for (int i = 0; i < diskTableSize; i++) {
                    if (diskTable[i].pid == pid && diskTable[i].page == page) {
                        diskLocation = i;
                        break;
                    } else if (firstUnused && diskTable[i].state == UNUSED) {
                        firstUnused = 0;
                        diskLocation = i;

                        sempReal(vmStatSem);
                        vmStats.freeDiskBlocks--;
                        semvReal(vmStatSem);
                    }
                }

                /* save frame to buffer. need mapping to MMU */
                int result = USLOSS_MmuMap(0, page, freeFrame, 
                        USLOSS_MMU_PROT_RW);
                if (result != USLOSS_MMU_OK) {
                    USLOSS_Console("Pager(): USLOSS_MmuMap Error: %d\n"
                            , result);
                }

                memcpy(buffer, addr, USLOSS_MmuPageSize());

                result = USLOSS_MmuUnmap(0, page);
                if (result != USLOSS_MMU_OK) {
                    USLOSS_Console("Pager(): USLOSS_MmuUnmap Error: "
                            "%d\n", result);
                }

                /* write page from buffer to disk */
                diskWriteReal(1, diskTable[diskLocation].track, 
                        diskTable[diskLocation].sector, USLOSS_MmuPageSize() / 
                        USLOSS_DISK_SECTOR_SIZE, buffer);

                /* set frame table and MMU access bits */
                frameTable[freeFrame].ref = UNREFERENCED;
                frameTable[freeFrame].dirty = CLEAN;
                USLOSS_MmuSetAccess(freeFrame, 
                        frameTable[freeFrame].dirty);

                /* update disk table and page table state */
                diskTable[diskLocation].state = USED;
                diskTable[diskLocation].pid = frameTable[freeFrame].pid;
                diskTable[diskLocation].page = page;
                frameTable[freeFrame].page->diskTableIndex = diskLocation;
                frameTable[freeFrame].page->frame = -1;

                sempReal(vmStatSem);
                vmStats.pageOuts++;
                semvReal(vmStatSem);
            }
        }


        /* find page number of fault*/
        page = (int)(long) ( (char *) faults[pid % MAXPROC].addr - 
                (char *) vmRegion) / USLOSS_MmuPageSize();
        
        /* update page table */
        processes[pid].pageTable[page].frame = freeFrame;
        processes[pid].pageTable[page].pageNum = page;
        
        /* update frame table */
        if (frameTable[freeFrame].state == USED) {
            frameTable[freeFrame].page->frame = -1;
        } else {
            frameTable[freeFrame].state = USED; 
        }
        frameTable[freeFrame].page = &processes[pid].pageTable[page];
        frameTable[freeFrame].pid = pid;

        /* Load page into frame from disk */
        if (processes[pid % MAXPROC].pageTable[page].diskTableIndex != -1) {
            int diskIndex = processes[pid].pageTable[page].diskTableIndex;

            /* read page from disk */
            char buffer[USLOSS_MmuPageSize()]; // buffer to read page to

            diskReadReal(1, diskTable[diskIndex].track,
                         diskTable[diskIndex].sector, USLOSS_MmuPageSize() /
                         USLOSS_DISK_SECTOR_SIZE, buffer);

            /* save buffer to frame */
            int result = USLOSS_MmuMap(0, page, freeFrame,
                USLOSS_MMU_PROT_RW);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager(): USLOSS_MmuMap Error: %d\n"
                    , result);
            }

            memcpy(faults[pid % MAXPROC].addr, buffer, USLOSS_MmuPageSize());
            
            result = USLOSS_MmuUnmap(0, page);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager(): USLOSS_MmuUnmap Error: "
                    "%d\n", result);
            }
            
            /* set access bits in MMU and frame table */
            result = USLOSS_MmuSetAccess(freeFrame, UNREFERENCED + CLEAN);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager(): USLOSS_MmuSetAccess Error: %d\n",
                        result);
            }

            frameTable[freeFrame].ref = UNREFERENCED;
            frameTable[freeFrame].dirty = CLEAN;

            /* set frame to page in page table */
            processes[pid % MAXPROC].pageTable[page].frame = freeFrame;

            sempReal(vmStatSem);
            vmStats.pageIns++;
            semvReal(vmStatSem);

        /* first time page is entering vm system, initialize frame */
        } else if (processes[pid].pageTable[page].accessed == 0) {

            /* create mapping for pager to access frame */
            int result = USLOSS_MmuMap(0, page, freeFrame, USLOSS_MMU_PROT_RW);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager(): USLOSS_MmuMap Error: %d\n", result);
            }

            memset(faults[pid % MAXPROC].addr, 0, USLOSS_MmuPageSize());

            result = USLOSS_MmuUnmap(0, page);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager(): USLOSS_MmuUnmap Error: %d\n", result);
            }

            /* update access bits of MMU and frame table */
            frameTable[freeFrame].ref = UNREFERENCED; 
            frameTable[freeFrame].dirty = CLEAN; 

            result = USLOSS_MmuSetAccess(freeFrame, UNREFERENCED + CLEAN);
            if (result != USLOSS_MMU_OK) {
                USLOSS_Console("Pager(): USLOSS_MmuSetAccess Error: %d\n", 
                        result);
            }

            processes[pid].pageTable[page].accessed = 1;

            sempReal(vmStatSem);
            vmStats.new++;
            semvReal(vmStatSem);
        }

        /* Unblock waiting (faulting) process */
        MboxSend(faults[pid % MAXPROC].replyMbox, NULL, 0);
    }
    return 0;
} /* Pager */

int getPID5() {
    int pid;
    getPID_real(&pid);
    return pid;
}

void debugPageTable(int pid){
    USLOSS_Console("-Process %d Page Table-\n", pid);
    for(int i=0; i<vmStats.pages; i++){
	USLOSS_Console("Page %d -> Frame %d - DiskIndex %d \n",i, processes[pid].pageTable[i].frame, processes[pid].pageTable[i].diskTableIndex);
    }
}

void debugFrameTable(int pid){
    USLOSS_Console("-Current Frame Table-\n", pid);
    for(int i=0; i<vmStats.frames; i++){
	if(frameTable[i].page != NULL)
        USLOSS_Console("Frame %d -> Process %d - Page %d - Ref %d - Dirty %d\n"
		,i, frameTable[i].pid,frameTable[i].page->pageNum, frameTable[i].ref, frameTable[i].dirty);
	else
	USLOSS_Console("Frame unused\n");
    }
}
