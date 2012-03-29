# Makefile for ptpd

USE_LINUX_PHC	?= y

#ifeq ($(USE_LINUX_PHC),y)
KBUILD_OUTPUT	?= /lib/modules/$(shell uname -r)/build
CFLAGS		= -DUSE_LINUX_PHC -I$(KBUILD_OUTPUT)/usr/include
PHC_SRC		= dep/phc.c
#endif

RM = rm -f
CFLAGS+= -Wall # -DBSD_INTERFACE_FUNCTIONS
CPPFLAGS =   -DPTPD_DBG
LDFLAGS+= -lm -lrt

PROG = ptpd2
SRCS = ptpd.c arith.c bmc.c protocol.c display.c\
	dep/msg.c dep/net.c dep/servo.c dep/startup.c dep/sys.c dep/timer.c \
	$(PHC_SRC)

OBJS = $(SRCS:.c=.o)

HDRS = ptpd.h constants.h datatypes.h \
	dep/ptpd_dep.h dep/constants_dep.h dep/datatypes_dep.h

CSCOPE = cscope
GTAGS = gtags
DOXYGEN = doxygen

TAGFILES = GPATH GRTAGS GSYMS GTAGS cscope.in.out cscope.out cscope.po.out

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(OBJS): $(HDRS)

tags:
	$(CSCOPE) -R -q -b
	$(GTAGS)
	$(DOXYGEN) Doxyfile

clean:
	$(RM) $(PROG) $(OBJS) $(TAGFILES) make.out
