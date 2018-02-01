/* Patrick's DEBUG printing constant... */
#define DEBUG 1
#define READY 1
#define QUIT 4
#define BLOCKED 2
#define RUNNING 3 

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;
/*
typedef struct childProc {
	struct childProc *next;
	struct childProc *prev;
	int quit;			//1 for can be run, and 0 for quit 
	int processPID;
} childProc;


*/


/* Queue struct for ready lists*/
typedef struct procQueue procQueue;
#define READYLIST 0
#define CHILDREN 1
#define DEADCHILDREN 2
#define ZAP 3

struct procQueue {
   procPtr head;
   procPtr tail;
   int size;
   int type; /*which procPtr to use next8*/
};

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int             zapped;
   /* other fields as needed... */
    procPtr parentPtr;
    procQueue  childQueue; /*processes children*/
    int quitStatus;
    procQueue deadChildQueue; 
    procPtr nextDeadSibling;
    
    procQueue zapQueue;
    procPtr nextZapPtr;
    int timeStarted;
    int cpuTime;
    int sliceTime;
};

#define TIMESLICE 80000

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

/*process status*/
#define EMPTY 0
#define READY1 1
#define RUNNING1 2
#define QUIT1 4
#define JBLOCKED 5
#define ZBLOCKED 6
