/*
* lwp.c - Main source file for the lightweight process library
* Author: Kyle Jennings
*/

#include "lwp.h"
#include <stdlib.h>
#include "smartalloc.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static void lwp_wrap(lwpfun f, void *arg);
static void r_admit(thread new);
static void r_remove(thread victim);
static thread r_next(void);

static struct scheduler publish = {NULL, NULL, r_admit, r_remove, r_next};

static int counter = 1;
static scheduler ActiveScheduler = &publish;
static thread ActiveThread = NULL;

static thread lib_tlist;
static thread zombies;

tid_t lwp_create(lwpfun f, void *arg, size_t len)
{
    /* Creates a new lightweight process which executes the given function
    with the given argument.
    lwp_create() returns the (lightweight) thread id of the new thread
    or NO_THREAD if the thread cannot be created. */

    thread new_thread;
    
    long pagesize;
    struct rlimit rlim;

    unsigned long *stack_top;

    int i;

    if(posix_memalign((void **) &new_thread, 16, sizeof(context)) != 0)
    {
        perror("posix malloc");
        exit(1);
    }
    memset(new_thread, 0, sizeof(context));
    
    /* setup the FP register */
    new_thread->state.fxsave = FPU_INIT;

    /* set the tid */
    new_thread->tid = counter++;

    /* get the stack/stacksize */
    /* get the page size */
    pagesize = sysconf(_SC_PAGE_SIZE);

    /* get the max stack size */
    if(getrlimit(RLIMIT_STACK, &rlim) < 0)
    {
        /* failed to get stack size */
        return NO_THREAD;
    }
    if(rlim.rlim_cur == RLIM_INFINITY)
    {
        /* No limit, so we set it to 8MB */
        new_thread->stacksize = (2 << 23);
    }
    else
    {
        /* There is a limit, round it up to the nearest page */
        if (rlim.rlim_cur % pagesize > 0)
            new_thread->stacksize = rlim.rlim_cur + (pagesize - (rlim.rlim_cur % pagesize));
        else
            new_thread->stacksize = rlim.rlim_cur;
    }

    /* allocate the stack */
    new_thread->stack = (unsigned long *) mmap(NULL, new_thread->stacksize, 
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if(new_thread->stack < 0)
    {
        perror("mmap");
        return NO_THREAD;
    }
    
    /* add the function from the signature */
    new_thread->state.rdi = (unsigned long) f;

    /* add the argument from the signature */
    new_thread->state.rsi = (unsigned long) arg;

    /* save the top of the stack locally */
    stack_top = (unsigned long *) (((unsigned long) new_thread->stack) + new_thread->stacksize - 1);
    
    /* NOTE: Check that stack_top is divisible by sixteen */
    if( (i = ((unsigned long) stack_top) % 16) > 0)
        stack_top = (unsigned long *) (((unsigned long) stack_top) - i);
    
    /* save the top of the stack to rbp for later */
    new_thread->state.rbp = (unsigned long) stack_top;

    /* push the exit address to the stack (lwp_exit) and move the sp */
    *stack_top = (unsigned long) lwp_exit;
    stack_top -= 1;

    /* push the starting function to the stack and move sp */
    *stack_top = (unsigned long) lwp_wrap;
    stack_top -= 1;

    /* push rbp to the stack */
    *stack_top = new_thread->state.rbp;

    /* change rbp to point below the stack frame just created */
    new_thread->state.rbp = (unsigned long) stack_top;

    /* add the thread to the scheduler */
    ActiveScheduler->admit(new_thread);

    if(!lib_tlist)
        lib_tlist = new_thread;
    
    return new_thread->tid;
}

void lwp_exit(int status)
{
    /* Terminates the current LWP and yields to whichever thread the
    scheduler chooses. lwp_exit() does not return. */

    ActiveScheduler->remove(ActiveThread);
    ActiveThread->status = status;

    if(zombies)
    {
        ActiveThread->lib_one = zombies;
        zombies = ActiveThread;
    }
    else
    {
        zombies = ActiveThread;
        ActiveThread->lib_one = NULL;
    }

    lwp_yield();
}

static void lwp_wrap(lwpfun f, void *arg)
{
    /* Call the given lwpfunction with the given argument.
    Calls lwp exit() with its return value */
    int rval;
    
    rval = f(arg);
    lwp_exit(rval);
}

tid_t lwp_gettid(void)
{
    /* Returns the tid of the calling LWP or NO_THREAD if not called by
    a LWP. */
    if(ActiveThread)
    {
        return ActiveThread->tid;
    }
    return NO_THREAD;
}

void lwp_yield_helper(rfile *old, rfile *new)
{
    swap_rfiles(old, new);
}

void lwp_yield(void)
{
    /* Yields control to another LWP. Which one depends on the scheduler.
    Saves the current LWP’s context, picks the next one, restores
    that thread’s context, and returns. If there is no next thread,
    terminates the program */

    thread prev_thread = ActiveThread;
    ActiveThread = ActiveScheduler->next();

    if(ActiveThread)
        lwp_yield_helper(&(prev_thread->state), &(ActiveThread->state));
    else
        exit(prev_thread->status);
}

void lwp_start(void)
{
    /* Starts the LWP system. Converts the calling thread into a LWP
    and lwp_yield()s to whichever thread the scheduler chooses. */

    thread new_thread;

    if(posix_memalign((void **) &new_thread, 16, sizeof(context)) != 0)
    {
        perror("posix malloc");
        exit(1);
    }
    memset(new_thread, 0, sizeof(context));

    /* set the tid */
    new_thread->tid = counter++;

    /* mark the stack so we don't free it */
    new_thread->stack = NULL;
    new_thread->stacksize = 0;

    new_thread->state.fxsave = FPU_INIT;

    ActiveScheduler->admit(new_thread);

    ActiveThread = new_thread;

    /* throw yourself upon the mercy of the almighty scheduler */
    lwp_yield();
}

tid_t lwp_wait(int *status)
{
    /* Waits for a thread to terminate, deallocates its
    resources, and reports its termination status if status is non-NULL.
    Returns the tid of the terminated thread or NO_THREAD */

    thread zombie;
    tid_t tid;
    
    /* wait for a thead to die */
    while(!zombies)
        lwp_yield();

    zombie = zombies;
    zombies = zombies->lib_one;

    tid = zombie->tid;

    if(zombie->status)
        *status = zombie->status;

    munmap(zombie->stack, zombie->stacksize);
    free(zombie);

    return tid;
}

void lwp_set_scheduler(scheduler sched)
{
    /* Causes the LWP package to use the given scheduler to choose the
    next process to run. Transfers all threads from the old scheduler
    to the new one in next() order. If scheduler is NULL the library
    should return to round-robin scheduling. */

    if(sched)
    {
        thread l;

        while( (l = ActiveScheduler->next()) != NO_THREAD)
        {
            ActiveScheduler->remove(l);
            sched->admit(l);
        }
        ActiveScheduler = sched;
    }

}

scheduler lwp_get_scheduler(void)
{
    /* Returns the pointer to the current scheduler. */
    return ActiveScheduler;
}

thread tid2thread(tid_t tid)
{
    /* Returns the thread corresponding to the given thread ID, or NULL
    if the ID is invalid */

    context dummy;
    thread l;

    dummy.lib_one = lib_tlist;

    for (l = &dummy; l->lib_one && l->lib_one->tid != tid; l = l->lib_one);

    if(l->lib_one)
        return l->lib_one;
    else
        return NULL;
}

/******************************************************************************/
/* Default Scheduler Definition */

static thread qhead = NULL;
#define tlist_next lib_one
#define tlist_prev lib_two
#define tnext sched_one
#define tprev sched_two
static thread lwp_tlist;

/**
 * @brief add a thread to the thread list
 * 
 * @param new the thread to add
 */
static void r_admit(thread new)
{
    /* add to queue */
    if (qhead)
    {
        new->tnext = qhead;
        new->tprev = qhead->tprev;
        new->tprev->tnext = new;
        qhead->tprev = new;
    }
    else
    {
        qhead = new;
        qhead->tnext = new;
        qhead->tprev = new;
    }

    /* add to global thread list */
    new->tlist_next = lwp_tlist;
    lwp_tlist = new;
}

static void r_remove(thread victim)
{
    context dummy;
    thread l;

    dummy.tlist_next = lwp_tlist;

    for (l = &dummy; l->tlist_next && l->tlist_next != victim; l = l->tlist_next)
        /* dum dee dum */;

    /* if we found it in tlist */
    if (l->tlist_next)
    {
        /* cut out of lwp_tlist */
        l->tlist_next = l->tlist_next->tlist_next;
        lwp_tlist = dummy.tlist_next;

        /* cut out of queue */
        victim->tprev->tnext = victim->tnext;
        victim->tnext->tprev = victim->tprev;
        /* what if it were qhead? */
        if (victim == qhead)
        {
            if (victim->tnext != victim)
                qhead = victim->tnext;
            else
                qhead = NULL;
        }
    }
}

static thread r_next(void)
{
    thread res;

    if (qhead)
    {
        res = qhead;
        qhead = qhead->tnext;
    }
    else
        res = NO_THREAD;

    return res;
}
