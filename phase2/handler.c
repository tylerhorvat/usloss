#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include "message.h"

extern int debugflag2;
#define CLOCKBOX 0
#define DISKBOX 1
#define TERMBOX 3

//extern void disableInterrupts(void);
//extern void enableInterrupts(void);
//extern void requireKernelMode(char *);
//extern void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
extern int IOmailboxes[7]; // mboxIDs for the IO devices
//extern int IOblocked = 0; // number of processes blocked on IO mailboxes

int clockInterruptCount = 0;
int Sysargs_number = 0;


/* ------------------------------------------------------------------------
   Name - nullsys
   Purpose - Handles invalid syscalls
   Parameters - USLOSS_Sysargs *args
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", Sysargs_number);
    USLOSS_Halt(1);
		
} /* nullsys */


/* ------------------------------------------------------------------------
   Name - clockHandler2
   Purpose - 1) Check that device is clock
			 2)	Call timeSlice function when necessary
			 3) Conditionally send to the clock i/o mailbox every 5th clock 
			 interrupt.
   Parameters - int dev, void *arg (Ignore arg; it is not used by the clock)
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void clockHandler2(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("clockHandler2(): called\n");

	int unit = 0;
	int status;
	int dev_rtn;
	int msg_rtn;
	
	dev_rtn = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, unit, &status);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("clockHandler2(): dev %d, unit %d, status %d, dev_rtn %d\n", dev, unit, status, dev_rtn);	
	
	if(dev == USLOSS_CLOCK_DEV)
	{
		if(dev_rtn == USLOSS_DEV_OK) 
		{
			clockInterruptCount ++;
			if(clockInterruptCount > 4)
			{
				clockInterruptCount = 0;
			
				timeSlice(); 
			
				msg_rtn = MboxCondSend(IOmailboxes[CLOCKBOX], &status, sizeof(int));
				
				if (DEBUG2 && debugflag2)
					USLOSS_Console("clockHandler2(): msg_rtn %d\n", msg_rtn);
			}
		}
		else
		{
			USLOSS_Console("clockHandler2(): device input error %d\n", dev);
			USLOSS_Halt(1);
		}
	}
	else
	{
		USLOSS_Console("clockHandler2(): device type error %d\n", dev_rtn);
		USLOSS_Halt(1);
	}
	
} /* clockHandler2 */


/* ------------------------------------------------------------------------
   Name - diskHandler
   Purpose - 1) Check that device is disk
			 2) Check that unit number is in the correct range
			 3)	Read disk status register using USLOSS_Device_Input
			 4) Conditionally send status to the appropriate i/o mailbox
			    Conditional send so disk is never blocked on the mailbox
   Parameters - int dev, void *arg
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void diskHandler(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): called\n");

	int unit = (long int) arg;
	int status;
	int dev_rtn;
	int msg_rtn;

	dev_rtn = USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &status);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): dev %d, unit %d, status %d, dev_rtn %d\n", dev, unit, status, dev_rtn);

	if(dev == USLOSS_DISK_DEV)
	{
		if(dev_rtn == USLOSS_DEV_OK) 
		{
			if(unit == 0)
				msg_rtn = MboxCondSend(IOmailboxes[DISKBOX], &status, sizeof(int));
			else if(unit == 1)
				msg_rtn = MboxCondSend(IOmailboxes[DISKBOX+1], &status, sizeof(int));
			else
			{
				USLOSS_Console("diskHandler(): device unit number error %d\n", unit);
				USLOSS_Halt(1);
			}
		}
		else
		{
			USLOSS_Console("diskHandler(): device input error %d\n", dev_rtn);
			USLOSS_Halt(1);
		}
	}
	else
	{
	}
	if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): msg_rtn %d\n", msg_rtn);
	
} /* diskHandler */


/* ------------------------------------------------------------------------
   Name - termHandler
   Purpose - 1) Check that device is terminal
			 2) Check that unit number is in the correct range
			 3)	Read terminal status register using USLOSS_Device_Input
			 4) Conditionally send status to the appropriate i/o mailbox
			    Conditional send so disk is never blocked on the mailbox
   Parameters - int dev, void *arg
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void termHandler(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): called\n");

	int unit = (long int) arg;
	int status;
	int dev_rtn;
	int msg_rtn;
	
	dev_rtn = USLOSS_DeviceInput(USLOSS_TERM_DEV, unit, &status);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): dev %d, unit %d, status %d, dev_rtn %d\n", dev, unit, status, dev_rtn);

	if(dev == USLOSS_TERM_DEV)
	{
		if(dev_rtn == USLOSS_DEV_OK) 
		{
			if(unit == 0)
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX], &status, sizeof(int));
			else if(unit == 1) 
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX+1], &status, sizeof(int));
			else if(unit == 2)
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX+2], &status, sizeof(int));
			else if(unit == 3) 
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX+3], &status, sizeof(int));
			else
			{
				USLOSS_Console("termHandler(): device unit number error %d\n", unit);
				USLOSS_Halt(1);
			}
		}
		else
		{
			USLOSS_Console("termHandler(): device input error %d\n", dev_rtn);
			USLOSS_Halt(1);
		}
	}
	else
	{
		USLOSS_Console("termHandler(): device type error %d\n", dev);
		USLOSS_Halt(1);
	}
    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): msg_rtn %d\n", msg_rtn);
	
} /* termHandler */


/* ------------------------------------------------------------------------
   Name - syscallHandler
   Purpose - Handles system calls.  Invoked by USLOSS_Syscall((void *)&args);
   Parameters - int dev, void *arg
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void syscallHandler(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("syscallHandler(): called\n");

	USLOSS_Sysargs *Sysargs_ptr = (USLOSS_Sysargs *) arg;
	Sysargs_number = Sysargs_ptr->number;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("syscallHandler(): dev %d, number %d\n", dev, Sysargs_number);

	if(dev == USLOSS_SYSCALL_INT)
	{
		if ((Sysargs_number < 0) || (Sysargs_number >= 50))
		{
			USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n", Sysargs_number);
			USLOSS_Halt(1);
		}	
		else
			nullsys((void *)&arg); // 
	}
	else
	{
		USLOSS_Console("syscallHandler(): device type error %d\n", dev);
		USLOSS_Halt(1);
	}

} /* syscallHandler */

