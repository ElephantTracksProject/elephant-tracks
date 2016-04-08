include ../Makefile.inc

include $(wildcard ../Makefile.local)

all: java_crw_demo.o

java_crw_demo.o: java_crw_demo.h java_crw_demo.c
	$(CC) $(CFLAGS) -I. -c java_crw_demo.c -o java_crw_demo.o

clean:
	rm -f *.o
