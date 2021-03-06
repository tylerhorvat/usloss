PREFIX = ${HOME}

TARGET = libphase4.a
ASSIGNMENT = 452phase4
CC = gcc
AR = ar

COBJS = phase4.o libuser.o
CSRCS = ${COBJS:.o=.c}

PHASE1LIB = patrickphase1
PHASE2LIB = patrickphase2
PHASE3LIB = patrickphase3

HDRS = proc4structs.h

INCLUDE = ${PREFIX}/include

CFLAGS = -Wall -g -std=gnu99 -I. -I${INCLUDE} 

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
        CFLAGS += -D_XOPEN_SOURCE
endif

LDFLAGS += -L. -L${PREFIX}/lib
TESTDIR = testcases
TESTS = test00 test01 test02 test03 test04 test05 test06 test07 test08 \
        test09 test10 test11 test12 test13 test14 test15 test16 test17 \
        test18 test19 test20 test21 test22 test23
		
# For Linux, the phase1 and phase2 libraries have to be here twice.
# For OSX, it is sufficient that they appear once; however, OSX does not
#    care if they are here twice.
LIBS = -l$(PHASE3LIB) -l$(PHASE2LIB) -l$(PHASE1LIB) -lusloss3.6 \
       -l$(PHASE1LIB) -l$(PHASE2LIB) -l$(PHASE3LIB) -lphase4



$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS) 

$(TESTS):	$(TARGET) p1.o
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS) p1.o

clean:
	rm -f $(COBJS) $(TARGET) test*.o test*.txt term* $(TESTS) \
		libuser.o p1.o core

submit: $(CSRCS) $(HDRS) Makefile
	tar cvzf phase4.tgz $(CSRCS) Makefile proc4structs.h

