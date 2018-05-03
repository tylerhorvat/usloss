#include <phase1.h>
/*
 * All processes use the same tag.
 */
#define TAG 0
#define SWAP 1

/*
 * Different states for a page.
 */
#define UNUSED 500
#define CORE 501
#define FRAME 502
/* You'll probably want more states */

/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
    int  track;
    int  sector;
} PTE;

/*
 * Frame table entry
 */
typedef struct FTE {
    int pid;    //process using frame, -1 if none.
    int state; // UNUSED or INCORE or INFRAME
    int page;  //page with frame
} FTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *address;      // Address that caused the fault.
    int  reply;  // Mailbox to send reply.
    // Add more stuff here.
} FaultMsg;


#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)