/* Welcome to hello.c!
When run, this program prints out the given message (Hello, world!)
Dependencies: none
*/
#include <stdio.h>

/* The message to print */
#ifndef HELLO_MESSAGE
#define HELLO_MESSAGE "Hello, world!\n"
#endif

/* main
Print out the message defined above and exit successfully.
Inputs: none
Outputs: int - return code, should always be 0.
*/
int main() {
    printf(HELLO_MESSAGE);
    return 0;
}