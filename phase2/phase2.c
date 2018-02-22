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
#include <string.h>

//#include "message.h"
#include "handler.c"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
void disableInterrupts();
void enableInterrupts();
void checkForKernelMode(char *);
void initializeQueue (queue*, int);
void enqueue(queue*, void*);
void *dequeue(queue *);
void *peek(queue);
void clearBox(int);
void clearSlot(int);
int createSlot(void *, int);
int sentProc(mboxProcPtr, void *, int);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 1;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];
mailSlot MailSlotTable[MAXSLOTS];
mboxProc mboxProcTable[MAXPROC];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...


int numBoxes, numSlots;

int nextMboxID = 0, nextSlotID = 0, nextProc = 0;

int IOmailboxes[7]; // mboxIDs for the IO devices
int IOblocked; // number of processes blocked on IO mailboxes

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
        clearBox(i);

    for (i = 0; i < MAXSLOTS; i++)
        clearSlot(i);

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

    if (DEBUG2 && debugflag2)
        USLOSS_Console("MboxCreate(): created mailbox with id = %d, numSlots = %d, slot_size = %d, numBoxes = %d\n", box->mboxID, box->numSlots, box->slotSize, numBoxes);

    enableInterrupts();
    return box->mboxID;
} /* MboxCreate */


/*mbox condsend*/
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    disableInterrupts();
    checkForKernelMode("MboxCondSend()");

    if (DEBUG2 && debugflag2)
        USLOSS_Console("MboxCondSend(): called with mbox_id: %d, msg_ptr: %d\n", mbox_id, msg_ptr, msg_size);

    // check for invalid mbox_id
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondSend(): called with invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // get mailbox
    mailbox *box = &MailBoxTable[mbox_id];

    // check for invalid arguments
    if (box->status == INACTIVE || msg_size < 0 || msg_size > box->slotSize)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondSend(): called with an invalid argument %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // handle blocked receiver
    if (box->blockedProcRec.size > 0 && (box->slots.size < box->numSlots || box->numSlots == 0)) 
    {
        mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcRec);
        int res = sentProc(proc, msg_ptr, msg_size);
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondSend(): unblocking process %d that was blocked on a receive\n", proc->pid);
        unblockProc(proc->pid);
        enableInterrupts();
        if (res < 0)
            return -1;
        return 0;
    }

    if (box->slots.size == box->numSlots)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondSend: conditional send failed, returning -2\n");
        enableInterrupts();
        return -2;
    }

    if (numSlots == MAXSLOTS)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("No slots available for conditional send to box %d, returning -2\n", mbox_id);
        return -2;
    }

    int slotid = createSlot(msg_ptr, msg_size);
    slotPtr slot = &MailSlotTable[slotid];
    enqueue(&box->slots, slot);

    enableInterrupts();
    return 0;
}
/*mbox cond send*/


/*mbox release*/
int MboxRelease(int mbox_id)
{
    disableInterrupts();
    checkForKernelMode("MboxRelease()");

    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxRelease(): called with invalid mailboxID: %d, returning -1\n", mbox_id);
        return -1;
    }

    // get mailbox
    mailbox *box = &MailBoxTable[mbox_id];

    // check if mailbox is valid
    if (box == NULL || box->status == INACTIVE)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxRelease(): mailbox %d inactive, returning -1\n", mbox_id);
        return -1;
    }

    // empty mailbox slots
    while (box->slots.size > 0)
    {
        slotPtr slot = (slotPtr)dequeue(&box->slots);
        clearSlot(slot->slotId);
    }

    // release the mailbox
    clearBox(mbox_id);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("MboxRelease(): released mailbox %d\n", mbox_id);

    // unlock any processes blocked on a send
    while (box->blockedProcSend.size > 0)
    {
        mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
        unblockProc(proc->pid);
        disableInterrupts();
    }

    // unblock processes blocked on a receive
    while (box->blockedProcRec.size > 0)
    {
        mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcRec);
        unblockProc(proc->pid);
        disableInterrupts();
    }

    enableInterrupts();
    return 0;
}
/*end mbox release*/

void clearBox(int i)
{
    MailBoxTable[i].mboxID = -1;
    MailBoxTable[i].status = INACTIVE;
    MailBoxTable[i].numSlots = -1;
    MailBoxTable[i].slotSize = -1;
    numBoxes--;
}

void clearSlot(int i)
{
    MailSlotTable[i].mboxID = -1;
    MailSlotTable[i].status = EMPTY;
    MailSlotTable[i].slotId = -1;
    numSlots--;
}


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
   //USLOSS_Console("mboxSend() 1\n");
   disableInterrupts();
   checkForKernelMode("MboxSend");
   //USLOSS_Console("mboxsend() 2\n");

   if (DEBUG2 && debugflag2)
       USLOSS_Console("MboxSend(): called with mbox_id: %d, msg_ptr: %d, msg_size: %d\n", mbox_id, msg_ptr, msg_size);
    //USLOSS_Console("mboxsend() 3\n");
    // check for valid mbox_id
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(); called with invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // get mailbox
    mailbox *box = &MailBoxTable[mbox_id];

    // check for valid arguments
    if (box->status == INACTIVE || msg_size < 0 || msg_size > box->slotSize)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): called with an invalid argument, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }


    // handle blocked receiver
    if (box->blockedProcRec.size > 0 && (box->slots.size < box->numSlots || box->numSlots == 0))
    {
        mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcRec);

        // give message to the receiver
        int result = sentProc(proc, msg_ptr, msg_size);    

        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): unblocking process %d that was blocked on receive\n", proc->pid);
        unblockProc(proc->pid);
        enableInterrupts();  //renableInterrupts
        if (result < 0)
            return -1;
        return 0;
    }

    if (box->slots.size == box->numSlots)
    {
        // initialize proc details
        mboxProc mproc;
        mproc.nextMboxProc = NULL;
        mproc.pid = getpid();
        mproc.msgPtr = msg_ptr;
        mproc.msgSize = msg_size;

        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): all slots are full, blocking pid %d...\n", mproc.pid);

        // add to blocked proccesses at this mailbox
        enqueue(&box->blockedProcSend, &mproc);
   
        blockMe(FULL_BOX);   // block
        disableInterrupts();

        // return -3 if process is zapped
        if (isZapped() || box->status == INACTIVE)
        {
             if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxSend(): process %d was zapped while blocked on a send, returning -3\n", mproc.pid);
             enableInterrupts();
            return -3;
        }
        
        enableInterrupts();
        return 0;
    }

    // mail slot overflow halts usloss
    if (numSlots == MAXSLOTS)
    {
        USLOSS_Console("Mail slot table overflow. Halting...\n");
        USLOSS_Halt(1);
    }

    // create new slot and add message to it
    int slotid = createSlot(msg_ptr, msg_size);
    slotPtr slot = &MailSlotTable[slotid];
    enqueue(&box->slots, slot);

    enableInterrupts();
    

    return 0;
} /* MboxSend */


int createSlot(void *msg_ptr, int msg_size)
{
    disableInterrupts();
    checkForKernelMode("createSlot()");

    // get index
    if (nextSlotID >= MAXSLOTS || MailSlotTable[nextSlotID].status == USED)
    {
        int i;
        for (i = 0; i < MAXSLOTS; i++)
        {
            if (MailSlotTable[i].status == EMPTY) 
            {
                nextSlotID = i;
                break;
            }
        }
    }

    slotPtr slot = &MailSlotTable[nextSlotID];
    slot->slotId = nextSlotID++;
    slot->status = USED;
    slot->msgSize = msg_size;
    numSlots++;

    // copy the message
    memcpy(slot->message, msg_ptr, msg_size);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("createSlot(): created new slot for message size %d, slotId: %d, numSlots: %d\n", msg_size, slot->slotId, numSlots);

    return slot->slotId;

}

int sentProc(mboxProcPtr proc, void *msgPtr, int msgSize)
{
    if (proc == NULL || proc->msgPtr == NULL || proc->msgSize < msgSize)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("sentProc(): invalid args, returning -1\n");
        proc->msgSize = -1;
        return -1;
    }
	
    // copy the message
    memcpy(proc->msgPtr, msgPtr, msgSize);
    proc->msgSize = msgSize;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("sentProc(): gave message size %d to process %d\n", msgSize, proc->pid);

    return 0;
}


/*mbox cond receive*/
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    disableInterrupts();
    checkForKernelMode("MboxCondReceive()");
    slotPtr slot;

    // check for valid mbox_id
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondReceive(): called with invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    mailbox *box = &MailBoxTable[mbox_id];
    int size;

    // check that the box is valid
    if (box->status == INACTIVE)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondReceive(): invalid box id: %d, returning -1\n", mbox_id);        enableInterrupts();
        return -1;
    }

    // hanlde 0 slot mailbox
    if (box->numSlots == 0)
    {
        mboxProc mproc;
        mproc.nextMboxProc = NULL;
        mproc.pid = getpid();
        mproc.msgPtr = msg_ptr;
        mproc.msgSize = msg_size;

        // if a process has sent, unblock and get the message
        if (box->blockedProcSend.size > 0)
        {
            mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
            sentProc(&mproc, proc->msgPtr, proc->msgSize);
            if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxCondReceive(): unblocking process %d that was blocked on send to 0 slot mailbox\n", proc->pid);
            unblockProc(proc->pid);
        }

        enableInterrupts();
        return mproc.msgSize;
    }

    // block if there are no messages available
    if (box->slots.size == 0)
    {
        // initialize proc details
        mboxProc mproc;
        mproc.nextMboxProc = NULL;
        mproc.pid = getpid();
        mproc.msgPtr = msg_ptr;
        mproc.msgSize = msg_size;
        mproc.messRec = NULL;

        // handle 0 slot mailbox
        if (box->numSlots== 0 && box->blockedProcSend.size > 0)
        {
            mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
            sentProc(&mproc, proc->msgPtr, proc->msgSize);
            if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxCondReceive(): unblocking process %d that was blocked on send to 0 slot mailbox\n", proc->pid);
            unblockProc(proc->pid);
            enableInterrupts();
            return mproc.msgSize;
        }

        // dont block on a conditional receive
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondReceive(): conditional receive failed, returning -2\n");
        enableInterrupts();
        return -2;
    }
    else
        slot = dequeue(&box->slots); // get the mailslot

    // check if they dont have enough room for the message
    if (slot == NULL || slot->status == EMPTY || msg_size < slot->msgSize)
    {
        if (DEBUG2 && debugflag2 && (slot == NULL || slot->status == EMPTY))
            USLOSS_Console("MboxCondReceive(): mail slot null or empty, returning -1\n");
        else if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondReceive(): no room for message, room provided: %d, message size: %d, returning -1\n", msg_size, slot->msgSize);
        enableInterrupts();
        return -1;
    }

    // copy the message
    size = slot->msgSize;
    memcpy(msg_ptr, slot->message, size);

    // free the mail slot
    clearSlot(slot->slotId);

    // unblock a proc that is blocked on a send to this mailbox
    if (box->blockedProcSend.size > 0)
    {
        mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
        // create slot for the sender's message
        int slotid = createSlot(proc->msgPtr, proc->msgSize);
        slotPtr slot = &MailSlotTable[slotid];
        enqueue(&box->slots, slot);
        // unblock sender
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondReceive(): unblocking process %d that was blocked on a send\n", proc->pid);
        unblockProc(proc->pid);
    }

    enableInterrupts();
    return size;
}
/*mbox cond receive*/

 
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
    disableInterrupts();
    checkForKernelMode("MboxReceive()");
    slotPtr slot;

    // check for invalid mbox_id
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
             USLOSS_Console("MboxReceive(): called with invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    mailbox *box = &MailBoxTable[mbox_id];
    int size;

    // check if box is valid
    if (box->status == INACTIVE)
    {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxReceive(): invalid box id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // handle 0 slots
    if (box->numSlots == 0)
    {
        mboxProc mproc;
        mproc.nextMboxProc = NULL;
        mproc.pid = getpid();
        mproc.msgPtr = msg_ptr;
        mproc.msgSize = msg_size;

        // if a process has sent, unblock it and get the message
        if (box->blockedProcSend.size > 0)
        {
            mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
            // copy the messag
            sentProc(&mproc, proc->msgPtr, proc->msgSize);


            if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxReceive(): unblocking process %d that was blocked on sent to 0 slot mailbox\n", proc->pid);
            unblockProc(proc->pid);
        }
        else   // otherwise block the receiver
        {
            if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxReceive(): blocking proces %d on 0 slot mailbox\n", mproc.pid);
            enqueue(&box->blockedProcRec, &mproc);
            blockMe(NO_MESSAGES);

            if (isZapped() || box->status == INACTIVE)
            {
                if (DEBUG2 && debugflag2)
                    USLOSS_Console("MboxReceive(): process %d as zapped while blocked on a send, returning -3\n", mproc.pid);
                enableInterrupts();
                return -3;
            } 
        }
  
        enableInterrupts();
        return mproc.msgSize;
    }

    // block if no messages available
    if (box->slots.size == 0)
    {
        // initialize details
        mboxProc mproc;
        mproc.nextMboxProc = NULL;
        mproc.pid = getpid();
        mproc.msgPtr = msg_ptr;
        mproc.msgSize = msg_size;
        mproc.messRec = NULL;

        // handle 0 slot mailbox
        if (box->numSlots == 0 && box->blockedProcSend.size > 0)
        {
            mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
            // copy the message
            //memcpy(mproc->msgPtr, proc->msgPtr, proc->msgSize);
            //mproc->msgSize = proc->msgSize;

            sentProc(&mproc, proc->msgPtr, proc->msgSize);

            if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxReceive(): unblocking process %d that was blocked on send to 0 slot mailbox\n", proc->pid);
            unblockProc(proc->pid);
            enableInterrupts();
            return mproc.msgSize;        
        }

        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxReceive(): no messages available, blocking pid %d...\n", mproc.pid);

        enqueue(&box->blockedProcRec, &mproc);
        blockMe(NO_MESSAGES);
        disableInterrupts();

        // return -3 if process zapped
        if (isZapped() || box->status == INACTIVE)
        {
            if (DEBUG2 && debugflag2)
                USLOSS_Console("MboxReceive(): either process &d was zapped, malbox was freed, or we did not get the message, returning -3\n", mproc.pid);
            enableInterrupts();
            return -3;
        }
        
        return mproc.msgSize;
    }
    else
        slot = dequeue(&box->slots);

    // check if there is not enough room for message
    if (slot == NULL || slot->status == EMPTY || msg_size < slot->msgSize)
    {
        if (DEBUG2 && debugflag2 && (slot == NULL || slot->status == EMPTY))
            USLOSS_Console("MboxReceive(): mail slot null or empty, returning -1\n");
        else if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxReceive(): no room for message, room provided: %d, message size: %d, returning -1\n", msg_size, slot->msgSize);
        enableInterrupts();
        return -1;
    }

    // finally
    size = slot->msgSize;
    memcpy(msg_ptr, slot->message, size);

    // free the mail slot
    clearSlot(slot->slotId);

    // unblock a proc that is blocked on a send
    if (box->blockedProcSend.size > 0)
    {
        mboxProcPtr proc = (mboxProcPtr)dequeue(&box->blockedProcSend);
        // create slot
        int slotID = createSlot(proc->msgPtr, proc->msgSize);
        slotPtr slot = &MailSlotTable[slotID];
        enqueue(&box->slots, slot);
        // unblock sender
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxReceive(): unblocking process %d that was blocked on a send\n", proc->pid);
        unblockProc(proc->pid);
    }
   
    enableInterrupts();
    return size;
    
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - Determine if there are processes blocked on any of the
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

	mailbox *clockbox = &MailBoxTable[IOmailboxes[CLOCKBOX]];
	mailbox *termbox = &MailBoxTable[IOmailboxes[TERMBOX]];
	mailbox *term1box = &MailBoxTable[IOmailboxes[TERMBOX+1]];
	mailbox *term2box = &MailBoxTable[IOmailboxes[TERMBOX+2]];
	mailbox *term3box = &MailBoxTable[IOmailboxes[TERMBOX+3]];
	mailbox *diskbox = &MailBoxTable[IOmailboxes[DISKBOX]];
	mailbox *disk1box = &MailBoxTable[IOmailboxes[DISKBOX+1]];
		
	if ((clockbox->blockedProcSend.size > 0) || (clockbox->blockedProcRec.size > 0))
		return 1;
	if ((termbox->blockedProcSend.size > 0) || (termbox->blockedProcRec.size > 0))
		return 1;
	if ((term1box->blockedProcSend.size > 0) || (term1box->blockedProcRec.size > 0))
		return 1;
	if ((term2box->blockedProcSend.size > 0) || (term2box->blockedProcRec.size > 0))
		return 1;
	if ((term3box->blockedProcSend.size > 0) || (term3box->blockedProcRec.size > 0))
		return 1;	
	if ((diskbox->blockedProcSend.size > 0) || (diskbox->blockedProcRec.size > 0))
		return 1;	
	if ((disk1box->blockedProcSend.size > 0) || (disk1box->blockedProcRec.size > 0))
		return 1;	
	
	return 0;

} /* check_io */

/* ------------------------------------------------------------------------
   Name - waitDevice
   Purpose - 1) Provides results of i/o operations
             2) Use MboxReceive on the appropriate mailbox
   Returns - (-1): the process was zap'd while waiting
			  (0): otherwise 
   Side Effects - none.
   ------------------------------------------------------------------------ */
// type = interrupt device type, unit = # of device (when more than one),
// status = where interrupt handler puts device's status register.
int waitDevice(int type, int unit, int *status)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("waitDevice(): called\n");

	int dev = type;
	int msg_ptr;
	int msg_rtn = -2;
	
	if ((dev == USLOSS_CLOCK_DEV) && (unit == 0))
		msg_rtn = MboxReceive(IOmailboxes[CLOCKBOX], &msg_ptr, sizeof(int));
	else if ((dev == USLOSS_TERM_DEV) && (unit == 0))
		msg_rtn = MboxReceive(IOmailboxes[TERMBOX], &msg_ptr, sizeof(int));
	else if ((dev == USLOSS_TERM_DEV) && (unit == 1))
		msg_rtn = MboxReceive(IOmailboxes[TERMBOX+1], &msg_ptr, sizeof(int));
	else if ((dev == USLOSS_TERM_DEV) && (unit == 2))
		msg_rtn = MboxReceive(IOmailboxes[TERMBOX+2], &msg_ptr, sizeof(int));
	else if ((dev == USLOSS_TERM_DEV) && (unit == 3))
		msg_rtn = MboxReceive(IOmailboxes[TERMBOX+3], &msg_ptr, sizeof(int));
	else if ((dev == USLOSS_DISK_DEV) && (unit == 0))
		msg_rtn = MboxReceive(IOmailboxes[DISKBOX], &msg_ptr, sizeof(int));
	else if ((dev == USLOSS_DISK_DEV) && (unit == 1))
		msg_rtn = MboxReceive(IOmailboxes[DISKBOX+1], &msg_ptr, sizeof(int));
	else
	{
		USLOSS_Console("waitDevice(): Error device %d, unit %d\n", dev, unit);
		USLOSS_Halt(1);
	}

	*status = msg_ptr;
	
    if (DEBUG2 && debugflag2)
        USLOSS_Console("waitDevice(): dev %d, unit %d, msg_ptr %d, status %d\n", dev, unit, msg_ptr, status);

    while (1) 
    {
        if (msg_rtn >= 0)
			break;
        USLOSS_WaitInt();
    }

	if (msg_rtn == -3)
		return -1;
	else
		return 0;
}


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

void* dequeue(queue *q)
{
    void* temp = q->head;
    if (q->head == NULL)
        return NULL;
    if (q->head == q->tail)
        q->head = q->tail = NULL;
    else
    {
        if (q->type == SLOTQUEUE)
            q->head = ((slotPtr)(q->head))->nextSlotPtr;
        else if (q->type == PROCQUEUE)
            q->head = ((mboxProcPtr)(q->head))->nextMboxProc;
    }
    q->size--;
    return temp;
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








