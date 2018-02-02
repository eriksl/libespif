.PHONY:		all

CC = gcc
CCLD = $(CC)
AR = ar
CFLAGS = -O3
LDFLAGS =

OBJS = espif.o libespif.o
OLIB = libespif.o
ALIB = libespif.a
HLIB = libespif.h
EXE  = espif

%.o:		%.c
			$(CC) --std=gnu11 -c -Wall $(CFLAGS) $< -o $@

all:		$(EXE) $(ALIB)

$(EXE):		$(OBJS) $(ALIB)
			$(CCLD) $(LDFLAGS) $(OBJS) -L. -lespif -o $@
			$(STRIP) $@

$(ALIB):	$(OLIB) $(HLIB)
			$(AR) rs $(ALIB) $(OLIB)

espif.o:	$(HLIB)
libespif.o:	$(HLIB)

install:	$(EXE) $(ALIB) $(HLIB)
			cp $(EXE) /home/erik/bin

clean:
			rm -f $(OBJS) $(OLIB) $(ALIB) $(EXE)
