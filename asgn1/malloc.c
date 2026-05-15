#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

/*
Welcome to malloc.c! ... or whatever exists of it..
Function: replaces the malloc, calloc, realloc, and
    free functions of the standard malloc library of functions.
Dependencies: standalone
*/

/* Global Variables */
struct Header *heapStart = NULL; // Pointer to the start of the heap
int NEW_BLOCK_SIZE = 64000; // 64kb default
int MALLOC_BLOCK_MULTIPLE = 16; // Break malloc blocks in multiples of 16

/* Data Section Header Definition */
struct Header {
    void *data; // Pointer to the beginning of data section
    int size; // Size of data section in bytes
    bool free;  // Indicates whether the data section is free
    void *next; // Pointer to the next header or void
};


/*~~~~~~~~~~~~~ SYSTEM/ INTERNAL FUNCTIONS ~~~~~~~~~~~~~*/
/*
setupBlock
Function: sets up a new data block with a proper header.
Inputs:
    start: a pointer pointing to the desired start of the block
    ntPointer: a pointer pointing to the next block (or address) to point to

Outputs:
    a pointer to a data header that describes
        the block and lists it as ready for use.
*/
static struct Header *setupBlock(void *start, void *ntPointer) {
    // Create block and put it in the address at the desired start
    struct Header *block;
    block = &start;
    
    // Calculations for block data pointer and size
    int *p = (void *)((intptr_t)start + sizeof(struct Header));
    int s = (intptr_t)start - (intptr_t)ntPointer - sizeof(struct Header);
    // Assignment of values
    block->data = &p;
    block->size = s;
    block->free = true; // We want the block to be usable
    block->next = &ntPointer;

    return block;
    
}

/*
mergeBlocks
Function: merges consecutive empty blocks
Inputs:
    none

Outputs:
    none
*/
static void mergeBlocks() {
    // Make variables for our while loop
    struct Header *current = &heapStart;
    struct Header *nt = NULL;

    // Go through the linked list of headers to find consecutive free blocks
    while (sizeof(current) == sizeof(struct Header)) {
        nt = current->next;
        // End of list
        if (sizeof(nt) != sizeof(struct Header)) {
            // Exit from while loop + function
            break;
        }
        // Blocks can be merged
        else if (current->free == true && nt->free == true) {
            setupBlock(current, nt->next);
        }
        // Move on
        current = nt;
    }
}

/*
freeBrk
Function: when called, frees up any remaining empty
    blocks at the end of the heap as memory to the OS
Inputs:
    none

Outputs:
    none
*/
static void freeBrk() {
    // Make variables for while loop
    struct Header *cur = &heapStart;

    // While loop - find the last block allocated
        // If free, this is the only block that needs to be given up
        // because of the existence of our merge function that runs
        // after each new block is created
    while (sizeof(cur) == sizeof(struct Header)) {
        // End of linked list
        if (sizeof(cur->next) != sizeof(struct Header)) {
            break;
        }
        // Move on
        cur = cur->next;
    }

    // If the block is free, move the breakline to its start
        // (giving it back to the OS)
    if (cur->free == true) {
        brk(cur);
    };
}

/*
findMyHeader
Function: finds the header of a given memory address.
Inputs:
    pointer: a pointer pointing to an address in memory the user references

Outputs:
    a pointer to a data header that contains the given address in its data
*/
static struct Header *findMyHeader(void *pointer) {
    // If we did not record the start of the heap,
    // there has not been memory allocated yet
    if (heapStart == NULL) {
        perror("You haven't malloc'd anything yet!");
        return NULL;
    }

    // If the beginning of the heap is not a header,
    // there are no available blocks to search
    if (sizeof(*heapStart) != sizeof(struct Header)) {
        perror("There's no malloc'd blocks...");
        return NULL;
    }

    // Variable set up for our while loop to look
    // through the linked list
    struct Header *looking = &heapStart;
    struct Header *found = NULL;
    
    // Loop to find correct address range
    while ((intptr_t)looking < (intptr_t)pointer) {
        // End of list, break loop
        if (sizeof(looking) != sizeof(struct Header)) {
            break;
        }
        // Found our header
        else if (looking->next > pointer) {
            found = looking;
            break;
        }
        // Did not find anything significant, move on
        else {
            looking = looking->next;
        }
    }

    // If the loop broke before finding a suitable header
    // we did not find our address in the heap
    if (found == NULL) {
        perror("Could not find your pointer! Is it correct?");
        return NULL;
    }

    // Otherwise return the header we found
    return found;
}

/*
createMoreMemory
Function: uses sbrk to allocate more memory for the heap.
Inputs:
    none

Outputs:
    a pointer to a data header with the new allocated space
*/
static struct Header *createMoreMemory() {
    // Move the break line with sbrk
    void *prevHeapLine = sbrk(NEW_BLOCK_SIZE);
    // Failure error catch
    if (prevHeapLine == ((void *)-1)) {
        perror("createMoreMemory - sbrk failed!");
        return NULL;
    }
    // Setting up variables for block assignment
    void *newLine = sbrk(0);
    struct Header *block;
    block = &prevHeapLine; // Place new block where heap previously ended

    // Debug
    printf("previous line was at %d\nline now at %d\n", prevHeapLine, newLine);

    // Block assignment
    block = setupBlock(prevHeapLine, newLine);

    // Just in case we had a block that was free at the
    // end of the heap that we can merge with this one
    mergeBlocks();
    
    return block;
}

/*~~~~~~~~~~~~~ USER CALLABLE FUNCTIONS ~~~~~~~~~~~~~*/
/*
(v)malloc
Function: allocates a user defined amount of bytes of memory
Inputs:
    bytes: amount of bytes to allocate

Outputs:
    a pointer to the first address of the data region the user can use
*/
void *vmalloc(int bytes) {
    if (getenv("DEBUG_MALLOC")) {
        fprintf(stderr, "MALLOC.C: malloc(%d)", bytes);
    }
    // We need to run this first to keep track of our
    // linked list
    if (heapStart == NULL) {
        printf("~initialized heapstart\n");
        heapStart = sbrk(0);
    }
    // Malloc'ing 0 bytes will return a pointer
    // with nothing in it..
    if (bytes == 0) {
        return NULL;
    }
    // Calculate the proper, divisible by 16 size
    // for our malloc block
    int size = (bytes % MALLOC_BLOCK_MULTIPLE + 1) * MALLOC_BLOCK_MULTIPLE;

    // Setting up variables for the while loop
    struct Header *location = &heapStart;

    // Debugging
    printf("heapstart is at %d\n", heapStart);
    printf("location is %d\n", *location);
    int a = sizeof(location);
    int b = sizeof(struct Header);
    printf("size of looking: %d\nsize of header: %d\n", a, b);

    // Finding an available block to use for our malloc
    while (sizeof(location) == sizeof(struct Header))  {
        // Debug
        printf("~inside malloc while loop\n");

        // If the block is free and of appropriate size
        if (location->free == true && location->size >= size) {
            // Debug
            printf("~found block to malloc\n");

            // If the size is exactly what we want, we don't need
            // further setup or fragmenting
            if (location->size == size) {
                location->free = false;
                return location->data;
            }

            // If the size is larger, we need to split the block
            // and setup the rest as a valid free block
            else {
                // Setup this block + new block variables
                void *newBlockEnd = location->next;
                int n = (intptr_t)location->data + size;
                void *newLocationEnd = (void *)(n);

                // Fix up this block to be used
                location->size = size;
                location->free = false;
                location->next = newLocationEnd;

                // Set up the new block
                setupBlock((void *)((intptr_t)newLocationEnd + 1), newBlockEnd);
                mergeBlocks();

                return location->data;
            }
        }
        // We didn't find a valid block here, move on
        location = location->next;

        // Debug
        printf("~continuing malloc loop\n");
    }

    // Debug
    printf("~creating more memory for malloc\n");

    // We did not find a free block to use, so we
    // need to create more memory for us via our
    // convenient createMoreMemory function
    void *more = createMoreMemory();
    if (more == NULL) {
        perror("Could not create more memory for malloc!");
        return NULL;
    }

    // Run the function again now that we have more memory
    return vmalloc(bytes);
}

/*
(v)calloc
Function: allocates an array in the heap.
Inputs:
    n: number of array items to include
    bytes: size in bytes of each array item

Outputs:
    a pointer to the first byte of usable data
*/
void *vcalloc(int n, int bytes) {
    if (getenv("DEBUG_MALLOC")) {
        fprintf(stderr, "MALLOC.C: calloc(%d, %d)", n, bytes);
    }

    // If any of the values are 0, return a null pointer
    if (n == 0 || bytes == 0) {
        return NULL;
    }

    // Malloc appropriate amount of blocks
    struct Header *block = vmalloc(n * bytes);
    // Set it all to 0
    memset(block->data, 0, block->size);

    return block->data;
}

/*
(v)free
Function: frees a block of previously allocated memory
Inputs:
    pointer: pointer to an address contained in the block to be freed

Outputs:
    none
*/
void vfree(void *pointer) {
    if (getenv("DEBUG_MALLOC")) {
        fprintf(stderr, "MALLOC.C: free(%p)", pointer);
    }

    // Setup a variable for the header. We need to get the
    // header first.
    struct Header *header;
    // ...and my handy function for doing so
    header = findMyHeader(pointer);
    if (header == NULL) {
        perror("Free: Could not find memory allocated for given pointer!");
        return;
    }

    // Set that block to free and merge any neighboring blocks
    header->free = true;
    mergeBlocks();
    // We free any blocks to the OS to save memory
    freeBrk();

    return;
}

/*
(v)realloc
Function: changes the amount of size allocated for a block
Inputs:
    pointer: pointer to an address in the desired block to mutate
    bytes: the size to change the block to

Outputs:
    a pointer to the first byte of usable
        data in the block, allocated to the correct size
*/
void *vrealloc(void *pointer, int bytes) {
    if (getenv("DEBUG_MALLOC")) {
        fprintf(stderr, "MALLOC.C: realloc(%p, %d)", pointer, bytes);
    }

    // If both are NULL/ 0, return NULL
    if (pointer == NULL && bytes == 0) {
        perror("Invalid input!");
        return NULL;
    }

    // If bytes are 0, it is equal to a free
    if (bytes == 0) {
        vfree(pointer);
    }

    // If pointer is NULL, it is equal to a malloc
    if (pointer == NULL) {
        return vmalloc(bytes);
    }

    // This also acts as a "no malloc" error catcher
    // but we find the pointer for the header that was given
    struct Header *heads = findMyHeader(pointer);

    // Find the 16 byte divisible Approved size and
    // use that instead
    int size = (bytes % MALLOC_BLOCK_MULTIPLE + 1) * MALLOC_BLOCK_MULTIPLE;
    
    // If the new size is smaller than the block size
    if (heads->size < size) {
        // Make some block calculations since we will just
        // be adjusting the existing block then setting
        // up the rest
        struct Header *newCurEnd = (void *)((intptr_t)heads->data + size);
        struct Header *newBlockEnd = heads->next;
        // Adjust our block
        heads->size = size;
        heads->next = newBlockEnd;

        // Setup the remainder as a free block
        setupBlock((void *)((intptr_t)newCurEnd + 1), newBlockEnd);
        mergeBlocks();

        return heads->data;
    }

    // If the new size is larger than the block size
    else if (heads->size > size) {
        // We setup variables to look at our nearby block
        struct Header *nex = heads->next;
        bool nexFree = nex->free;
        int nexSize = nex->size;
        int sizeCh = nexSize + heads->size - sizeof(struct Header);

        // If we can expand in place
        if (nexFree == true && sizeCh > size) {
            // Setup our header to now include that block in
            // our data by setting our "next" pointer
            // to the next block's next. Also adjust size to
            // what we need
            heads->size = size;
            heads->next = (void *)((intptr_t)heads->data + size);
            
            // Set up the remainder
            setupBlock((void *)((intptr_t)heads->next + 1), nex->next);

            return heads->data;
        }
        // If we cannot expand in place
        else {
            // Setup to start a while loop to go through
            // the linked list
            struct Header *current = &heapStart;

            // Go through the linked list to look for a suitable block
            while (sizeof(current) == sizeof(struct Header)) {
                // If the block can fit our data
                if (current->free == true && current->size > size) {
                    // Set that block as taken, copy the data to it,
                    // and set the correct size
                    current->free = false;
                    memcpy(&heads->data, &current->data, heads->size);
                    current->size = size;

                    // Find the pointer where the new block starts
                    // and setup the extra block
                    void *bl = (void *)((intptr_t)current->data + size);
                    setupBlock(bl, current->next);
                    current->next = bl;

                    // Free what we used before and merge blocks
                    vfree(heads);
                    mergeBlocks();

                    return current->data;
                }
                // Move on down the list
                current = current->next;
            }
            // If we get here, we did not have enough space
            // to expand in our current heap, so we create
            // more memory and rerun the function
            createMoreMemory();
            return vrealloc(pointer, bytes);
        }
    }

    // The sizes of both are equal
    else {
        perror("The size you entered is already allocated.");
        return heads->data;
    }                
}

/*
main
Function: debugging
Inputs:
    none

Outputs:
    none
*/
int main() {
    printf("Welcome to vic's debugging program!\n");

    // Malloc section
    printf("Running malloc...\n");
    void *a = vmalloc(4);
    printf("Malloc is referring to %d!\n", a);

    // Calloc section
    printf("Running calloc...\n");
    void *b = valloc(5, 2);
    printf("Calloc is referring to %d!\n", b);

    // Realloc section
    printf("Running realloc...\n");
    void *c = vrealloc(a, 15);
    printf("Realloc now refers to %c!\n", c);

    // Free section
    printf("Freeing...\n");
    vfree(b);
    vfree(c);
    printf("Freed! Exiting...\n");
}