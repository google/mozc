PROF=-pg
CFLAGS=-g -Wall $(PROF) -O2
DIST=rx-1.1.2

all	: rx_example rx_test

rx_example	: rx_example.o rx.o
	gcc rx_example.o rx.o $(PROF) -o rx_example

rx_test	: rx_test.o rx.o
	gcc rx_test.o rx.o $(PROF) -o rx_test

rx_example.o	: rx_example.c rx.h
rx_test.o	: rx_test.c rx.h

rx.o	: rx.c rx.h

clean	:
	rm -f *.o

dist	:
	rm -rf $(DIST)
	mkdir $(DIST)
	cp -f README Makefile rx_example.c rx_test.c rx.c rx.h $(DIST)
	tar cvzf $(DIST).tar.gz $(DIST)
