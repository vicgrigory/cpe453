CC = gcc
CFLAGS = -Wall -g -fpic
LD = gcc
LDFLAGS = 

intel-all: lib/libmalloc.so lib64/libmalloc.so

lib/libmalloc.so: lib malloc32.o
	$(CC) $(CFLAGS) -m32 -shared -o $@ malloc32.o

lib64/libmalloc.so: lib64 malloc64.o
	$(CC) $(CFLAGS) -shared -o $@ malloc64.o

lib:
	mkdir lib

lib64:
	mkdir lib64

malloc32.o: malloc.c
	$(CC) $(CFLAGS) -m32 -c -o malloc32.o malloc.c

malloc64.o: malloc.c
	$(CC) $(CFLAGS) -m64 -c -o malloc64.o malloc.c

clean:
	rm -f *.o core.* lib/* lib64/* malloc
	rmdir lib lib64

malloc: malloc.c
	$(CC) $(CFLAGS) -o malloc -I ~pn-cs453/Given/Asgn1/include -L ~pn-cs453/Given/Asgn1/lib6 malloc.c
	# ./malloc
	# rm -f malloc
