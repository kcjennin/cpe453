/*
* lwp.c - Main source file for the lightweight process library
* Author: Kyle Jennings
*/

#include <lwp.h>
#include <smartalloc.h>

tid_t lwp_create(lwpfun function, void *argument) {
    /* Creates a new lightweight process which executes the given function
    with the given argument.
    lwp_create() returns the (lightweight) thread id of the new thread
    or NO_THREAD if the thread cannot be created. */
}

void lwp_start(void) {
    /* Starts the LWP system. Converts the calling thread into a LWP
    and lwp_yield()s to whichever thread the scheduler chooses. */

}

void lwp_yield(void) {
    /* Yields control to another LWP. Which one depends on the scheduler.
    Saves the current LWP’s context, picks the next one, restores
    that thread’s context, and returns. If there is no next thread,
    terminates the program */
}

void lwp_exit(int) {
    /* Terminates the current LWP and yields to whichever thread the
    scheduler chooses. lwp_exit() does not return. */
}

tid_t lwp_wait(int *status) {
    /* Waits for a thread to terminate, deallocates its
    resources, and reports its termination status if status is non-NULL.
    Returns the tid of the terminated thread or NO_THREAD */
}

tid_t lwp_gettid(void) {
    /* Returns the tid of the calling LWP or NO_THREAD if not called by
    a LWP. */
}

thread tid2thread(tid_t tid) {
    /* Returns the thread corresponding to the given thread ID, or NULL
    if the ID is invalid */
}

void lwp_set_scheduler(scheduler) {
    /* Causes the LWP package to use the given scheduler to choose the
    next process to run. Transfers all threads from the old scheduler
    to the new one in next() order. If scheduler is NULL the library
    should return to round-robin scheduling. */
}

scheduler lwp_get_scheduler(void) {
    /* Returns the pointer to the current scheduler. */
}
