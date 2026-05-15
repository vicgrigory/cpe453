/* Welcome to min_ls.c!
 * This is the entrypoint for minls. It sets
 *      up the filesystem structure in memory and then
 *      calls the function that prints the desired information.
 * Dependencies: min.h, min_common.c, min_filesystem.c
 */
#include <stdio.h>
#include "min.h"

/* main
 * Entrypoint for minls. Calls the common setup function,
 *          then calls the display function to display the desired file details.
 * Inputs: argc, argv - regular main function arguments.
 *          Parses these in a separate function
 * Output: int - return success code of the function
 */
int main(int argc, char *argv[]) {
    int er;
    /* Sets up the partitions, superblock, root inode,
     *      and finds the inode we want */
    er = setup(argc, argv);
    if (er == -1) {
        fprintf(stderr, "Setup error!\n");
        return -1;
    }

    /* Displays the inode information */
    er = display_minls();
    if (er == -1) {
        fprintf(stderr, "General minls error!\n");
        return -1;
    }

    /* Unmalloc what we did in the filesystem and exit properly */
    er = malloc_exit();
    if (er == -1) {
        fprintf(stderr, "Exit failed, blocks not freed!\n");
    }

    return 0;
}