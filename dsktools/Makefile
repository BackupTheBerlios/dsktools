#$Id: Makefile,v 1.5 2001/12/30 17:16:54 nurgle Exp $

all:	dskwrite dskread

clean:
	rm dskread dskwrite *.o *~

gvim:
	gvim *.c *.h Makefile ChangeLog TODO README AUTHORS

dskread: dskread.c common.o
	gcc -g -o dskread dskread.c common.o

dskwrite: dskwrite.c common.o
	gcc -g -o dskwrite dskwrite.c common.o

common.o: common.c
	gcc -g -c common.c
