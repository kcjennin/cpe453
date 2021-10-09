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
void *malloc_16(size_t size);
void free_16(void *ptr);

static struct scheduler publish = {NULL, NULL, r_admit, r_remove, r_next};

static int counter = 1;
static scheduler ActiveScheduler = &publish;
static thread ActiveThread = NULL;

static thread lib_tlist;
static thread zombies;


/**
 * @brief Creates a new lightweight process which executes the given function
 * with the given argument. lwp_create() returns the (lightweight) thread id of
 * the new thread or NO_THREAD if the thread cannot be created.
 * 
 * @param f starting function of the thread
 * @param arg argument for the starting function
 * @param len not used
 * @return tid_t id of the created thread or NULL if there was an error
 */
tid_t lwp_create(lwpfun f, void *arg, size_t len)
{
    thread new_thread;
    long pagesize;
    struct rlimit rlim;
    unsigned long *stack_top;
    int i;

    if( !(new_thread = (thread) malloc_16(sizeof(context))) )
    {
        return NO_THREAD;
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

    /* add the thread to the library list */
    if(!lib_tlist)
        lib_tlist = new_thread;
    else
    {
        new_thread->lib_one = lib_tlist;
        lib_tlist = new_thread;
    }
    
    return new_thread->tid;
}


/**
 * @brief Terminates the current LWP and yields to whichever thread the
 *  scheduler chooses. lwp_exit() does not return.
 * 
 * @param status exit status of the thread
 */
void lwp_exit(int status)
{
    thread l;
    context dummy;

    /* Remove the current process from the scheduler and save the status */
    ActiveScheduler->remove(ActiveThread);
    ActiveThread->status = status;

    /* If there are zombies already, add the thread to the beginning of the
       list */
    if(zombies)
    {
        ActiveThread->lib_one = zombies;
        zombies = ActiveThread;
    }
    /* Otherwise it becomes the beginning */
    else
    {
        zombies = ActiveThread;
        ActiveThread->lib_one = NULL;
    }

    /* update the head of our active threads */
    if(ActiveThread == lib_tlist)
        lib_tlist = lib_tlist->lib_one;
    else
    {
        /* remove it from the library list */
        dummy.lib_one = lib_tlist;
        for(l = &dummy; l->lib_one && l->lib_one != ActiveThread; l = l->lib_one);

        if(l->lib_one)
        {
            l->lib_one = l->lib_one->lib_one;
        }
    }

    /* Let other threads run */
    lwp_yield();
}


/**
 * @brief Call the given lwpfunction with the given argument.
 *  Calls lwp exit() with its return value
 * 
 * @param f main functinon of the thread
 * @param arg argument of the main function
 */
static void lwp_wrap(lwpfun f, void *arg)
{
    int rval;
    
    rval = f(arg);
    lwp_exit(rval);
}

/**
 * @brief Returns the tid of the calling LWP or NO_THREAD if not called by
 *  a LWP.
 * 
 * @return tid_t id of the current thread
 */
tid_t lwp_gettid(void)
{
    if(ActiveThread)
    {
        return ActiveThread->tid;
    }
    return NO_THREAD;
}


/**
 * @brief Calls swap_rfiles without the interference of locals
 * 
 * @param old previous thread registers
 * @param new new thread registers
 * 
 * @return does not return
 */
void lwp_yield_helper(rfile *old, rfile *new)
{
    swap_rfiles(old, new);
}


/**
 * @brief Yields control to another LWP. Which one depends on the scheduler.
 *  Saves the current LWP’s context, picks the next one, restores
 *  that thread’s context, and returns. If there is no next thread,
 *  terminates the program
 * 
 * @return does not return
 */
void lwp_yield(void)
{
    int status;
    
    /* save the old thread and get the next thread */
    thread prev_thread = ActiveThread;
    ActiveThread = ActiveScheduler->next();

    /* If we have threads left, yield to them */
    if(ActiveThread)
        lwp_yield_helper(&(prev_thread->state), &(ActiveThread->state));
    /* Otherwise deallocate the current thread and exit the program. */
    else
    {
        if(prev_thread->stack)
            munmap(prev_thread->stack, prev_thread->stacksize);
        status = prev_thread->status;
        free_16(prev_thread);
        exit(status);
    }
}


/**
 * @brief Starts the LWP system. Converts the calling thread into a LWP
 *  and lwp_yield()s to whichever thread the scheduler chooses.
 * 
 */
void lwp_start(void)
{
    thread new_thread;

    /* create a 16-byte aligned thread */
    new_thread = (thread) malloc_16(sizeof(context));
    memset(new_thread, 0, sizeof(context));

    /* set the tid */
    new_thread->tid = counter++;

    /* mark the stack so we don't free it */
    new_thread->stack = NULL;
    new_thread->stacksize = 0;

    /* Admit the new thread and set is as active */
    ActiveScheduler->admit(new_thread);
    ActiveThread = new_thread;

    /* add the thread to the library list */
    if(!lib_tlist)
        new_thread = lib_tlist;
    else
    {
        new_thread->lib_one = lib_tlist;
        lib_tlist = new_thread;
    }

    /* throw yourself upon the mercy of the almighty scheduler */
    lwp_yield();
}


/**
 * @brief Waits for a thread to terminate, deallocates its
 *  resources, and reports its termination status if status is non-NULL.
 *  Returns the tid of the terminated thread or NO_THREAD
 * 
 * @param status 
 * @return tid_t 
 */
tid_t lwp_wait(int *status)
{
    thread zombie;
    tid_t tid;
    
    /* wait for a thead to die */
    while(!zombies)
        lwp_yield();

    /* grab the undead thread off the list and update the list */
    zombie = zombies;
    zombies = zombies->lib_one;

    /* save the id */
    tid = zombie->tid;

    /* save the status */
    *status = zombie->status;

    /* deallocate it */
    if(zombie->stack)
        munmap(zombie->stack, zombie->stacksize);
    free_16(zombie);

    return tid;
}


/**
 * @brief Causes the LWP package to use the given scheduler to choose the
 *  next process to run. Transfers all threads from the old scheduler
 *  to the new one in next() order. If scheduler is NULL the library
 *  should return to round-robin scheduling.
 * 
 * @param sched scheduler to change to or NULL if no change.
 */
void lwp_set_scheduler(scheduler sched)
{
    if(sched)
    {
        thread l;

        /* migrate the threads to the new scheduler */
        while( (l = ActiveScheduler->next()) != NO_THREAD)
        {
            ActiveScheduler->remove(l);
            sched->admit(l);
        }
        ActiveScheduler = sched;
    }

}


/**
 * @brief Returns the pointer to the current scheduler.
 * 
 * @return scheduler 
 */
scheduler lwp_get_scheduler(void)
{
    return ActiveScheduler;
}


/**
 * @brief Returns the thread corresponding to the given thread ID, or NULL
 *  if the ID is invalid
 * 
 * @param tid id of the thread we want
 * @return thread
 */
thread tid2thread(tid_t tid)
{
    context dummy;
    thread l;

    /* setup a dummy to point to the start of our threads */
    dummy.lib_one = lib_tlist;

    /* go until the end or we find our target */
    for (l = &dummy; l->lib_one && l->lib_one->tid != tid; l = l->lib_one);

    /* return the thread if we found it */
    if(l->lib_one)
        return l->lib_one;
    else
        return NULL;
}

/******************************************************************************/
/* Default Scheduler Definition */

static thread qhead = NULL;
#define tnext sched_one
#define tprev sched_two

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
}

static void r_remove(thread victim)
{
    context dummy;
    thread l;

    if(qhead)
    {
        if(qhead == victim)
        {
            /* cut out of queue */
            victim->tprev->tnext = victim->tnext;
            victim->tnext->tprev = victim->tprev;
            
            /* what if it were qhead? */
            if (victim->tnext != victim)
                qhead = victim->tnext;
            else
                qhead = NULL;
        }
        else
        {
            dummy.tnext = qhead->tnext;

            for (l = &dummy; (l->tnext) && (l->tnext != victim) && (l->tnext != qhead); l = l->tnext);

            /* if we found it in tlist and it didn't loop */
            if (l->tnext && l->tnext != qhead)
            {
                /* cut out of queue */
                victim->tprev->tnext = victim->tnext;
                victim->tnext->tprev = victim->tprev;
            }
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


/******************************************************************************/
/* Helper functions */

void *malloc_16(size_t size)
{
    unsigned char *thread_ptr;
    unsigned char *malloc_ptr = malloc(sizeof(context) + 16);

    if(!malloc_ptr)
    {
        perror("malloc");
        return NULL;
    }

    thread_ptr = (unsigned char *) (((unsigned long) (malloc_ptr + 16)) & (~0xf));
    *(thread_ptr-1) = thread_ptr - malloc_ptr;
    return thread_ptr;
}

void free_16(void *ptr)
{
    unsigned char *malloc_ptr = ptr;
    malloc_ptr = malloc_ptr - *(malloc_ptr-1);
    free(malloc_ptr);
}