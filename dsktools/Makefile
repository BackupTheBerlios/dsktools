#$Id: Makefile,v 1.6 2003/08/24 19:53:38 nurgle Exp $

# build targets

all:	dskwrite dskread

clean:
	rm dskread dskwrite *.o *~

# edit and debug targets

gvim:
	gvim *.c *.h Makefile ChangeLog TODO README AUTHORS

tr:
	time ./dskread x.dsk

tw:
	time ./dskwrite x.dsk

# dependencies

dskread: dskread.c common.o
	gcc -g -o dskread dskread.c common.o

dskwrite: dskwrite.c common.o
	gcc -g -o dskwrite dskwrite.c common.o

common.o: common.c
	gcc -g -c common.c
