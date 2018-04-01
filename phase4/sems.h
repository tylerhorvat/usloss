/* Queue struct for processes */
typedef struct procStruct4 procStruct4;
typedef struct procStruct4 * procPtr;
typedef struct procQueue procQueue;

#define BLOCKED 0
#define CHILDREN 1

struct procQueue {
	procPtr 	head;
	procPtr		tail;
	int		size;
	int		type; /* which procPtr to use for next */
};

/* 
* Process struct for phase 4
*/
struct procStruct4 {
	int             pid;
    	int		mboxID; /* 0 slot mailbox belonging to this process */
    	int (* startFunc) (char *);   /* function where process begins */
    	procPtr         nextProc;
    	procPtr         nextSibling;
    	procPtr         previous;
    	procQueue       childQueue;
};

/* 
* Semaphore struct
*/
typedef struct semaphore semaphore;
struct semaphore {
 	int 		id;
 	int 		value;
 	int 		startingValue;
 	procQueue       blockedProcs;
 	int 		priv_mBoxID;
 	int 		mutex_mBoxID;
 };