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
extern int vmStatSem;
extern Process processes[MAXPROC];
extern FTE *frameTable;
extern VmStats vmStats;
extern int sempReal(int);
extern int semvReal(int);

void
p1_fork(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    if(vmInitialized)
    {
        // set number of pages in process's page table and malloc memory
        int pages = processes[pid % MAXPROC].numPages;
		//int frames = processes[pid % MAXPROC].frames;  //djf
        processes[pid % MAXPROC].pageTable = malloc(pages * sizeof(PTE));
		
        //initialize page table for the process
        //for(int i = 0; i < pages; i++)
        for(int i = 0; i < vmStats.pages; i++)  //djf
		{
            processes[pid % MAXPROC].pageTable[i].pageNum = i; //djf
            processes[pid % MAXPROC].pageTable[i].accessed = 0;
            processes[pid % MAXPROC].pageTable[i].state = UNMAPPED;
            processes[pid % MAXPROC].pageTable[i].diskTableIndex = -1;
        }

        for(int i = 0; i < vmStats.frames; i++)  //djf
		{
            processes[pid % MAXPROC].pageTable[i].frame = i; //djf
        }

        processes[pid % MAXPROC].vm = 1;  //djf, this is a vm process

        sempReal(vmStatSem);  //djf
        vmStats.new++;
        semvReal(vmStatSem);	
	}
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);

    int result;

    if(vmInitialized)
    {
        if(processes[old % MAXPROC].vm)
        {
            for(int i = 0; i < vmStats.pages; i++)
            {
                //unmap page from MMU
                if(processes[old % MAXPROC].pageTable[i].state == MAPPED)
                {
                    result = USLOSS_MmuUnmap(0, i);
                    if(result != USLOSS_MMU_OK)
                    {
                        USLOSS_Console("USLOSS_MmuUnmap Error: %d\n", result);
                    }
                    processes[old % MAXPROC].pageTable[i].state = UNMAPPED;
                }
            }
        }

        // if new process is a vm process
        if(processes[new % MAXPROC].vm)
        {
            int frame;
			//USLOSS_Console("p1_switch: new %d\n", processes[new % MAXPROC].vm);
   
            //for every page in the table, see if it should be mapped to a frame
            for(int i = 0; i < vmStats.pages; i++)
            {
                frame = processes[new % MAXPROC].pageTable[i].frame;

                //page should only be mapped if != -1
                //if(frame != -1)  
				if(processes[new % MAXPROC].pageTable[i].state == UNMAPPED) //djf
                {
                    result = USLOSS_MmuMap(0, i, frame, USLOSS_MMU_PROT_RW);
                    //USLOSS_Console("p1_switch MnuMap result %d\n", result);
                    if(result != USLOSS_MMU_OK)
                    {
                        USLOSS_Console("USLOSS_MmuMap error: %d\n", result);
                    }
					processes[new % MAXPROC].pageTable[i].state = MAPPED; //djf
				}
            }
        }

        sempReal(vmStatSem);
        vmStats.switches++;
        semvReal(vmStatSem);

	//USLOSS_Console("p1_switch: switches %d,faults %d\n", vmStats.switches, vmStats.faults);

    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    int result;

    //unmap pages that are mapped in the MMU
    if(vmInitialized && processes[pid % MAXPROC].vm)
    {
        int frame;
        for(int i = 0; i < vmStats.pages; i++)
        {
            frame = processes[pid % MAXPROC].pageTable[i].frame;

            //page is mapped, if frame is not -1 in pageTable
            if(frame != -1)
            {
                result = USLOSS_MmuUnmap(0, i);
                if(result != USLOSS_MMU_OK)
                {
                    USLOSS_Console("USLOSS_MmuUnmap Error: %d\n", result);
                }
            

                // update frame table
                processes[pid % MAXPROC].pageTable[i].frame = -1;
                frameTable[frame].state = UNUSED;
                frameTable[frame].ref = UNREFERENCED;
                frameTable[frame].dirty = CLEAN;

                //update MMU
                result = USLOSS_MmuSetAccess(frame, UNREFERENCED + CLEAN);
                if(result != USLOSS_MMU_OK)
                {
                    USLOSS_Console("USLOS_MmuSetAccess Error: %d\n", result);
                }

                sempReal(vmStatSem); 
                vmStats.freeFrames++;
                semvReal(vmStatSem);
            }
        } 
        free(processes[pid % MAXPROC].pageTable); //free malloced memory
    }
} /* p1_quit */
