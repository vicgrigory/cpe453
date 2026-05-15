#include <lwp.h>
#include <fp.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

/*
LWP.c
Attempts to create a threaded process. Shares stack memory
and moves registers to run threads.
*/

/*
Struct for a circularly linked list for the round robin scheduler.
Also used to go through currently queued threads
*/
struct scheduleList {
    struct scheduleList *previous;
    thread *threadInfo;
    struct scheduleList *next;
};

scheduleList start = NULL; // first thread to orient scheduler
struct scheduleList threadRun = NULL; // circle linked list for threads
scheduleList current; // current thread

// Scheduler functions
/*
Admit a thread into the scheduler
Input: thread to admit
*/
void rr_admit(thread new) {
    if (threadRun == NULL) { // Add first thread
        threadRun->previous = NULL;
        threadRun->threadInfo = *new;
        threadRun->next = NULL;
        start = new;
    }
    else if (scheduleList->next == NULL) { // Add second thread
        struct scheduleList newThread;
        newThread->previous = *threadRun;
        newThread->threadInfo = *new;
        newThread->next = *threadRun;

        threadRun->previous = *newThread;
        threadRun->next = *newThread;
    }
    else { // Modify previous and next to new scheduleList object
        // Find first thread in our list
        struct scheduleList *currentLoop = *threadRun;
        while (currentLoop->threadInfo != start) {
            currentLoop = currentLoop->next;
        }
        struct scheduleList newThread;
        newThread->previous = *threadRun;
        newThread->threadInfo = *new;
        newThread->next = *threadRun;
        // Add new thread to the end
        currentLoop->previous->next = newThread;
        currentLoop->previous = newThread;
    }
}
/*
Remove a thread from the scheduler
Input: thread to remove
*/
void rr_remove(thread victim) {
    // Go through the whole list and remove it
        // this doesnt edit the original list ?
    struct scheduleList *currentLoop = *threadRun;
    if (currentLoop == NULL) {
        perror("No threads present!");
        return;
    }
    while(true) {
        if (currentLoop->threadInfo == victim) {
            if (currentLoop->next == NULL) {
                currentLoop->threadInfo = NULL;
                return;
            }
            else if (currentLoop->next == currentLoop->previous) {
                currentLoop->next = NULL;
                currentLoop->previous = NULL;
                return;
            }
            currentLoop->previous->next = currentLoop->next;
            currentLoop->next->previous = currentLoop->previous;
            return;
        }
        currentLoop = currentLoop->next;
    }
    

}
/*
Returns the next thread the scheduler will run
Output: thread to run
*/
thread rr_next() {
    scheduleList looking;
    while (looking != current) {
        looking = looking->next;
    }
    current = looking->next;
    return current;
}
/*
Returns the number of threads in the scheduler
Output: number of threads
*/
int rr_qlen() {
    struct scheduleList *currentLoop = threadRun->next;
    int i = 0;
    while (currentLoop != current) {
        i = i + 1;
        currentLoop = currentLoop->next;
    }
    return i;
}

scheduler default = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
scheduler *sched = &default; // global scheduler variable

tid_t tid = 0; // tid global counter (to assign)
int threadList = 0; // number of created threads
thread exited = NULL; // recently exited thread for wait
scheduleList allThreads; // all threads created

// LWP functions
/*
Create a lightweight process and add it to the scheduler
Output: tid of the new thread
*/
tid_t lwp_create(lwpfun function, void *argument) {
    // Create our thread context struct
    struct threadinfo_st threadInformation;

    // Find stack size
    struct rlimit *limit;
    getrlimit(RLIMIT_STACK, limit);
    if (limit->rlim_cur == RLIM_INFINITY || limit->rlim_cur == NULL) {
        threadInformation->stacksize = 8000000; // 8MB in bytes
    }
    else {
        threadInformation->stacksize = sysconf(_SC_PAGE_SIZE * limit->rlim_cur);
    }
    
    // Initialize stack for our thread information
    unsigned long *threadS = threadInformation->stack;
    int propsReadType = PROT_READ|PROT_WRITE;
    int propsExistType = MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK;
    threadS = mmap(NULL, threadInformation->stackSize, propsReadType, propsExistType, -1, 0);
    if (threadS == MAP_FAILED) {
        perror("Stack allocated for LWP failed!");
        return NO_THREAD;
    }

    // Create and fill register information
    swap_rfiles(threadInformation->state, NULL);
    threadInformation->state.fxsave = FPU_INIT;
    threadInformation->rbp = (intptr_t)threadS - threadInformation->stackSize;
    threadInformation->rdi = *function;
    /* This is where I'd put the correct arguments
        into the threadInformation variable so
        that swap_rfiles would insert them correctly
        when yield switches to another thread */

    // Generate a TID for this thread
    tid = tid + 1;
    threadInformation->tid = tid;
    threadList = threadList + 1;

    // Add our thread to the scheduler
    sched.admit(threadInformation);
    return threadInformation->tid;
}
/*
Initialize the program by making it a LWP and starting the scheduler
*/
void lwp_start() {
    // Initialize our thread stuff
    struct threadinfo_st threadInformation;
    // Not adding in our stack so we don't deallocate it by accident
    threadInformation->stack = NULL;
    // Add required registers
    swap_rfiles(threadInformation->state, NULL);
    threadInformation->state.fxsave = FPU_INIT;
    threadInformation->tid = lwp_gettid();
    // Admit into scheduler and yield
    sched.admit(threadInformation);
    lwp_yield();
}
/*
Wrapper function for threads
*/
static void lwp_wrap(lwpfun function, void *arg) {
    int rval;
    rval = function(arg);
    lwp_exit(rval);
}
/*
Yield current thread operations to another thread
*/
void lwp_yield() {
    // Save current registers + stack
    struct threadinfo_st threadInformation;
    threadInformation->stacksize = sysconf(_SC_PAGE_SIZE * limit->rlim_cur);
    int propsReadType = PROT_READ|PROT_WRITE;
    int propsExistType = MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK;
    threadInformation->stack = mmap(NULL, threadInformation->stackSize, propsReadType, propsExistType, -1, 0);
    if (threadInformation->state == MAP_FAILED) {
        perror("Stack allocated for LWP failed!");
        return NO_THREAD;
    }

    swap_rfiles(threadInformation->state, NULL);

    // Add our thread to the scheduler and get next thread
    sched.admit(threadInformation);
    thread next = sched->next();
    if (next == NULL) {
        perror("No threads present in the scheduler!");
        exit(0);
    }

    // Put context into registers
    swap_rfiles(NULL, *next->state);
}
/*
Set thread as exited
Input: exit value of the thread function
*/
void lwp_exit(int exitval) {
    // Get lower 8 bits of exit value
    current->status = (int)(exitval & 0xff);
    sched.remove(current);
    exited = current;
    current = NULL;
    lwp_yield();
}
/*
Exit and deallocate thread resources
Input: status of thread
Output: tid of exited thread
*/
tid_t lwp_wait(int *status) {
    // Check if we have an exited thread
    if (exited == NULL) {
        return NO_THREAD;
    }
    // Unmap stack
    int stat = munmap(exited->stack, exited->stacksize);
    if (stat == -1) {
        perror("Error unmapping thread stack!");
        return;
    }
    // Return id and clear exited variable
    exited->status = *status;
    tid_t id = exited->tid;
    exited = NULL;
    return id;
}
/*
Get the tid of the current thread
Output: tid of current thread
*/
tid_t lwp_gettid() {
    if (current == NULL) {
        return NO_THREAD;
    }
    return current->threadInfo->tid;
}
/*
Get the thread based on the tid
Input: tid of desired thread
Output: thread associated with the tid
*/
thread tid2thread(tid_t tid) {
    for (int i = 0; i < threadList; i++) {
        if (allThreads->current->tid == tid) {
            return allThreads->threadInfo;
        }
        allThreads = allThreads->next;
    }
    return NULL;
}
/*
Set the scheduler to a desired one or the default one
Input: desired scheduler, or none if using default
*/
void lwp_set_scheduler(scheduler newSched) {
    if (newSched == NULL) {
        // Use round robin
        scheduler defSched = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
        newSched = defSched;
    }
    // Transfer threads to new scheduler
    thread transfer = sched->next;
    while (transfer != NULL) {
        newSched->admit(transfer);
        sched->remove(transfer);
        transfer = sched.next();
    }
    sched = &newSched;
}
/*
Get scheduler
Output: scheduler variable
*/
scheduler lwp_get_scheduler() {
    return *sched;
}