#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
/*
Welcome to pipeit.c!
Function: performs ls | sort -r -> outfile via C + pipes + forking.
Dependencies: standalone
*/


/*
MAIN
Function: forks two children, each running a part
of the bash command specified above.
The parent waits on their closure, reports on their status, and terminates.

Inputs: none
Outputs: int - return code
*/
int main() {
    /* Setup Block */
    int p[2]; // Pipe file descriptors
    pid_t cp_o, cp_t; // Child process IDs
    int c1, c2; // Close pipe read and pipe write return codes respectively
    int dp1, dp2; // dup2 STDIN and STDOUT return codes respectively
    FILE *f; // Output file descriptor
    int fc; // Close file return code
    int status1, status2; // Status of children 1 and 2 respectively
    int exit_s1, exit_s2; // Exit status of children 1 and 2 respectively

    if (pipe(p) == -1) { // Pipe error check
        perror("Failed to open pipe!");
        return 1;
    }


    /* Child 1 */
    cp_o = fork();
    if (cp_o == -1) { // Child creation error check
        perror("Failed to fork first child!");
        return 3;
    }
    if (cp_o == 0) {
        c1 = close(p[0]); // Close pipe read end + error check
        if (c1 == -1) {
            perror("Child 1 - failed to close pipe read!");
            return 1;
        }

        dp2 = dup2(p[1], STDOUT_FILENO);
        // Set pipe write end as STDOUT + error check
        if (dp2 == -1) {
            perror("Child 1 - failed to dup2 pipe write to stdout!");
            return 4;
        }
        c2 = close(p[1]); // Close pipe write end + error check
        if (c2 == -1) {
            perror("Child 1 - failed to close pipe write!");
            return 1;
        }

        execl("/usr/bin/ls", "ls", NULL); // Run ls
        perror("Child 1 - execl failed!");
        // Report an error if execl does not take over this process
        return 2;
    }
    

    /* Child 2 */
    cp_t = fork();
    if (cp_t == -1) { // Child creation error check
        perror("Failed to fork second child!");
        return 3;
    }
    if (cp_t == 0) {
        c2 = close(p[1]); // Close pipe write end + error check
        if (c2 == -1) {
            perror("Child 2 - failed to close pipe write!");
            return 1;
        }

        dp1 = dup2(p[0], STDIN_FILENO);
        // Set pipe read end as STDIN + error check
        if (dp1 == -1) {
            perror("Child 2 - failed to dup2 pipe read to stdin!");
            return 4;
        }
        c1 = close(p[0]); // Close pipe read end + error check
        if (c1 == -1) {
            perror("Child 2 - failed to close pipe read!");
            return 1;
        }

        f = fopen("outfile", "w");
        // Open and overwrite/ create file "outfile" + error check
        if (f == NULL) {
            perror("Child 2 - failed to create output file!");
            return 5;
        }
        dp2 = dup2(fileno(f), STDOUT_FILENO);
        // Set file descriptor as STDOUT + error check
        if (dp2 == -1) {
            perror("Child 2 - failed to dup2 file to stdout!");
            return 4;
        }
        fc = fclose(f); // Close the file descriptor + error check
        if (fc == -1) {
            perror("Child 2 - failed to close file!");
            return 5;
        }
        
        execl("/usr/bin/sort", "sort", "-r", NULL); // Run sort -r
        perror("Child 2 - execl failed!");
        // Report an error if execl does not take over this process
        return 2;
    }
    

    /* Parent */
    c1 = close(p[1]); // Close pipe write end + error check
    if (c1 == -1) {
        perror("Parent: failed to close pipe write!");
        return 1;
    }
    c2 = close(p[0]); // Close pipe read end + error check
    if (c2 == -1) {
        perror("Parent: failed to close pipe read!");
        return 1;
    }

    waitpid(cp_o, &status1, 0); // Wait on child 1
    if (WIFEXITED(status1)) {
        exit_s1 = WEXITSTATUS(status1);
        // Report an error if child did not exit with 0
        if (exit_s1 != 0) {
            perror("Child 1 exited abnormally!\n");
            return 3;
        }
    }

    waitpid(cp_t, &status2, 0); // Wait on child 2
    if (WIFEXITED(status2)) {
        exit_s2 = WEXITSTATUS(status2);
        // Report an error if child did not exit with 0
        if (exit_s2 != 0) {
            perror("Child 2 exited abnormally!\n");
            return 3;
        }
    
    }
    printf("Success!\n"); // To make sure it actually ran and got here
    return 0;
}