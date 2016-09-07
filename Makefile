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

install:	$(EXE) $(ALIB) $(HLIB)
			sudo install -o root -g adm -m 751 $(EXE) /nfs/bin
#			sudo install -o root -g adm -m 755 $(ALIB) /nfs/bin
#			sudo install -o root -g adm -m 644 $(HLIB) /usr/local/include

clean:
			rm -f $(OBJS) $(OLIB) $(ALIB) $(EXE)
