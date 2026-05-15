CC = gcc
CFLAGS = -Wall -ansi -pedantic -g

hello: hello.o
	$(CC) -g -o hello hello.o
hello.o: hello.c
	$(CC) $(CFLAGS) -c -o hello.o hello.c

test: hello
	@echo "Testing hello..."
	@./hello
	@echo "done."

clean:
	rm -rf hello.o *~ TAGS