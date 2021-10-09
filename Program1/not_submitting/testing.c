#include <stdio.h>
#include "lwp.h"

int tmain(int arg);

static int counter = 0;

int main()
{
    int i;
    for(i=1; i<6; i++)
    {
        if(lwp_create((lwpfun) tmain, (void *)i, 0) == NO_THREAD)
        {
            printf("Failed to create thread %d\n", i);
        }
    }
    printf("The scheduler address? %lx\n", (unsigned long) lwp_get_scheduler());
    printf("Stacksize of thread %d: %lx\n", 3, (unsigned long) tid2thread((tid_t) 3));
    lwp_start();
    
    for(i=1; i<6; i++)
    {
        int st, tid;
        
        tid = lwp_wait(&st);
        printf("Thread %d exited with status %d\n", tid, st);
    }
    lwp_exit(0);
}

int tmain(int arg)
{
    printf("tid %ld:\n\tcounter %d\n", lwp_gettid(), counter++);
    lwp_yield();
    printf("tid %ld:\n\tcounter %d\n", lwp_gettid(), counter++);
    lwp_exit(0);
}