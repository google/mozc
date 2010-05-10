PROF=-pg
CFLAGS=-g -Wall $(PROF) -O2
DIST=rx-1.0rc2

main	: main.o rx.o
	gcc main.o rx.o $(PROF) -o main

main.o	: main.c rx.h

rx.o	: rx.c rx.h

clean	:
	rm -f *.o

dist	:
	rm -rf $(DIST)
	mkdir $(DIST)
	cp -f README Makefile main.c rx.c rx.h $(DIST)
	tar cvzf $(DIST).tar.gz $(DIST)
