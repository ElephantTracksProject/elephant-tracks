include ../Makefile.inc

include $(wildcard ../Makefile.local)

all: agent_util.o

agent_util.o: agent_util.c agent_util.h
	gcc $(CFLAGS) -I. -c agent_util.c -o agent_util.o

clean:
	rm -f agent_util.o
