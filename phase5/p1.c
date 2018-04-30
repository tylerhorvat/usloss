#include "usloss.h"
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <vm.h>

#define DEBUG 0 

extern int debugflag;
extern int vmInitialized;
extern Process processes[MAXPROC];
extern FTE *frameTable;
extern DTE *diskTable;
extern int vmStatSem;
extern VmStats vmStats;
extern int  sempReal(int semaphore);
extern int  semvReal(int semaphore);

/*----------------------------------------------------------------------
 * p1_fork
 *
 * Creates a page table for a process, if the virtual memory system
 * has been initialized.
 *
 * Results: None
 *
 * Side effects: pageTable malloced and initialized for process
 *----------------------------------------------------------------------*/
void p1_fork(int pid) {
    if (DEBUG)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    if (vmInitialized) {
        /* set number of pages in process's page table and malloc memory */
        int pages = processes[pid % MAXPROC].numPages;
        processes[pid % MAXPROC].pageTable = malloc(pages * sizeof(PTE));

        /* initialize page table for the process */
        for (int page = 0; page < pages; page++) {
            processes[pid % MAXPROC].pageTable[page].frame = -1;
            processes[pid % MAXPROC].pageTable[page].accessed = 0;
            processes[pid % MAXPROC].pageTable[page].state = UNMAPPED;
            processes[pid % MAXPROC].pageTable[page].diskTableIndex = -1;
        }
    }
} /* p1_fork */

/*----------------------------------------------------------------------
 * p1_switch
 *
 * Unmaps and Maps pages to frames in the MMU for processes that are context
 * switching. A process must be set as a virtual memory process by the fault
 * handler in phase5.c to unmap and map.
 *
 * Results: None
 *
 * Side effects: mapping are added or removed from the MMU
 *----------------------------------------------------------------------*/
void p1_switch(int old, int new) {
    if (DEBUG) {
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
        USLOSS_Console("p1_switch(): new = %d, vm = %d\n", new, 
                processes[new].vm);
    }

    int result;

    /* only perform mappings and unmappings if vm is initialized */
    if (vmInitialized) {

        /* if old is a vm process */
        if (processes[old % MAXPROC].vm) {
            for(int i = 0; i < vmStats.pages; i++){

                /* Unmap page from MMU */
                if (processes[old % MAXPROC].pageTable[i].state == MAPPED) {
                    result = USLOSS_MmuUnmap(0, i);
                    if (result != USLOSS_MMU_OK) {
                        USLOSS_Console("p1_switch(old): "
                                "USLOSS_MmuUnmap Error: %d\n", result);
                    }
                    processes[old % MAXPROC].pageTable[i].state = UNMAPPED;
                }
            }
        }

        /* if new is a vm process */
        if (processes[new % MAXPROC].vm) {
            int frame;

            /* 
             * for every page in the pageTable, see if it should be mapped
             * to a frame.
             */
            for(int i = 0; i < vmStats.pages; i++){
                frame = processes[new % MAXPROC].pageTable[i].frame;

                /* page should only be mapped to a frame if frame is not -1 */
                if (frame != -1) {
                    result = USLOSS_MmuMap(0, i, frame, USLOSS_MMU_PROT_RW);
                    if(result != USLOSS_MMU_OK){
                        USLOSS_Console("p1_switch(new): USLOSS_MmuMap error: "
                                "%d\n", result);
                    }
                    processes[new % MAXPROC].pageTable[i].state = MAPPED;
                }
            }

        }

        sempReal(vmStatSem);
        vmStats.switches++;
        semvReal(vmStatSem);
    }
} /* p1_switch */

/*----------------------------------------------------------------------
 * p1_quit
 *
 * Removes mappings to the MMU for processes that are quiting.
 *
 * Results: None
 *
 * Side effects: mappings are removed from the MMU
 *----------------------------------------------------------------------*/
void p1_quit(int pid) {
   
    int result; 

    if (DEBUG)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    // Unmap pages that are maped in the MMU 
    if (vmInitialized && processes[pid % MAXPROC].vm) {
        int frame;
        if (DEBUG)
                USLOSS_Console("p1_quit() 1\n");

        for(int page = 0; page < vmStats.pages; page++) {
           //USLOSS_Console("diskTableIndex: %d\n", processes[pid % MAXPROC].pageTable[page].diskTableIndex);
            
            if (DEBUG)
                USLOSS_Console("p1_quit() 2\n");


            int diskLocation = processes[pid % MAXPROC].pageTable[page].diskTableIndex;

            //USLOSS_Console("disklocation = %d\n", diskLocation);

            if (DEBUG)
                USLOSS_Console("p1_quit() 3\n");
            
            if(diskLocation != -1)
            {
                diskTable[diskLocation].state = UNUSED;

            if (DEBUG)
                USLOSS_Console("p1_quit() 4\n");

                vmStats.freeDiskBlocks++;
            }
            if (DEBUG)
                USLOSS_Console("p1_quit() 5\n");

            frame = processes[pid % MAXPROC].pageTable[page].frame;

            if (DEBUG)
                USLOSS_Console("p1_quit() 6\n");


            
            //page is mapped, if frame is not -1 in pageTable 
            if (frame != -1) {
                result = USLOSS_MmuUnmap(0, page);
                if (result != USLOSS_MMU_OK) {
                    USLOSS_Console("p1_quit(): "
                            "USLOSS_MmuUnmap Error: %d\n", result);
                }
                
                //update frame table 
                processes[pid % MAXPROC].pageTable[page].frame = -1;
                frameTable[frame].state = UNUSED;
                frameTable[frame].ref = UNREFERENCED;
                frameTable[frame].dirty = CLEAN;

                //update MMU
                result = USLOSS_MmuSetAccess(frame, UNREFERENCED + CLEAN);
                if (result != USLOSS_MMU_OK) {
                    USLOSS_Console("p1_quit: USLOSS_MmuSetAccess "
                        "Error: %d\n", result);
                }  

                sempReal(vmStatSem);
                vmStats.freeFrames++;
                semvReal(vmStatSem);
            }
        }
        free(processes[pid % MAXPROC].pageTable); // free malloced memory
    }
} /* p1_quit */
