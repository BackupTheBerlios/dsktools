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
