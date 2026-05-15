/* Welcome to min_common.c!
 * This is the shared "entrypoint" for both minls and minget.
 * Since minls and minget are very similar, they just call
 *      this function and the parser decides how to proceed
 *      based on the arguments given.
 * Dependencies: min.h, min_filesystem.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "min.h"

/* find_inode
 * Split and go down the path to find the directory with our inode.
 * Inputs: path - the argument's parsed path
 *         res - the inode that will contain the information
 *          minls and minget need
 *         verbose_mode - verbosity level
 * Output: int - function return code
 */
int find_inode(char* path, int verbose_mode) {
    char* next_dir;
    int er;

    /* Use strtok to cut up the path the first time */
    next_dir = strtok(path, "/");

    /* If the directory is not empty, we run */
    if (next_dir != NULL) {
        /* Enter the directory the first time */
        /* Check for filename size limit */
        if (strlen(next_dir) > DIR_SIZE) {
            fprintf(stderr, "Name is too long!\n");
            return -1;
        }
        /* Proceed into the filesystem */
        er = fs_proceed(next_dir, OFF);
        if (er == -1) {
            fprintf(stderr, "Could not go to directory %s!\n", next_dir);
            return -1;
        }
        /* Strtok again */
        next_dir = strtok(NULL, "/");
    }
    
    /* While strtok still gives us an output, we still
     *      have more of the path to go though */
    while (next_dir != NULL) {
        /* Enter the directory */
        er = fs_proceed(next_dir, OFF);
        if (er == -1) {
            fprintf(stderr, "Could not go to directory %s!\n", next_dir);
            return -1;
        }
        /* Strtok again */
        next_dir = strtok(NULL, "/");
    }
    /* When we reach the end of the path, return */
    return 0;
}

/* setup
 * Set up the inode information based on the given arguments.
 *      This includes parsing.
 * Inputs: argc, argv - the function arguments given when
 *          calling the function
 *        final_inode - the inode to fill with the information
 *          used for minget or minls
 * Output: int - function return code
 */
int setup(int argc, char *argv[]) {
    args_t* arguments;
    char* take_path;
    int er;

    /* Parse the arguments that were given and place
     *      into the arguments variable */
    arguments = (args_t*)malloc(sizeof(args_t));
    er = parse_args(argc, argv, arguments);
    if (er == -1) {
        fprintf(stderr, "Had trouble parsing!\n");
        return -1;
    }

    /* Did not use any arguments, exit without file reading */
    else if (er == 1) {
        return 0;
    }

    /* Set the arguments in the filesystem functions */
    set_arguments(arguments);

    /* Check the image to make sure it is correct and
     * place the root inode information into final_inode */
    er = check_image();
    if (er == -1) {
        fprintf(stderr, "Could not find valid filesystem!\n");
        return -1;
    }

    /* Go through the directory and find what we want */
    take_path = arguments->chosen_function == MINLS ?
        arguments->path : arguments->srcpath;

    /* Find the file/ directory we want */
    er = find_inode(take_path, arguments->verbose_mode);
    if (er == -1) {
        fprintf(stderr, "Could not go to given directory!\n");
        return -1;
    }
    if (arguments->verbose_mode == ON) {
        printf("Setup complete!\n\n");
    }
    return 0;
}