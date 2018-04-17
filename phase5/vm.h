/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500  //untouched
#define INCORE 501  //on the disk
#define INFRAME 502 //in the frame table

#define USED 503    //frame that is mapped to a page in memory
#define SWAPDISK 1  //disk to use
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Frame table entry
 */
// Create a struct for your frame table:
typedef struct FTE {
    int pid;         //pid of process using the frame, -1 if non 
    int state;       //whether it is free/in use
    int page;        //the page using this frame

} FTE;

/* Disk table entry */
typedef struct DTE {
    int pid;         //pid of process using this disk block, -1 if none
    int page;        //the page using this disk blcok
    int track;       //what track the page is on
    int sector;      //sector it starts on
} DTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Add more stuff here */
    int pid;
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
    inr pageNum;     //the page the fault occurred on
} FaultMsg;

