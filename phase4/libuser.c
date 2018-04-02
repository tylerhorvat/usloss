/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <libuser.h>

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

/*
 *  Routine:  Sleep
 *
 *  Description:  Delays the calling process for the specified number of seconds (sleep).
 *
 *  Input:  
 *			arg1: number of seconds to delay the process
 *
 *  Output:  
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *
 */
int  Sleep(int seconds)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) ((long) seconds);

    USLOSS_Syscall(&sysArg);
	
    return (long) sysArg.arg4;	
}


/*
 *  Routine:  DiskRead
 *
 *  Description:  Reads one or more sectors from a disk (diskRead).
 *
 *  Input:
 *			arg1: the memory address to which to transfer
 *			arg2: number of sectors to read
 *			arg3: the starting disk track number
 *			arg4: the starting disk sector number
 *			arg5: the unit number of the disk from which to read
 *			
 *  Output:       
 *			arg1: 0 if transfer was successful; the disk status register otherwise.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 *	Note:  The arg4 result is only set to -1 if any of the input parameters are 
 *         obviously invalid, e.g. the starting sector is negative.
 * 
 */
int  DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) ((long) sectors);
	sysArg.arg3 = (void *) ((long) track);
	sysArg.arg4 = (void *) ((long) first);
    sysArg.arg5 = (void *) ((long) unit);		
	
    USLOSS_Syscall(&sysArg);
    status = (int *) sysArg.arg1;
    return (long) sysArg.arg4;
}


/*
 *  Routine:  DiskWrite
 *
 *  Description:  Writes one or more sectors to the disk (diskWrite).
 *
 *  Input:
 *			arg1: the memory address from which to transfer.
 *			arg2: number of sectors to write.
 *			arg3: the starting disk track number.
 *			arg4: the starting disk sector number.
 *			arg5: the unit number of the disk to write.
 *			
 *  Output:       
 *			arg1: 0 if transfer was successful; the disk status register otherwise.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 *	Note:  The arg4 result is only set to -1 if any of the input parameters are 
 *         obviously invalid, e.g. the starting sector is negative.
 * 
 */
int  DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) ((long) sectors);
	sysArg.arg3 = (void *) ((long) track);
	sysArg.arg4 = (void *) ((long) first);
    sysArg.arg5 = (void *) ((long) unit);		
	
    USLOSS_Syscall(&sysArg);
    status = (int *) sysArg.arg1;
    return (long) sysArg.arg4;
}


/*
 *  Routine:  DiskSize
 *
 *  Description:  Returns information about the size of the disk (diskSize).
 *
 *  Input:
 *			arg1: the unit number of the disk
 *			
 *  Output:       
 *			arg1: size of a sector, in bytes
 *			arg2: number of sectors in a track
 *			arg3: number of tracks in the disk
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *
 */
int  DiskSize(int unit, int *sector, int *track, int *disk)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) ((long) unit);
	
    USLOSS_Syscall(&sysArg);
    sector = (int *) sysArg.arg1;
    track = (int *) sysArg.arg2;
	disk = (int *) sysArg.arg3;
    return (long) sysArg.arg4;
}


/*
 *  Routine:  TermRead
 *
 *  Description:  Read a line from a terminal (termRead).
 *
 *  Input:
 *			arg1: address of the user’s line buffer.
 *			arg2: maximum size of the buffer.
 *			arg3: the unit number of the terminal from which to read.
 *			
 *  Output:       
 *			arg2: number of characters read.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 */
int  TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void *) ((long) bsize);
	sysArg.arg3 = (void *) ((long) unit_id);		
	
    USLOSS_Syscall(&sysArg);
    nread = (int *) sysArg.arg1;
    return (long) sysArg.arg4;	
}


/*
 *  Routine:  TermWrite
 *
 *  Description:  Write a line to a terminal (termWrite).
 *
 *  Input:
 *			arg1: address of the user’s line buffer.
 *			arg2: number of characters to write.
 *			arg3: the unit number of the terminal to which to write.
 *			
 *  Output:       
 *			arg2: number of characters written.
 *			arg4: -1 if illegal values are given as input; 0 otherwise.
 *			
 */
int  TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
	
    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void *) ((long) bsize);
	sysArg.arg3 = (void *) ((long) unit_id);		
	
    USLOSS_Syscall(&sysArg);
    nwrite = (int *) sysArg.arg1;
    return (long) sysArg.arg4;	
}


/* end libuser.c */
