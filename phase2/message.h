
#define DEBUG2 1

typedef struct mailSlot  *slotPtr;
typedef struct mailbox    mailbox;
typedef struct mailSlot   mailSlot;
typedef struct mboxProc   mboxProc;
typedef struct mboxProc  *mboxProcPtr;
typedef struct queue queue;
void clockHandler2(int, void *);
void nullsys(USLOSS_Sysargs *);
void diskHandler(int, void *);
void termHandler(int, void *);
void syscallHandler(int, void *);

struct queue {
    void      *head;
    void      *tail;
    int        size;
    int        type;
};

struct mailbox {
    int       mboxID;
    int       status;
    int       numSlots;
    int       slotSize;
    queue     slots;              // queue of mailSlots in this mailbox
    queue     blockedProcSend;    // procs blocked on send
    queue     blockedProcRec;     // procs blocked on receive
    // other items as needed...
};

struct mailSlot {
    int       mboxID;
    int       status;
    int       slotId;
    slotPtr   nextSlotPtr;
    char      message[MAX_MESSAGE];
    int       msgSize;
    // other items as needed...
};

struct mboxProc {
    mboxProcPtr    nextMboxProc;
    int            pid;
    void          *msgPtr;
    int            msgSize;
    slotPtr        messRec;
};

// mailbox status constants
#define INACTIVE 0
#define ACTIVE 1

// mail slot status conatants
#define EMPTY 0
#define USED 1

// process status constants
#define FULL_BOX 11
#define NO_MESSAGES 12

#define SLOTQUEUE 0
#define PROCQUEUE 1

//defines for message boxes 
#define CLOCKBOX 0
#define DISKBOX 1
#define TERMBOX 3


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
