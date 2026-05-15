/* Welcome to min.h!
This header file defines the many Minix and custom constants used to parse
the filesystem.
Dependencies: standalone
*/

#ifndef MINH
#define MINH
#include <stdint.h>

/* Definitions for argument sorting */
#ifndef MIN_ARGS
#define MIN_ARGS

#define MINLS 0
#define MINGET 1
#define EXTRA_ON 2
#define ON 1
#define OFF 0
#define ON_MORE 2
#define EMPTY_NUM -1
#define EMPTY_STR ""

#endif

/* Main arguments struct */
typedef struct given_args {
    int verbose_mode;
    int chosen_function;

    int partition;
    int subpartition;

    char* imagefile;
    char* path;
    char* srcpath;
    char* dstpath;
} args_t;

/* Minix filesystem constants */
#ifndef MIN_CONST
#define MIN_CONST

#define PARTITION_TABLE_LOCATION 0x1BE
#define PARTITION_TYPE_MINIX_FLAG 0x81
#define BYTE_FIVE_TEN_VPT_MINIX 0x55
#define BYTE_FIVE_ELEVEN_VPT_MINIX 0xAA
#define MINIX_MAGIC_NUMBER 0x4D5A
#define MINIX_MAGIC_NUMBER_REV 0x5A4D
#define INODE_SIZE_BYTES 64
#define DIRECTORY_SIZE_BYTES 64

#define BYTE_CHECK_NUM 2
#define BYTE_CHECK_POS 510
#define SECTOR_SIZE 512
#define NUM_PARTITIONS 4

#define SUPERBLOCK_OFFSET 1024

/* Permission octals */
#define FILE_ALL_OCTAL 0170000
/* #define FILE_IS_LINK_OCTAL 0120000 */
#define FILE_IS_REG_OCTAL 0100000
#define DIRECTORY_OCTAL 0040000
#define OWNER_READ_OCTAL 0000400
#define OWNER_WRITE_OCTAL 0000200
#define OWNER_EXECUTE_OCTAL 0000100
#define GROUP_READ_OCTAL 0000040
#define GROUP_WRITE_OCTAL 0000020
#define GROUP_EXECUTE_OCTAL 0000010
#define OTHER_READ_OCTAL 0000004
#define OTHER_WRITE_OCTAL 0000002
#define OTHER_EXECUTE_OCTAL 0000001

#define FILE_IS_REG(m) (((m) & FILE_ALL_OCTAL) == FILE_IS_REG_OCTAL)

#endif

/* Partition table structure */
typedef struct __attribute__ ((__packed__)) partition_table {
    /* Located at PARTITION_TABLE_LOCATION */
    uint8_t bootind;
    uint8_t start_head;
    uint8_t start_sec;
    uint8_t start_cyl;
    uint8_t type; /* Place to check for filesystem type */
    uint8_t end_head;
    uint8_t end_sec;
    uint8_t end_cyl;
    uint32_t lfirst; /* First sector w/ LBA addressing */
    uint32_t size; /* Size of the partition in sectors */
} ptable_t;

/* Superblock */
typedef struct __attribute__ ((__packed__)) superblock {
    uint32_t ninodes;
    uint16_t pad1;
    int16_t i_blocks;
    int16_t z_blocks;
    uint16_t firstdata;
    int16_t log_zone_size;
    int16_t pad2;
    uint32_t max_file;
    uint32_t zones;
    int16_t magic;
    int16_t pad3;
    uint16_t blocksize;
    uint8_t subversion;
} sb_t;

/* Number of direct zones in Minix */
#ifndef DIRECT_ZONES
#define DIRECT_ZONES 7
#endif

/* inodes */
typedef struct __attribute__ ((__packed__)) inodes {
    uint16_t mode;
    uint16_t links;
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
} in_t;

/* Directory size */
#ifndef DIR_SIZE
#define DIR_SIZE 60
#endif

/* Directory entry */
typedef struct __attribute__ ((__packed__)) directory {
    uint32_t inode;
    unsigned char name[DIR_SIZE];
} d_entry;

/* min_parse functions */
extern int parse_args(int, char**, args_t*);

/* min_filesystem functions */
extern void set_arguments(args_t*);
extern int check_image();

extern int fs_proceed(char*, int);
extern int display_minls();
extern int display_minget();

extern int malloc_exit(void);

/* min_common functions */
extern int setup(int, char**);

#endif