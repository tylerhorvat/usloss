#include "usloss.h"
#include <vm.h>
#include <stdlib.h>
#include <usyscall.h>
#include <phase5.h>
#define DEGUG 0

extern int debugflag; 
extern VmStats  vmStats;
extern void* vmRegion;
extern FTE* frameTable;
extern int vmInitialized;
extern Process processes[MAXPROC];

/*
 *----------------------------------------------------------------------
 *
 * p1_fork --
 *
 * fork a process
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
p1_fork(int pid)
{
    //if(vmInitialized)
    //{
        processes[pid%MAXPROC].pageTable = (PTE*)malloc(processes[pid%MAXPROC].numPages * sizeof(PTE));
        PTE* page = processes[pid%MAXPROC].pageTable;

        // initialize the page table
        for(int i =0; i <  processes[pid%MAXPROC].numPages; i++)
        {
            page->state = UNUSED;
            page->frame = -1;
            page->diskBlock = UNUSED;
            page->track = -1;
            page->sector = 1;
            page++;
	}
    //}
} /* p1_fork */

/*
 *----------------------------------------------------------------------
 *
 * p1_switch --
 *
 * called by dispatcher, load page table into MMU
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      called USLOSS_MmuGetMap and USLOSS_MmuUnmap.
 *
 *----------------------------------------------------------------------
 */
void
p1_switch(int old, int new)
{
    if (vmRegion == NULL)
        return;

    int result;

    vmStats.switches++;
    
    // map old process's page
    if (old > 0)
    {
        Process* proc = &processes[old%MAXPROC];
        int frame = 0, protPtr = 0;
        for(int page = 0; page < proc->numPages && (&proc->pageTable[page]) != NULL; page++)
        {
            int mapping = USLOSS_MmuGetMap(TAG, page, &frame, &protPtr);
            if (mapping == USLOSS_MMU_OK)
            {
                result = USLOSS_MmuUnmap(TAG, page);
                if (result != USLOSS_MMU_OK)
                {
                    USLOSS_Console("p1_switch():\t mmu unmap failed. error %d\n", result);
                    USLOSS_Halt(1);                       
                }
            }

        }
    }

    // map new process's page
    if (new > 0){
        Process* proc = &processes[new%MAXPROC];
        for(int page = 0; page < proc->numPages && proc->pageTable != NULL; page++)
        {
            if (proc->pageTable[page].state == FRAME)
            {
                result = USLOSS_MmuMap(TAG, page, proc->pageTable[page].frame, USLOSS_MMU_PROT_RW);
                if (result != USLOSS_MMU_OK)
                {
                    USLOSS_Console("p1_switch():\t mmu map failed with error %d\n", result);
                    USLOSS_Halt(1);                       
                }  
            }            
        }
    }
} /* p1_switch */

/*
 *----------------------------------------------------------------------
 *
 * p1_quit --
 *
 * quit a process
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      called USLOSS_MmuGetMap and USLOSS_MmuUnmap.
 *
 *----------------------------------------------------------------------
 */
void
p1_quit(int pid)
{

    int frame = 0, protPtr = 0, result;
    
    Process* proc = &processes[pid%MAXPROC];
    for(int page = 0; page < proc->numPages && proc->pageTable != NULL; page++)
    {
        result = USLOSS_MmuGetMap(TAG, page, &frame, &protPtr);
        if (result == USLOSS_MMU_OK)
        {
            result = USLOSS_MmuSetAccess(frame,0);
            result = USLOSS_MmuUnmap(TAG, page);
            if (result != USLOSS_MMU_OK)
            {
                USLOSS_Console("p1_quit():\t mmu upmap failed with error %d\n", result);
                USLOSS_Halt(1);                
            }

            // clear the page
            proc->pageTable[page].state = UNUSED;
            proc->pageTable[page].frame = -1;
            proc->pageTable[page].diskBlock = -1;
            
            // clear the frame
            frameTable[frame].pid = -1;
            frameTable[frame].state = UNUSED;
            frameTable[frame].page = -1;
            vmStats.freeFrames++;
        }
    }

    if (proc->pageTable != NULL)
        free(proc->pageTable);
} /* p1_quit */