.PHONY:		all

OBJS = espif.o libespif.o
OLIB = libespif.o
ALIB = libespif.a
HLIB = libespif.h
EXE  = espif

%.o:		%.c
			gcc -c -Wall -O3 $< -o $@

all:		$(EXE) $(ALIB)

$(EXE):		$(OBJS) $(ALIB)
			gcc -O3 -s $(OBJS) -L. -lespif -o $@

$(ALIB):	$(OLIB) $(HLIB)
			ar rs $(ALIB) $(OLIB)

espif.o:	$(HLIB)
libespif.o:	$(HLIB)

install:
			install -o root -g adm -m 751 espif /usr/local/bin

clean:
			rm -f $(OBJS) $(OLIB) $(ALIB) $(EXE)
