#$Id: Makefile,v 1.4 2001/12/30 16:31:10 nurgle Exp $

all:	dskwrite dskread

clean:
	rm dskread dskwrite *.o *~

gvim:
	gvim *.c *.h Makefile ChangeLog TODO

dskread: dskread.c common.o
	gcc -g -o dskread dskread.c common.o

dskwrite: dskwrite.c common.o
	gcc -g -o dskwrite dskwrite.c common.o

common.o: common.c
	gcc -g -c common.c
