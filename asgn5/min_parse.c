/*
 * Welcome to min_parse.c!
 * This is a helper file designed to parse through the
 *      arguments used to tell minls and minget what to do.
 * Most of this is string checking and variable setting,
 *      which is then set into a given address for the minls and minget
 *      functions to use during their runtime.
 *
 * Dependencies: min.h
 */

#define _SVID_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include "min.h"

/* set_default
Sets the default values of the args_t variable given to the parser.
Input: args - the struct to set the values of
Output: none
*/
void set_default(args_t* args) {
    args->verbose_mode = OFF;
    args->partition = EMPTY_NUM;
    args->subpartition = EMPTY_NUM;
    args->imagefile = EMPTY_STR;
    args->path = EMPTY_STR;
    args->srcpath = EMPTY_STR;
    args->dstpath = EMPTY_STR;
}

/* parse_args
 * Go through the given arguments of minls or minget and
 *       parse them into variables for the functions to use.
 * Inputs: argc, argv - inputs of the minls or minget entry point
 *          result - the variable the function will populate
 *          with the run details
 * Output: int - status of the function. -1 if an error occurred.
*/
int parse_args(int argc, char *argv[], args_t* result) {
    int opt;
    int giv;
    
    /* Set defaults before starting */
    set_default(result);

    /* Using getopt for our options */
    while ((opt = getopt(argc, argv, ":vp:s:")) != -1) {
        switch(opt) {
            /* Verbose mode */
            case 'v':
                if (result->verbose_mode != EXTRA_ON) {
                    result->verbose_mode++;
                }
                break;
            /* Partition number */
            case 'p':
                giv = atoi(optarg);
                if ((giv < 0) || (giv > 4)){
                    fprintf(stderr, "Invalid partition option!\n");
                    return -1;
                }
                result->partition = giv;
                break;
            /* Subpartition number */
            case 's':
                giv = atoi(optarg);
                if ((giv < 0) || (giv > 4)){
                    fprintf(stderr, "Invalid subpartition option!\n");
                    return -1;
                }
                result->subpartition = giv;
                break;
            /* Unknown option */
            case '?':
                fprintf(stderr, "Unknown option... recieved %c.\n", optopt);
                return -1;
            /* Missing argument */
            case ':':
                fprintf(stderr, "Missing argument for option %c.\n", optopt);
                return -1;
        }
    }
    /* We cannot only have a subpartition */
    if ((result->partition == EMPTY_NUM) &&
        (result->subpartition != EMPTY_NUM)) {
        fprintf(stderr, "Subpartition given without partition!\n");
        return -1;
    }

    /* Check the function we want to run. This is
     *      necessary so that we can look for the correct parameters */
    if (strstr(argv[0], "minget") != NULL) {
        result->chosen_function = MINGET;
    }
    else if (strstr(argv[0], "minls") != NULL) {
        result->chosen_function = MINLS;
    }
    else {
        fprintf(stderr, "Invalid function!\n");
        return -1;
    }

    /* If we only have 1 argument, the function name,
     *      we give usage information */
    if (argc == 1) {
        /* minls specific */
        if (result->chosen_function == MINLS) {
            printf("usage: minls [ -v ] [ -p num [ -s num ] ]");
            printf(" imagefile [ path ]\n");
        }
        /* minget specific */
        else if (result->chosen_function == MINGET) {
            printf("usage: minget [ -v ] [ -p num [ -s num ] ] ");
            printf("imagefile srcpath [ destpath ]\n");
        }
        /* Shouldn't get here */
        else {
            fprintf(stderr, "Could not find the function called!\n");
            return -1;
        }
        /* minls and minget share this information */
        printf("Options:\n");
        printf("-p   part     ");
        printf("--- select partition for filesystem (default: none)\n");
        printf("-s   sub      ");
        printf("--- select subpartition for filesystem (default: none)\n");
        printf("-v   verbose  --- output verbose messages\n");
        return 1;
    }

    /* Sort out non option arguments now that we 
     *      now which function we're running */
    while (optind < argc) {
        /* Imagefile */
        if (strcmp(result->imagefile, EMPTY_STR) == 0) {
            result->imagefile = strdup(argv[optind++]);
            continue;
        }
        /* minls specific */
        if (result->chosen_function == MINLS) {
            /* Path */
            if (strcmp(result->path, EMPTY_STR) == 0) {
                result->path = strdup(argv[optind++]);
                continue;
            }
            /* Any more arguments will be invalid */
            else {
                fprintf(stderr, "Too many arguments!\n");
                return -1;
            }
        }
        /* minget specific */
        else if (result->chosen_function == MINGET) {
            /* Source path */
            if (strcmp(result->srcpath, EMPTY_STR) == 0) {
                result->srcpath = strdup(argv[optind++]);
                continue;
            }
            /* Destination path */
            else if (strcmp(result->dstpath, EMPTY_STR) == 0) {
                result->dstpath = strdup(argv[optind++]);
                continue;
            }
            /* Any more arguments will be invalid */
            else {
                fprintf(stderr, "Too many arguments!\n");
                return -1;
            }
        }
    }

    /* Verbose logging*/
    if (result->verbose_mode > OFF) {
        printf("Chosen function: ");
        result->chosen_function == MINGET
            ? printf("minget\n") : printf("minls\n");
        printf("Verbose levels: %d\n", result->verbose_mode);
        printf("Partition number selected: %d\n", result->partition);
        printf("Subpartition number selected: %d\n", result->subpartition);
        printf("Image file: %s\n", result->imagefile);
        printf("Path: %s\n", result->path);
        printf("Source path: %s\n", result->srcpath);
        printf("Destination path: %s\n", result->dstpath);
        printf("\n");
    }
    return 0;
}