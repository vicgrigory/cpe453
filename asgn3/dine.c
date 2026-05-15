#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

// Time to dawdle during phases
#ifndef DAWDLEFACTOR
#define DAWDLEFACTOR 100
#endif
// Number of philosophers at the table
#ifndef NUM_PHILOSOPHERS
#define NUM_PHILOSOPHERS 5
#endif
// Constants for philosopher status
#define EATING 1
#define THINKING 2
#define READY 0
// Invalid fork
#define INVALID_FORK -1

/*
Welcome to the second C program I've finished (no way), dine.c!
Run this program (optionally with a number) to simulate philosophers dining at
the table and taking turns with their forks. If a number is given, the
philosophers will eat that many times.

Dependencies: standalone
*/

int times = 1; // Number of times to repeat. Defaults to 1

// Struct for philosopher status to display
struct pinfo {
    int status;
    int forkOne;
    int forkTwo;
};

int philid[NUM_PHILOSOPHERS]; // IDs of the philosopher pthreads
pthread_t philthread[NUM_PHILOSOPHERS]; // pthreads of the philosophers
sem_t forks[NUM_PHILOSOPHERS]; // Fork semiphores

struct pinfo philinfo[NUM_PHILOSOPHERS]; // Philosopher information to print
sem_t touchArray; // Semiphore for array writing

/* dawdle
Wait for some random time. Varies philosophers.
Input: none
Output: none
*/
void dawdle() {
    struct timespec tv;
    int msec = (int)((((double)random()) / RAND_MAX) * DAWDLEFACTOR);
    tv.tv_sec = 0;
    tv.tv_nsec = 1000000 * msec;
    if (-1 == nanosleep(&tv, NULL)) {
        perror("nanosleep");
    }
}

/* writeLongLine
Write a line filled with "=". Purely display, used three times.
Input: equals - number of equals signs. Varies with the amount of forks.
Output: none
*/
void writeLongLine(int equals) {
    // First column
    printf("|");
    int i;
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        // Prints the equals signs
        int j;
        for (j = 0; j < equals; j++){
            printf("=");
        }
        // Column marker
        printf("|");
    }
    // New line as a "reset"
    printf("\n");
    return;
}

/* writeLine
Write a line with the current philosopher status and
    which forks they're holding.
Input: none
Output: none
*/
void writeLine() {
    // Iterate over all philosophers
    int i;
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        // Column starter
        printf("| ");
        // Fork logic
        int j;
        for (j = 0; j < NUM_PHILOSOPHERS; j++) {
            // Iterate over the number of forks. If the loop count matches
            // the fork in the philosopher struct (it is being held by that
            // philosopher) then print out that number there.
            if (philinfo[i].forkOne == j || philinfo[i].forkTwo == j) {
                char forkNumber;
                forkNumber = j+'0';
                printf("%c", forkNumber);
            }
            // Otherwise print a dash
            else {
                printf("-");
            }
        }
        // Status logic
        if (philinfo[i].status == READY) {
            printf("       "); // 7 spaces, ready
        }
        else if (philinfo[i].status == EATING) {
            printf(" Eat   "); // 1 space, Eat, 3 spaces
        }
        else if (philinfo[i].status == THINKING) {
            printf(" Think "); // 1 space, Think, 1 space
        }
        else {
            // Weird status number, shouldn't ever get here
            printf("For PID %d: ", i);
            perror("status is invalid!");
            exit(-1);
        }
    }
    printf("|\n");
    return;
}

/* philo
Logic for philosopher pthreads. Downs and ups semiphores to simulate a dinner.
Input: id - the id number of the philosopher as called from main
Output: void* - for the pthread_create. returns NULL */
void *philo(void *id) {
    int myID = *(int*)id; // ID as an int
    int loop = 0; // Loop count
    int firstFork;
    int secondFork;
    int semierror; // Used for semiphore errors

    /* Determine fork order based on ID */
    if (myID % 2 == 0) {
        // Even ID: pick up the fork to the left first
        firstFork = myID + 1;
        secondFork = myID;
    }
    else {
        // Odd ID: pick up the fork to the right first
        firstFork = myID;
        secondFork = myID + 1;
    }

    // Philosophers on the edges should pick up fork 0.
    if (firstFork == NUM_PHILOSOPHERS) {
        firstFork = 0;
    }
    if (secondFork == NUM_PHILOSOPHERS) {
        secondFork = 0;
    }

    // Dining loop
    while (times > loop) {
        /* Down the first fork semiphore */
        semierror = sem_wait(&forks[firstFork]);
        if (semierror == -1) {
            printf("%d: Eat F1: ", myID);
            perror("Fork Semiphore could not be downed!");
            exit(-1);
        }

        /* Make array update with array semiphore */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: EatF1 ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        // firstFork and secondFork are not necessarily in order
        if (firstFork > secondFork) {
            philinfo[myID].forkTwo = firstFork;
        }
        else {
            philinfo[myID].forkOne = firstFork;
        }
        // Write current status as it is
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat F1: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }

        /* Down the second fork semiphore */
        semierror = sem_wait(&forks[secondFork]);
        if (semierror == -1) {
            printf("%d: Eat F2: ", myID);
            perror("Fork semiphore could not be downed!");
            exit(-1);
        }

        /* Make array update with array semiphore */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat F2: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        if (firstFork > secondFork) {
            philinfo[myID].forkOne = secondFork;
        }
        else {
            philinfo[myID].forkTwo = secondFork;
        }
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat F2: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }
        
        /* Change status to eat */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat S1: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        // This is the only thing we need to update here
        philinfo[myID].status = EATING;
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat S1: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }

        /* Dawdle for a while */
        dawdle();

        /* Change status to none */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat S2: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        philinfo[myID].status = READY;
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Eat S2: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }

        /* Make array update with array semiphore */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Think F1: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        if (firstFork > secondFork) {
            philinfo[myID].forkTwo = INVALID_FORK;
        }
        else {
            philinfo[myID].forkOne = INVALID_FORK;
        }
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Think F1: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }
        /* Up the first fork semiphore */
        semierror = sem_post(&forks[firstFork]);
        if (semierror == -1) {
            printf("%d: Think F1: ", myID);
            perror("Fork semiphore could not be upped!");
            exit(-1);
        }

        

        /* Make array update with array semiphore */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Think F2: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        if (firstFork > secondFork) {
            philinfo[myID].forkOne = INVALID_FORK;
        }
        else {
            philinfo[myID].forkTwo = INVALID_FORK;
        }
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Think F2: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }
        /* Up the second fork semiphore */
        semierror = sem_post(&forks[secondFork]);
        if (semierror == -1) {
            printf("%d: Think F2: ", myID);
            perror("Fork semiphore could not be upped!");
            exit(-1);
        }

        /* Change status to think */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Think S1: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        philinfo[myID].status = THINKING;
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Think S1: ", myID);
            perror("Array semiphore could not be upped!");
            exit(-1);
        }

        /* Dawdle for a bit */
        dawdle();

        /* Change status to none/ reset for another loop */
        semierror = sem_wait(&touchArray);
        if (semierror == -1) {
            printf("%d: Think S2: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        philinfo[myID].status = READY;
        writeLine();
        semierror = sem_post(&touchArray);
        if (semierror == -1) {
            printf("%d: Think S2: ", myID);
            perror("Array semiphore could not be downed!");
            exit(-1);
        }
        
        // Increment loop
        loop = loop + 1;
    }
    
    return NULL;
}

/* main
Main function. Creates the given amount of philosophers, parses
the arguments given, and starts the pthreads for the dinner.
Input: int argc, char *argv[] - command arguments when called
Output: int - exit status
*/
int main(int argc, char *argv[]) {
    // Argument parsing. If an argument is given, then
    // overwrite the loop number variable
    if (argv[1] != NULL) {
        times = strtol(argv[1], NULL, 10);
    }

    // This part only really comes up if the loop
    // number variable has been modified. Edge cases.
    if (times < 1) {
        perror("Invalid argument!");
        exit(1);
    }

    int semInitArray;
    semInitArray = sem_init(&touchArray, 0, 1);
    if (semInitArray == -1) {
        perror("Array semiphore could not be initialized!");
        exit(-1);
    }

    int rowChars; // Number of characters between columns. Used for display
    rowChars = 1 + NUM_PHILOSOPHERS + 7;
    // 1 equal for space + i equals (forks) + 7 equals (status)
    
    // Print the first row of equals signs
    writeLongLine(rowChars);

    // Top display row, varies with number of philosophers
    // This loop also creates the fork semiphores and
    // initializes the philosopher status
    int i;
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        // Initialization of dinner
        philid[i] = i;
        int semInitFork;
        semInitFork = sem_init(&forks[i], 0, 1);
        if (semInitFork == -1) {
            printf("%d: ", i);
            perror("Fork semiphore could not be initialized!");
            exit(-1);
        }
        philinfo[i].status = READY;
        philinfo[i].forkOne = INVALID_FORK;
        philinfo[i].forkTwo = INVALID_FORK;

        // Display part
        printf("|");
        int j;
        // Print spaces in front of philosopher letters
        for (j = 0; j < rowChars/2 - 1; j++) {
            // The chars/2 - 1 is intentional
            printf(" ");
        }
        // If we have an odd character count, we add another space
            // To explain:
            // 11: 11/2 = 5
                // 5 + 1 + 5
                // (5-1 + 1) + 1 + 5
                // We need to add an extra space here
            // 12: 12/2 = 6
                //  5 + 1 + 6
                // (6-1) + 1 + 6
                // Works with no intervention
        if (rowChars % 2 == 1) {
            printf(" ");
        }
        // Print the ascii character
        char myAscii;
        myAscii = i + 'A';
        printf("%c", myAscii);
        // Print the rest of the spaces
        for (j = 0; j < rowChars/2; j++) {
            printf(" ");
        }
    }
    // Finish the row along with a new line
    printf("|\n");
    // Print equals sign row
    writeLongLine(rowChars);
    // Print initial statuses before processes begin
    writeLine();

    // Initialize the philosopher pthreads
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        int r;
        r = pthread_create(&philthread[i], NULL, &philo, (void *)(&philid[i]));
        if (r == -1) {
            printf("%d: ", i);
            perror("Philosopher pthread could not be created!");
            exit(-1);
        }
    }

    // Wait for them all to finish
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(philthread[i], NULL);
    }
    // Print last equals sign row
    writeLongLine(rowChars);
    return 0;
}