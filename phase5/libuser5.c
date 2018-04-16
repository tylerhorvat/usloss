/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <libuser.h>
#include <usloss.h>
#include <usyscall.h>

#ifdef PHASE_3

/*
 *  Routine:  VmInit
 *
 *  Description: Initializes the VM system.
 *		
 *
 *  Arguments:   int mappings -- number of mappings the MMU can hold
 *               int pages -- number of pages in VM region
 *		 int frames -- number of frames of physical memory
 *	         int pagers -- number of pager daemons to create
 *		 
 *  Return Value: -1 for illegal values, -2 if the region has already been initialized, 
 *                0 otherwise
 */

int VmInit(int mappings, int pages, int frames, int pagers, void **region)
{
    USLOSS_Sysargs sa;
    int		   rc;

    CHECKMODE;
    sa.number = SYS_VMINIT;
    sa.arg1 = (void *) ((long) mappings);
    sa.arg2 = (void *) ((long) pages);
    sa.arg3 = (void *) ((long) frames);
    sa.arg4 = (void *) ((long) pagers);
    USLOSS_Syscall((void *) &sa);
    rc = (int) ((long) sa.arg4);
    if (rc == 0) {
	*region = sa.arg1;
    }
    return rc;
}

/*
 *  Routine:  VmDestroy
 *
 *  Description: Stops the VM system.
 *		
 *
 *  Arguments:   None
 *		 
 *  Return Value: None
 */
void VmDestroy(void)
{
    USLOSS_Sysargs sa;

    CHECKMODE;
    sa.number = SYS_VMDESTROY;
    USLOSS_Syscall((void *) &sa);
    return;
}

#endif 

/* end libuser.c */
