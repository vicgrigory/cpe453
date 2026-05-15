CC = gcc
CFLAGS = -Wall -g -std=c99 -pedantic -O -D_XOPEN_SOURCE=700
LD = gcc
LDFLAGS = 

pipeit: pipeit.c
	$(CC) $(CFLAGS) pipeit.c -o pipeit

clean:
	rm -f pipeit outfile