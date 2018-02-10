/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <stdlib.h>

//#include "message.h"
#include "handler.c"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
void disableInterrupts();
void enableInterrupts();
void checkForKernelMode(char *);
void initializeQueue (queue*, int);
void enqueue(queue*, void*);
void *dequeue(queue*);
void *peek(queue);
void emptyBox(int);
void emptySlot(int);

/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];
mailSlot MailSlotTable[MAXSLOTS];
mboxProc mboxProcTable[MAXPROC];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...


int numBoxes, numSlots;

int nextMboxID = 0, nextSlotID = 0, nextProc = 0;

// system call vector
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kidPid;
    int status;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    checkForKernelMode("start1");

    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    int i;
    for (i = 0; i < MAXMBOX; i++)
        emptyBox(i);

    for (i = 0; i < MAXSLOTS; i++)
        emptySlot(i);

    numBoxes = 0;
    numSlots = 0;

    // Initialize USLOSS_IntVec and system call handlers,
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;

    // allocate mailboxes for interrupt handlers.  Etc... 
    IOmailboxes[CLOCKBOX] = MboxCreate(0, sizeof(int)); // one clock unit
    IOmailboxes[TERMBOX] = MboxCreate(0, sizeof(int));  // four terminal units
    IOmailboxes[TERMBOX+1] = MboxCreate(0, sizeof(int));
    IOmailboxes[TERMBOX+2] = MboxCreate(0, sizeof(int));
    IOmailboxes[TERMBOX+3] = MboxCreate(0, sizeof(int));
    IOmailboxes[DISKBOX] = MboxCreate(0, sizeof(int));   // two disk units
    IOmailboxes[DISKBOX+1] = MboxCreate(0, sizeof(int));

    for (i = 0; i < MAXSYSCALLS; i++)
        systemCallVec[i] = nullsys;

    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kidPid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kidPid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    disableInterrupts();
    checkForKernelMode("MboxCreate()");

    // check for illegal arguments, or all mailboxes full
    if (numBoxes == MAXMBOX || slots < 0 || slot_size < 0 || slot_size > MAX_MESSAGE)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCreate(): illegal args or max boxes reached, returning -1\n");
        return -1;

    }

    // find next available index
    if (nextMboxID >= MAXMBOX || MailBoxTable[nextMboxID].status == ACTIVE) 
    {
        int i;
        for (i = 0; i < MAXMBOX; i++)
        {
            if (MailBoxTable[i].status == INACTIVE)
            {
                nextMboxID = i;
                break;
            }
        }
    }

    // get next mailbox
    mailbox *box = &MailBoxTable[nextMboxID];

    // initialize mailbox
    box->mboxID = nextMboxID++;
    box->numSlots = slots;
    box->slotSize = slot_size;
    box->status = ACTIVE;
    initializeQueue(&box->slots, SLOTQUEUE);
    initializeQueue(&box->blockedProcSend, PROCQUEUE);
    initializeQueue(&box->blockedProcRec, PROCQUEUE);

    numBoxes++;

    enableInterrupts();
    return box->mboxID;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    return 0;
} /* MboxSend */

 
/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    return 0;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - Determine if there any processes blocked on any of the
             interrupt mailboxes.
   Returns - 1 if one (or more) processes are blocked; 0 otherwise
   Side Effects - none.

   Note: Do nothing with this function until you have successfully completed
   work on the interrupt handlers and their associated mailboxes.
   ------------------------------------------------------------------------ */
int check_io(void)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("check_io(): called\n");
    return 0;
} /* check_io */

void checkForKernelMode(char * name) 
{
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0)
    {
        USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n",
                        name, getpid());
        USLOSS_Halt(1);
    }
}

void disableInterrupts()
{
    int result; 
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        //not in kernel mode
        if(DEBUG2 && debugflag2)
            USLOSS_Console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
        USLOSS_Halt(1);
    }
    else 
    {
        //in kernel mode
       result = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
	   
    }
	
    if (result == USLOSS_DEV_OK)
        return;
    else 
    {
        USLOSS_Console("PSR not valid\n");
        USLOSS_Halt(1);		
    }
    return;
}

void enableInterrupts()
{
    int result; 
    //turn interrupts on iff in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) 
    {
        if(DEBUG2 && debugflag2)
            USLOSS_Console("Kernel Error: Not in kernel mode, may not enable interrupts\n");
        USLOSS_Halt(1);
    }
    else
    {
       result = USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
    }	
    if (result == USLOSS_DEV_OK)
        return;
    else 
    {
        USLOSS_Console("PSR not valid\n");
        USLOSS_Halt(1);	
    }
    return;
}

void initializeQueue(queue *q, int type) 
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    q->type = type;
}


void enqueue(queue *q, void *p)
{
    if (q->head == NULL && q->tail == NULL)
    {
       q->head = q->tail = p; 
    }
    else
    {
        if (q->type == SLOTQUEUE)
            ((slotPtr)(q->tail))->nextSlotPtr = p;
        else if (q->type == PROCQUEUE)
            ((mboxProcPtr)(q->tail))->nextMboxProc = p;
        q->tail = p;
    }
    q->size++;
}

void emptyBox(int i)
{
    MailBoxTable[i].mboxID = -1;
    MailBoxTable[i].status = INACTIVE;
    MailBoxTable[i].numSlots = -1;
    MailBoxTable[i].slotSize = -1;
}

void emptySlot(int i)
{
    MailSlotTable[i].mboxID = -1;
    MailSlotTable[i].status = EMPTY;
    MailSlotTable[i].slotId = -1;
}


























































