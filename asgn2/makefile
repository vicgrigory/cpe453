CC = gcc
CFLAGS = -Wall -g -fpic


liblwp.so: magic64 lwp.o
	$(CC) $(CFLAGS) −shared −o $@ lwp.o
	
lwp.o: lwp.c
	$(CC) $(CFLAGS) -c -std=c99 -I ~pn-cs453/Given/Asgn2/include -o lwp.o lwp.c

magic64: ~pn-cs453/Given/Asgn2/src/magic64.S
	$(CC) -o magic64.o -c ~pn-cs453/Given/Asgn2/src/magic64.S

clean:
	rm -rf magic64.o