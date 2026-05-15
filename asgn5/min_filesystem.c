/* Welcome to min_filesystem.c!
 * This is the logic for all the filesystem information
 *      and manipulation, and is what actually reads and parses the image data.
 * Dependencies: min.h
 */

#define _SVID_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "min.h"

args_t* arguments; /* Arguments passed */
FILE* image; /* Open file descriptor for the image file */
ptable_t* partition_table; /* Partition table */
ptable_t* subpartition_table;
/* Subpartition table for our desired partition */
sb_t* superblock; /* Superblock information */

int partition_offset; /* Offset of the partition we're looking at */
int inode_byte_start; /* The byte where the inode block starts */
int zonesize; /* Size of a zone */
int dirs_in_zone; /* Number of directories in a zone */
int num_zones_total; /* Number of zones total in a file */

unsigned char* last_filename; /* Last inode file looked at in directory */

in_t* root_inode; /* Root inode information */
in_t* cur_inode; /* Inode for a current directory or file */

char* pathname; /* The path to print */

/* set_arguments
 * Sets the arguments to the global variable for later use.
 * Input: given - the args_t struct from the parser
 * Output: none
 */
void set_arguments(args_t* given) {
    arguments = given;
}

/* check_partition
 * Checks the validity of the partition (or subpartition). Used in check image.
 * Inputs: offset - offset of the partition to check
 *         partition_num - partition or subpartition number
 *         table - the table struct to fill
 * Output: int - function return code
 */
int check_partition(int offset, int partition_num, ptable_t* table) {
    unsigned char valid_check[2];
    int er;

    /* Check byte 510 and 511 */
    er = fseek(image, offset + BYTE_CHECK_POS, SEEK_SET);
    if (er == -1) {
        fprintf(stderr, "Could not seek to byte 510!\n");
        return -1;
    }
    er = fread(valid_check, sizeof(char), BYTE_CHECK_NUM, image);
    if (er != BYTE_CHECK_NUM) {
        fprintf(stderr, "Could not read proper byte amount!\n");
        return -1;
    }
    if (valid_check[0] != BYTE_FIVE_TEN_VPT_MINIX ||
        valid_check[1] != BYTE_FIVE_ELEVEN_VPT_MINIX) {
        fprintf(stderr, "Bytes 510 and 511 do not match a valid filesystem!\n");
        fprintf(stderr, "510: %x, 511: %x\n", valid_check[0], valid_check[1]);
        return -1;
    }

    /* Read partition information into our struct */
    er = fseek(image, offset + PARTITION_TABLE_LOCATION, SEEK_SET);
    if (er == -1) {
        fprintf(stderr, "Could not seek to partition table!\n");
        return -1;
    }
    er = fread(table, sizeof(ptable_t), NUM_PARTITIONS, image);
    if (er != NUM_PARTITIONS) {
        fprintf(stderr, "Could not read partition table!\n");
        return -1;
    }

    /* Check for the Minix partition flag */
    if (table[partition_num].type != PARTITION_TYPE_MINIX_FLAG) {
        fprintf(stderr, "This partition is not a Minix partition!\n");
        return -1;
    }

    return 0;
}

/* check_superblock
 * Checks the validity of a superblock.
 * Input: partition_offset - offset of the partition, or the byte it starts at
 * Output: int - function return code
 */
int check_superblock(int partition_offset) {
    int er;

    /* Put superblock into its struct */
    er = fseek(image, partition_offset + SUPERBLOCK_OFFSET, SEEK_SET);
    if (er == -1) {
        fprintf(stderr, "Could not seek to the superblock!\n");
        return -1;
    }
    er = fread(superblock, sizeof(sb_t), 1, image);
    if (er != 1) {
        fprintf(stderr, "Could not read superblock information!\n");
        return -1;
    }

    /* Check if this filesystem contains the Minix magic number */
    if (superblock->magic != MINIX_MAGIC_NUMBER) {
        fprintf(stderr, "Filesystem does not contain Minix magic number!\n");
        return -1;
    }
    return 0;
}

/* check_image
 * Main function that checks for image validity. Opens the
 *      file descriptor and initializes other values.
 * Input: none
 * Output: int - function return code
 */
int check_image() {
    int er;
    /* Checking if image exists and is openable */
    image = fopen(arguments->imagefile, "rb");
    if (image == NULL) {
        fprintf(stderr, "Could not open given image file!\n");
        return -1;
    }

    /* This is where we would base our calculations
     *      if the disk is unpartitioned */
    partition_offset = 0;
    
    /* If we do not have a specified partition, we skip
     *      it and look for the superblock */
    if (arguments->partition != EMPTY_NUM) {
        /* Check partition validity*/
        partition_table = (ptable_t*)malloc(NUM_PARTITIONS * sizeof(ptable_t));
        er = check_partition(0, arguments->partition, partition_table);
        if (er == -1) {
            fprintf(stderr, "Invalid partition!\n");
            er = fclose(image);
            if (er == -1) {
                fprintf(stderr, "Could not close image file!\n");
            }
            free(partition_table);
            return -1;
        }
        /* Set new partition offset since we have partitions */
        partition_offset = partition_table[arguments->partition].lfirst
            * SECTOR_SIZE;
        /* Note: for the sake of this homework, we throw an error
         *      here and exit if the partition is not Minix. It is
         *      theoretically possible that the partition is not Minix
         *      and the subpartition is. */
        
        if (arguments->verbose_mode > OFF) {
            printf("--------- Partition %d ---------\n", arguments->partition);
            printf("bootind | shead | ssector | scylinder | type\n");
            printf("%d | ", partition_table->bootind);
            printf("%d | ", partition_table->start_head);
            printf("%d | ", partition_table->start_sec);
            printf("%d | ", partition_table->start_cyl);
            printf("%d\n", partition_table->type);
            printf("------------------------------\n");
            printf("ehead | esector | ecylinder | lfirst | size\n");
            printf("%d | ", partition_table->end_head);
            printf("%d | ", partition_table->end_sec);
            printf("%d | ", partition_table->end_cyl);
            printf("%d | ", partition_table->lfirst);
            printf("%d\n", partition_table->size);
        }
        /* If we have a subpartition, we need to find its partition table */
        if (arguments->subpartition != EMPTY_NUM) {
            /* Check subpartition validity */
            subpartition_table=
                (ptable_t*)malloc(NUM_PARTITIONS*sizeof(ptable_t));
            er = check_partition(partition_offset,
                arguments->subpartition, subpartition_table);
            if (er == -1) {
                er = fclose(image);
                if (er == -1) {
                    fprintf(stderr, "Could not close image file!\n");
                }
                free(partition_table);
                free(subpartition_table);
                fprintf(stderr, "Invalid subpartition!\n");
                return -1;
            }
            /* Set our offset to the subpartition */
            partition_offset=subpartition_table[arguments->subpartition].lfirst
                * SECTOR_SIZE;

            /* Verbose logging */
            if (arguments->verbose_mode > OFF) {
                printf("--------- Subpartition %d ---------\n",
                    arguments->partition);
                printf("bootind | shead | ssector | scylinder | type\n");
                printf("%d | ", subpartition_table->bootind);
                printf("%d | ", subpartition_table->start_head);
                printf("%d | ", subpartition_table->start_sec);
                printf("%d | ", subpartition_table->start_cyl);
                printf("%d\n", subpartition_table->type);
                printf("------------------------------\n");
                printf("ehead | esector | ecylinder | lfirst | size\n");
                printf("%d | ", subpartition_table->end_head);
                printf("%d | ", subpartition_table->end_sec);
                printf("%d | ", subpartition_table->end_cyl);
                printf("%d | ", subpartition_table->lfirst);
                printf("%d\n", subpartition_table->size);
            }
        }
    }
    
    /* Check superblock/ filesystem validity */
    superblock = (sb_t*)malloc(sizeof(sb_t));
    er = check_superblock(partition_offset);
    if (er == -1) {
        fprintf(stderr, "Invalid superblock!\n");
        er = fclose(image);
        if (er == -1) {
            fprintf(stderr, "Could not close image file!\n");
        }
        free(superblock);
        if (arguments->partition != EMPTY_NUM) {
            free(partition_table);
        }
        if (arguments->subpartition != EMPTY_NUM) {
            free(subpartition_table);
        }
        return -1;
    }

    /* Verbose logging*/
    if (arguments->verbose_mode > OFF) {
        printf("--------- Superblock ---------\n");
        printf("ninodes | pad1 | i_blocks | z_blocks | firstdata\n");
        printf("%d | ", superblock->ninodes);
        printf("%d | ", superblock->pad1);
        printf("%d | ", superblock->i_blocks);
        printf("%d | ", superblock->z_blocks);
        printf("%d\n", superblock->firstdata);
        printf("------------------------------\n");
        printf("log_zone_size | pad2 | max_file | zones | magic | pad3\n");
        printf("%d | ", superblock->log_zone_size);
        printf("%d | ", superblock->pad2);
        printf("%d | ", superblock->max_file);
        printf("%d | ", superblock->zones);
        printf("%d | ", superblock->magic);
        printf("%d\n", superblock->pad3);
        printf("------------------------------\n");
        printf("blocksize | subversion\n");
        printf("%d | ", superblock->blocksize);
        printf("%d\n", superblock->subversion);
    }

    /* Calculate where the root inode is */
    inode_byte_start = (2 + superblock->i_blocks + superblock->z_blocks) *
        superblock->blocksize + partition_offset;

    /* Put root inode into its struct */
    root_inode = (in_t*)malloc(sizeof(in_t));
    er = fseek(image, inode_byte_start , SEEK_SET);
    if (er == -1) {
        fprintf(stderr, "Could not seek to the first inode!\n");
        er = fclose(image);
        if (er == -1) {
            fprintf(stderr, "Could not close image file!\n");
        }
        free(root_inode);
        free(superblock);
        if (arguments->partition != EMPTY_NUM) {
            free(partition_table);
        }
        if (arguments->subpartition != EMPTY_NUM) {
            free(subpartition_table);
        }
        return -1;
    }
    er = fread(root_inode, sizeof(in_t), 1, image);
    if (er != 1) {
        fprintf(stderr, "Could not read root inode information!\n");
        er = fclose(image);
        if (er == -1) {
            fprintf(stderr, "Could not close image file!\n");
        }
        free(root_inode);
        free(superblock);
        if (arguments->partition != EMPTY_NUM) {
            free(partition_table);
        }
        if (arguments->subpartition != EMPTY_NUM) {
            free(subpartition_table);
        }
        return -1;
    }

    /* Verbose logging */
    if (arguments->verbose_mode > OFF) {
        printf("\n");
        printf("--------- Root Inode ---------\n");
        printf("mode | links | uid | gid | size\n");
        printf("%d | ", root_inode->mode);
        printf("%d | ", root_inode->links);
        printf("%d | ", root_inode->uid);
        printf("%d | ", root_inode->gid);
        printf("%d\n", root_inode->size);
        printf("------------------------------\n");
        printf("atime | mtime | ctime\n");
        printf("%d | ", root_inode->atime);
        printf("%d | ", root_inode->mtime);
        printf("%d\n", root_inode->ctime);
        printf("------------------------------\n");
        printf("zone 0 | indirect | dindirect\n");
        printf("%d | ", root_inode->zone[0]);
        printf("%d | ", root_inode->indirect);
        printf("%d\n", root_inode->two_indirect);
    }

    /* Variable initialization */
    zonesize = superblock->blocksize << superblock->log_zone_size;
    dirs_in_zone = zonesize/sizeof(d_entry);
    cur_inode = root_inode;
    last_filename = (unsigned char *)"/";
    num_zones_total = DIRECT_ZONES +
        ((superblock->blocksize/sizeof(uint32_t))^2) +
            ((superblock->blocksize/sizeof(uint32_t))^3);
    pathname = (char*)malloc(sizeof("/"));
    strcpy(pathname, "/");
    return 0;
}

/* fetch_inode
 * Fetch the inode information of a given inode number.
 * Input: inode_number - the inode number as an int
 *        place_inode - buffer to place the inode information into
 * Output: int - function return code
 */
int fetch_inode(int inode_number, in_t* place_inode) {
    int inode_location;
    int er;

    /* We cannot fetch an inode less than 1 */
    if (inode_number < 1) {
        return 1;
    }

    /* Calculate the byte address of where the inode is */
    inode_location = inode_byte_start + (inode_number-1) * sizeof(in_t);

    /* Get inode information */
    er = fseek(image, inode_location, SEEK_SET);
    if (er == -1) {
        fprintf(stderr, "Could not seek to inode location!\n");
        return -1;
    }
    er = fread(place_inode, sizeof(in_t), 1, image);
    if (er == -1) {
        fprintf(stderr, "Could not read desired inode!\n");
        return -1;
    }

    return 0;
}

/* fetch_zone_content
 * Fetch the information located at a given zone.
 * Input: zone - zone number to fetch from
 *        location - buffer to fill with information
 *        size - size of the elements to write
 *        times - count variable in fread
 * Output: int - function return code
 */
int fetch_zone_content(int zone, void* location, int size, int times) {
    int byte;
    int er;

    /* We shouldn't ever be fetching anything from
     *      zone 0 once we're done initializing */
    if (zone < 1) {
        fprintf(stderr, "Zone 0 is invalid!\n");
        return -1;
    }

    /* Find the byte address of the zone we want */
    byte = zone * zonesize;

    /* Get the zone content */
    er = fseek(image, byte + partition_offset, SEEK_SET);
    if (er == -1) {
        fprintf(stderr, "Could not seek to zone location!\n");
        return -1;
    }
    er = fread(location, size, times, image);
    if (er == -1) {
        fprintf(stderr, "Could not read from zone location!\n");
        return -1;
    }

    return 0;
}

/* compare_directory
 * Compares the directory name with the path name we're looking for.
 * Inputs: directory - directory entry struct to look into
 *         name - name to check for
 * Output: int - inode number of the match. -1 if not matching.
 */
int compare_directory(d_entry* directory, char* name) {
    int res;

    /* Compare and return inode number if it matches */
    res = strncmp((char*)directory->name, name, DIR_SIZE);
    if (res == 0) {
        return directory->inode;
    }
    else {
        return -1;
    }
}

/* inode_print
 * Prints out the information of a given inode.
 * Input: inode - inode struct to print information for
 *         dir_print_bool - trigger to print a directory
 *              instead of just one inode
 * Output: none
 */
void inode_print(in_t* inode, int dir_print_bool) {
    /* Case checks for every permission + print */
    (inode->mode & DIRECTORY_OCTAL) == DIRECTORY_OCTAL
        ? printf("d") : printf("-");
    (inode->mode & OWNER_READ_OCTAL) == OWNER_READ_OCTAL
        ? printf("r") : printf("-");
    (inode->mode & OWNER_WRITE_OCTAL) == OWNER_WRITE_OCTAL
        ? printf("w") : printf("-");
    (inode->mode & OWNER_EXECUTE_OCTAL) == OWNER_EXECUTE_OCTAL
        ? printf("x") : printf("-");
    (inode->mode & GROUP_READ_OCTAL) == GROUP_READ_OCTAL
        ? printf("r") : printf("-");
    (inode->mode & GROUP_WRITE_OCTAL) == GROUP_WRITE_OCTAL
        ? printf("w") : printf("-");
    (inode->mode & GROUP_EXECUTE_OCTAL) == GROUP_EXECUTE_OCTAL
        ? printf("x") : printf("-");
    (inode->mode & OTHER_READ_OCTAL) == OTHER_READ_OCTAL
        ? printf("r") : printf("-");
    (inode->mode & OTHER_WRITE_OCTAL) == OTHER_WRITE_OCTAL
        ? printf("w") : printf("-");
    (inode->mode & OTHER_EXECUTE_OCTAL) == OTHER_EXECUTE_OCTAL
        ? printf("x") : printf("-");

    printf("   ");
    
    /* Print the size of the inode in bytes */
    printf("%d", inode->size);

    printf("   ");
    /* Print the name of the inode */
    if (dir_print_bool == OFF) {
        printf("%s", pathname+1);
    }
    else {
        printf("%s", last_filename);
    }

    printf("\n");
}

/* dir_loop
 * The function that actually looks inside the directory
 *      contents and does the print / compare logic.
 * Inputs: cur - the directory entry to loop through
 *         name - the name to search for if the mode is set to OFF
 *         mode - whether to print or compare
 * Outputs: int - function return code
 */
int dir_loop(d_entry* cur, char* name, int mode) {
    int res;
    int er;
            
    /* If we want to print inode information ... */
    if (mode == ON) {
        /* Set information to print inode stuff */
        last_filename = cur->name;

        if (arguments->verbose_mode == ON && cur->inode != 0) {
            printf("Entry name: %s\n", cur->name);
            printf("Trying to fetch %d\n", cur->inode);
        }
        /* Get the more detailed info about our inode */
        er = fetch_inode(cur->inode, cur_inode);
        if (er == -1) {
            fprintf(stderr, "Could not load inode!\n");
            return -2;
        }

        /* Return if the inode was deleted and do not print */
        if (cur_inode->links == 0 || er == 1) {
            return 0;
        }
        /* If the inode was 0 */
        if (er == 1) {
            return 2;
        }

        /* Print the inode info */
        inode_print(cur_inode, ON);
        return 0;
    }

    /* If we just want to compare and find a specific directory entry ... */
    else if (mode == OFF) {
        if (arguments->verbose_mode == ON) {
            printf("Comparing %s and %s..\n", cur->name, name);
        }
        /* Compare to the name we're looking for */
        res = compare_directory(cur, name);
        if (res != -1) {
            /* If we match then we load that inode and exit */
            last_filename = cur->name;
            er = fetch_inode(cur->inode, cur_inode);
            if (er == -1) {
                fprintf(stderr, "Could not load inode!\n");
                return -1;
            }

            /* If that inode is deleted, throw an error */
            if (cur_inode->links == 0 || er == 1) {
                fprintf(stderr, "File referred to was deleted!\n");
                return -1;
            }

            /* Indicate that we found a match*/
            return 1;
        }
        else {
            /* If we don't match we move on */
            return 0;
        }
    }

    /* Should not reach this point */
    return -1;
}

/* gather_zones
 * Gather the zones used to store file information and
 *      store it in the given int array buffer.
 * Inputs: inode - the inode to look through
 *         zones - the array of all zone numbers
 *         non_empty - the array of nonempty zone numbers
 *         total - number of total zones found, including 0s
 * Output: int - the amount of full zones found
 */
int gather_zones(in_t* inode, int* zones, int* non_empty, int* total) {
    /* Information to keep track of to calculate zone validity */
    int j; /* Total zones gone through */
    int k; /* Zones that are not empty */
    int direct_i; /* Counter for direct zones */
    int direct_first_i; /* First nonempty direct zone */
    int direct_last_i; /* Last nonempty direct zone */
    int indirect_i; /* Counter for indirect zones */
    int indirect_first_i; /* First nonempty indirect zone */
    int indirect_last_i; /* Last nonempty indirect zone */
    int d_indirect_first_i; /* First nonempty d-indirect zone */
    int d_indirect_last_i; /* Last nonempty d-indirect zone */
    int num_zones; /* Max number of zones possible in a block */
    uint32_t* indirect; /* Array for indirect zones */
    uint32_t* double_indirect; /* Array for d-indirect zones */
    int er;

    /* Initialize */
    num_zones = superblock->blocksize/sizeof(uint32_t);
    direct_first_i = EMPTY_NUM;
    direct_last_i = EMPTY_NUM;
    indirect_first_i = EMPTY_NUM;
    indirect_last_i = EMPTY_NUM;
    d_indirect_first_i = EMPTY_NUM;
    d_indirect_last_i = EMPTY_NUM;
    j = 0;
    k = 0;

    /* Verbose logging */
    if (arguments->verbose_mode == ON) {
        printf("direct zones:\n");
        for (direct_i = 0; direct_i < DIRECT_ZONES; direct_i++) {
            printf("zone %d: %d\n", direct_i, inode->zone[direct_i]);
        }
        printf("indirect: %d\n", inode->indirect);
        printf("two indirect: %d\n", inode->two_indirect);
        printf("\n");
    }

    /* Go through each type of zone to retrieve full data zones */

    /* Direct zones */
    for (direct_i = 0; direct_i < DIRECT_ZONES; direct_i++) {
        /* Add this to our zone array */
        zones[j] = inode->zone[direct_i];
        /* If the zone is not empty, we add it to our full zone array  */
        if (inode->zone[direct_i] != 0) {
            /* Set first nonempty zone if not done already */
            if (direct_first_i == EMPTY_NUM) {
                direct_first_i = direct_i;
            }
            non_empty[k] = inode->zone[direct_i];
            /* Set last nonempty zone */
            direct_last_i = direct_i;
            k++;
        }
        j++;
    }

    /* Indirect zones */
    if (inode->indirect != 0) {
        /* Reinitialize others for correct calculation */
        direct_last_i = DIRECT_ZONES-1;
        /* Fetch the content at the indirect zone to
         *      get the new zone directories */
        indirect = (uint32_t*)malloc(superblock->blocksize);
        er = fetch_zone_content(inode->indirect,
            indirect, sizeof(uint32_t), num_zones);
        if (er == -1) {
            fprintf(stderr, "Failed to fetch indirect zone!\n");
            return -1;
        }
        /* Iterate over those the same way we do for direct */
        for (direct_i = 0;
            direct_i < num_zones;
                direct_i++) {
            zones[j] = indirect[direct_i];
            if (indirect[direct_i] != 0) {
                if (arguments->verbose_mode > OFF) {
                    printf("found %d, indirect\n", indirect[direct_i]);
                }
                if (indirect_first_i == EMPTY_NUM) {
                    indirect_first_i = direct_i;
                }
                non_empty[k] = indirect[direct_i];
                indirect_last_i = direct_i;
                k++;
            }
            j++;
        }
    }

    /* Double indirect zones */
    if (inode->two_indirect != 0) {
        /* Reinitialize others for correct calculation */
        direct_last_i = DIRECT_ZONES-1;
        indirect_first_i = 0;
        indirect_last_i = num_zones-1;
        /* Fetch d-indirect zone content */
        er = fetch_zone_content(inode->two_indirect,
            indirect, sizeof(uint32_t), num_zones);
        if (er == -1) {
            fprintf(stderr, "Failed to fetch double indirect zone!\n");
            return -1;
        }
        double_indirect = (uint32_t*)malloc(superblock->blocksize);
        /* Iterate over those by fetching their directory */
        for (indirect_i = 0;
            indirect_i < num_zones;
                indirect_i++) {
            /* If the zone is not empty, do what we did for indirect zones */
            if (indirect[indirect_i] != 0) {
                er = fetch_zone_content(indirect[indirect_i],
                    double_indirect, sizeof(uint32_t),
                        num_zones);
                if (er == -1) {
                    fprintf(stderr, "Failed to fetch from d-indirect zone!\n");
                    return -1;
                }
                for (direct_i = 0;
                    direct_i < num_zones;
                        direct_i++) {
                    zones[j] = double_indirect[direct_i];
                    if (double_indirect[direct_i] != 0) {
                        if (arguments->verbose_mode > OFF) {
                            printf("found %d, dindirect\n",
                                double_indirect[direct_i]);
                        }
                        if (d_indirect_first_i == EMPTY_NUM) {
                            d_indirect_first_i = direct_i;
                        }
                        non_empty[k] = double_indirect[direct_i];
                        d_indirect_last_i = direct_i;
                        k++;
                    }
                    j++;
                }
            }
        }
    }

    /* Change for calculation */
    direct_first_i = direct_first_i == EMPTY_NUM ? 0 : direct_first_i;
    indirect_first_i = indirect_first_i == EMPTY_NUM ? 0 : indirect_first_i;
    d_indirect_first_i = d_indirect_first_i == EMPTY_NUM ?
        0 : d_indirect_first_i;
    
    /* The glorious calculation itself */
    *total = (direct_last_i - direct_first_i + 1) +
        (indirect_last_i - indirect_first_i + 1) +
        (d_indirect_last_i - d_indirect_first_i + 1);

    /* Verbose logging */
    if (arguments->verbose_mode > OFF) {
        printf("total zones: %d\n", *total);
        printf("j logged %d\n", j);
        printf("k logged %d\n", k);
        printf("direct first %d\n", direct_first_i);
        printf("direct last %d\n", direct_last_i);
        printf("indirect first %d\n", indirect_first_i);
        printf("indirect last %d\n", indirect_last_i);
        printf("d indirect first %d\n", d_indirect_first_i);
        printf("d indirect last %d\n", d_indirect_last_i);
    }

    /* Malloc free stuff*/
    if (inode->indirect != 0) {
        free(indirect);
    }
    if (inode->two_indirect != 0) {
        free(double_indirect);
    }
    /* Return the amount of zones we found */
    return k;
}

/* fs_proceed
 * A loop function to iterate through a directory.
 * Inputs: compare_name - the name to look for. only applies
 *              if print_inode_information_bool is set ON
 *         print_inode_information_bool - set to ON to print inode
 *              information for a directory. set to OFF to use compare_name
 *              and find a given directory.
 * Output: int - function return code
 */
int fs_proceed(char* compare_name, int print_inode_information_bool) {
    d_entry* directory;
    d_entry* cur;
    int zones_num;
    int er;
    int pathsize;
    int* zone_buffer;
    int* all_zones;
    int* non_empty_int;
    char* newpath;
    int i;
    int j;

    /* Check if the inode is a directory or not */
    if ((cur_inode->mode & DIRECTORY_OCTAL) != DIRECTORY_OCTAL) {
        fprintf(stderr, "This file is not a directory!");
        return -1;
    }

    /* Verbose logging */
    if (arguments->verbose_mode == ON) {
        printf("current inode mode: %d\n", cur_inode->mode);
        printf("current inode links: %d\n", cur_inode->links);
        printf("current inode uid: %d\n", cur_inode->uid);
        printf("current inode gid: %d\n", cur_inode->gid);
        printf("current inode size: %d\n", cur_inode->size);
        printf("current inode atime: %d\n", cur_inode->atime);
        printf("current inode mtime: %d\n", cur_inode->mtime);
        printf("current inode ctime: %d\n", cur_inode->ctime);
        printf("current inode zone 0: %d\n", cur_inode->zone[0]);
        printf("current inode indirect: %d\n", cur_inode->indirect);
        printf("current inode two indirect: %d\n", cur_inode->two_indirect);
        printf("\n");
        printf("zone size: %d\n", zonesize);
        printf("\n");
    }

    /* Initialize and get zones we'll need to look through */
    all_zones = (int*)malloc(sizeof(int)*num_zones_total);
    non_empty_int = (int*)malloc(sizeof(int));
    directory = (d_entry*)malloc(zonesize);
    zone_buffer = (int*)malloc(num_zones_total * sizeof(int));
    zones_num = gather_zones(cur_inode, zone_buffer, all_zones, non_empty_int);
    free(all_zones);
    free(non_empty_int);
    /* We do not need to count the empty inodes here, so
     *      we don't do anything with that variable */
    if (zones_num <= 0) {
        fprintf(stderr, "Could not find inode directory!\n");
        return -1;
    }

    /* Iterate through each zone to construct the directory contents */
    for (i = 0; i < zones_num; i++) {
        /* Zone 0 is not valid, skip*/
        if (zone_buffer[i] == 0) {
            continue;
        }

        /* Verbose logging*/
        if (arguments->verbose_mode == ON) {
            printf("Trying to get zone %d\n", zone_buffer[i]);
        }
        /* Fetch the content in the zone */
        er = fetch_zone_content(zone_buffer[i],
            directory, sizeof(d_entry), dirs_in_zone);
        if (er == -1) {
            fprintf(stderr, "Could not fetch directory contents!\n");
            return -1;
        }

        /* Go through the directory entries */
        for (j = 0; j < dirs_in_zone; j++) {
            /* Load directory */
            cur = &directory[j];
            er = dir_loop(cur, compare_name,
                print_inode_information_bool);
            switch (er) {
                /* Comparison failed */
                case -1:
                    fprintf(stderr, "Directory compare failed!\n");
                    /* Free stuff we mallocd before exiting */
                    free(zone_buffer);
                    free(directory);
                    return -1;
                /* Print failed */
                case -2:
                    fprintf(stderr, "Directory print failed!\n");
                    /* Free stuff we mallocd before exiting */
                    free(zone_buffer);
                    free(directory);
                    return -1;
                /* Match was found */
                case 1:
                    /* Find the pathsize of the new path */
                    pathsize=strlen(pathname)+strlen(compare_name)+2;
                    if (arguments->verbose_mode == ON) {
                        printf("path: %s\n", pathname);
                        printf("size: %lu\n", strlen(pathname));
                        printf("found: %s\n", compare_name);
                        printf("size: %lu\n", strlen(compare_name));
                        printf("new pathsize: %d\n", pathsize);
                    }
                    /* Malloc and initialize it */
                    newpath = (char*)malloc(pathsize*sizeof(char));
                    newpath[0] = '\0';
                    /* Concatenate what we had before */
                    strcat(newpath, pathname);
                    if (strcmp(pathname, "/") != 0) {
                        strcat(newpath, "/");
                    }
                    if (arguments->verbose_mode == ON) {
                        printf("path now: %s\n", newpath);
                        printf("size: %lu\n", strlen(newpath));
                    }
                    /* Concatenate what we found */
                    strcat(newpath, compare_name);
                    /* Move the path over */
                    free(pathname);
                    pathname = strdup(newpath);
                    if (arguments->verbose_mode == ON) {
                        printf("final path: %s\n", pathname);
                        printf("size of the new path: %lu\n", strlen(newpath));
                        printf("size of path now: %lu\n", strlen(pathname));
                        printf("\n");
                    }
                    /* Freeing stuff we mallocd before exiting*/
                    free(newpath);
                    free(zone_buffer);
                    free(directory);
                    return 0;
                /* Print completed */
                case 2:
                    break;
            }
        }
    }

    /* If we get here and we're comparing, we didn't find a directory */
    if (print_inode_information_bool == OFF) {
        fprintf(stderr, "Could not find the directory!\n");
        return -1;
    }

    /* Free what we malloc'd before */
    free(zone_buffer);
    free(directory);
    return 0;
}

/* display_minls
 * Display information function for minls.
 * Input: none
 * Output: int - function return code
 */
int display_minls() {
    int er;
    if (pathname == NULL) {
        pathname = "/";
    }
    /* Check if inode is a file or directory for proper printing */
    if ((cur_inode->mode & DIRECTORY_OCTAL) != DIRECTORY_OCTAL) {
        /* Print out the permissions and name of the file */
        inode_print(cur_inode, OFF);
    } else {
        /* Use the directory loop to print out the
         * information of the files in it */
        printf("%s:\n", pathname);
        er = fs_proceed(NULL, ON);
        if (er == -1) {
            fprintf(stderr, "Directory loop print failed!\n");
            return -1;
        }
    }
    return 0;
}

/* display_minget
 * Print inode contents into the destination
 * Inputs: none
 * Output: int - function return code
 */
int display_minget() {
    int zones_write; /* Number of zones to write */
    int bytes_write; /* Number of bytes to write */
    int* zones_buffer; /* Buffer for full zones */
    int zone_count; /* Number of full zones we got */
    char* buffer; /* Buffer for writing */
    int output; /* File descriptor for output */
    int write_bool; /* Bool for writing */
    int* zones_all; /* Buffer for all zones */
    int zone_count_all; /* Number of total zones we found */
    int er;

    /* Check if the inode is file or directory */
    if ((cur_inode->mode & DIRECTORY_OCTAL) == DIRECTORY_OCTAL) {
        /* If the inode is a directory, we cannot print its data with minget */
        fprintf(stderr, "This inode is a directory, get failed!\n");
        return -1;
    }
    /* If the file is not of the correct file type (symlink), bail */
    if (!FILE_IS_REG(cur_inode->mode)) {
        fprintf(stderr, "This inode is of the incorrect type!\n");
        return -1;
    }

    /* If we have a file, we send its data to our print destination */
    if (strcmp(arguments->dstpath, EMPTY_STR) == 0) {
        /* Use stdout if no path was inputted */
        output = STDOUT_FILENO;
    }
    else {
        /* Open our file */
        output = open(arguments->dstpath, O_WRONLY | O_CREAT | O_TRUNC);
        if (output == -1) {
            fprintf(stderr, "Could not open destination file for writing\n!");
            return -1;
        }
    }

    /* Verbose logging */
    if (arguments->verbose_mode == ON) {
        printf("File descriptor for output: %d\n\n", output);
        printf("inode size: %d\n", cur_inode->size);
    }

    /* If the inode's size is 0 we can just exit */
    if (cur_inode->size == 0) {
        er = close(output);
        if (er == -1) {
            fprintf(stderr, "Error closing file!\n");
            return -1;
        }
        return 0;
    }

    /* Get our zones to write */
    zones_all = (int*)malloc(sizeof(int)*num_zones_total);
    zones_buffer = (int*)malloc(num_zones_total * sizeof(int));
    zone_count = gather_zones(cur_inode, zones_buffer,
        zones_all, &zone_count_all);
    if (zone_count <= 0) {
        fprintf(stderr, "Could not gather zones to print!\n");
        free(zones_all);
        free(zones_buffer);
        return -1;
    }

    /* Find out how much to write */
    zones_write = zone_count_all - 1;
    bytes_write = cur_inode->size - (zones_write*zonesize);
    
    /* Verbose logging */
    if (arguments->verbose_mode == ON) {
        printf("\n");
        printf("writing to fd: %d\n", output);
        printf("total size: %d\n", cur_inode->size);
        printf("zones to write: %d\n", zones_write);
        printf("bytes writing in zones: %d\n",
            zones_write*zonesize);
        printf("bytes to write: %d\n", bytes_write);
        printf("zones found: %d\n", zone_count_all);
        printf("zonesize: %d\n", zonesize);
    }

    /* Check for appropriate write size */
    if (bytes_write > zonesize) {
        fprintf(stderr, "Huge bytesize remaining!\n");
        free(zones_all);
        free(zones_buffer);
        return -1;
    }

    /* Write correct amount while looking through zones */
    write_bool = ON;
    buffer = (void*)malloc(zonesize);
    while (write_bool == ON) {
        /* Verbose logging */
        if (arguments->verbose_mode == ON) {
            printf("\nWe have %d zones and %d bytes to write!\n",
                zones_write, bytes_write);
        }
        
        if (zones_write == 0) {
            /* Get our information from the zone */
            if (arguments->verbose_mode == ON) {
                printf("zone num %d\n", zones_buffer[zone_count_all-1]);
            }
            /* If the zone is empty, just set the memory to 0 */
            if (zones_buffer[zone_count_all-1] == 0) {
                memset(buffer, 0, bytes_write);
            }
            /* Otherwise fetch the zone */
            else {
                er = fetch_zone_content(zones_buffer[zone_count_all-1],
                    buffer, 1, bytes_write);
                if (er == -1) {
                    fprintf(stderr, "Failed to get zone content!\n");
                    free(zones_all);
                    free(zones_buffer);
                    free(buffer);
                    return -1;
                }
            }
            
            /* Write our number of bytes to the file */
            er = write(output, buffer, bytes_write);
            if (arguments->verbose_mode == ON) {
                printf("bytes written: %d\n", er);
            }
            /* If we wrote less than expected we bail */
            if (er < bytes_write) {
                fprintf(stderr, "Full amount of bytes were not written!\n");
                free(zones_all);
                free(zones_buffer);
                free(buffer);
                return -1;
            }

            /* End the while loop */
            if (arguments->verbose_mode == ON) {
                printf("Written!\n");
            }
            write_bool = OFF;
        }
        else {
            /* Get our information from the zone */
            if (arguments->verbose_mode == ON) {
                printf("index: %d\n", zone_count_all-zones_write-1);
            }
            /* 0 out the buffer if zone is 0 */
            if (zones_buffer[zone_count_all-zones_write-1] == 0) {
                memset(buffer, 0, zonesize);
            }
            /* Get zone content */
            else {
                er = fetch_zone_content(
                    zones_buffer[zone_count_all-zones_write-1],
                    buffer, 1, zonesize);
                if (er == -1) {
                    fprintf(stderr, "Failed to get zone content!\n");
                    free(zones_all);
                    free(zones_buffer);
                    free(buffer);
                    return -1;
                }
            }
            
            /* Write our number of bytes to the file */
            er = write(output, buffer, zonesize);
            if (er < zonesize) {
                fprintf(stderr, "Full bytes here were not written!\n");
                free(zones_all);
                free(zones_buffer);
                free(buffer);
                return -1;
            }
            /* One less zone to write */
            zones_write--;
        }
    }
    
    /* Close our file */
    if (!(output == STDOUT_FILENO)) {
        er = close(output);
        if (er == -1) {
            fprintf(stderr, "Error closing file!\n");
            free(zones_all);
            free(zones_buffer);
            free(buffer);
            return -1;
        }
    }
    /* Free malloc'd stuff */
    free(zones_all);
    free(zones_buffer);
    free(buffer);
    return 0;
}

/* exit
 * Frees malloc'd structs
 * Inputs: none
 * Output: int - function return code
 */
int malloc_exit() {
    int er;
    er = fclose(image);
    if (er == -1) {
        fprintf(stderr, "Could not close image file!\n");
    }
    free(pathname);
    free(root_inode);
    free(superblock);
    free(partition_table);
    free(subpartition_table);
    free(arguments->imagefile);
    if (arguments->chosen_function == MINLS
        && strcmp(arguments->path, EMPTY_STR) != 0) {
        free(arguments->path);
    }
    if (arguments->chosen_function == MINGET) {
        free(arguments->srcpath);
        if (strcmp(arguments->dstpath, EMPTY_STR) != 0) {
            free(arguments->dstpath);
        }
    }
    free(arguments);
    return 0;
}