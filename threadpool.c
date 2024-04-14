#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int task(void*);
int main(int argc,char* argv[])
{
    if(argc!=4)
    {
        printf("Usage: pool <pool-size> <number-of-tasks> <max-number-of-request>\n");
        exit(EXIT_FAILURE);
    }
    size_t pool_size=atoi(argv[1]);
    size_t num_of_tasks=atoi(argv[2]);
    size_t max_num_of_tasks=atoi(argv[3]);
    threadpool* myPool;
    myPool= create_threadpool((int)pool_size);
    for (int i = 0; i <num_of_tasks;i++)
    {
        if(i>=max_num_of_tasks) break;
        dispatch(myPool,(dispatch_fn)task,NULL);
    }
    destroy_threadpool(myPool);
    exit(EXIT_SUCCESS);
}
threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool<0 || num_threads_in_pool>MAXT_IN_POOL)
    {
        printf("num_threads_in_pool is incorrect\n");
        exit(EXIT_FAILURE);
    }
    threadpool *tp=(threadpool*)malloc(sizeof(threadpool));
    if(tp==NULL)
    {
        printf("malloc error\n");
        exit(EXIT_FAILURE);
    }
    tp->num_threads=num_threads_in_pool;
    tp->qsize=0;
    tp->threads=(pthread_t*)malloc(num_threads_in_pool*sizeof(pthread_t));
    if(tp->threads==NULL)
    {
        printf("malloc error\n");
        exit(EXIT_FAILURE);
    }
    tp->qhead=NULL;
    tp->qtail=NULL;
    int result1=pthread_mutex_init(&tp->qlock,NULL); // needs to be closed!!!!
    if (result1!=0)
    {
        free(tp->threads);
        free(tp);
        printf("pthread_mutex_init failed\n");
        exit(EXIT_FAILURE);
    }
    int result2=pthread_cond_init(&tp->q_empty,NULL);
    if (result2!=0)
    {
        pthread_mutex_destroy(&tp->qlock); // Clean up mutex
        free(tp->threads);
        free(tp);
        printf("pthread_cond_init failed\n");
        exit(EXIT_FAILURE);
    }
    int result3= pthread_cond_init(&tp->q_not_empty,NULL);
    if (result3!=0)
    {
        pthread_mutex_destroy(&tp->qlock); // Clean up mutex
        pthread_cond_destroy(&tp->q_empty);
        free(tp->threads);
        free(tp);
        printf("pthread_cond_init failed\n");
        exit(EXIT_FAILURE);
    }
    tp->shutdown=0;
    tp->dont_accept=0;
    for (int i = 0; i <num_threads_in_pool;i++)
    {
        int res=pthread_create(&tp->threads[i],NULL, do_work,(void*)tp);
        if(res!=0)
        {
            pthread_mutex_destroy(&tp->qlock); // Clean up mutex
            pthread_cond_destroy(&tp->q_empty);
            pthread_cond_destroy(&tp->q_not_empty);
            free(tp->threads);
            free(tp);
            printf("error: pthread_create\n");
            exit(EXIT_FAILURE);
        }
    }
    return tp;
}
void* do_work(void* p)
{
    threadpool* tp=(threadpool*)p;
    while(1) {
        if(pthread_mutex_lock(&tp->qlock)!=0)//lock the mutex
        {
            printf("lock error\n");
            pthread_exit(NULL);
        }
        //inside the critical section:
        while(tp->shutdown==0 && tp->qsize == 0)
        {
            pthread_cond_wait(&tp->q_not_empty,&tp->qlock);
        }
        if (tp->shutdown == 1)//if destruction process has begun, exit thread
        {
            pthread_mutex_unlock(&tp->qlock);
            pthread_exit(NULL);
        }
        // dequeue the next work and do it:
        work_t* currentWork=tp->qhead;
        if(tp->qsize>1)
        {
            work_t* temp=tp->qhead->next;
            tp->qhead=temp;
        }
        else tp->qhead=NULL;// if qsize=1
        currentWork->next=NULL;
        tp->qsize=(tp->qsize-1);
        // if it's the last work,and we want to close the queue:
        if(tp->dont_accept==1 && tp->qsize==0)
        {
            // maybe also unlock here?
            pthread_cond_signal(&tp->q_empty);// wakes up the destroy thread
        }
        if(pthread_mutex_unlock(&tp->qlock)!=0)// Out of the critical section
        {
            printf("mutex_unlock error\n");
            pthread_exit(NULL);
        }
        // Because work_t has a pointer to a function,calls the function with her argument:
        currentWork->routine(currentWork->arg);
        free(currentWork);//check if its ok !!!!!!!!!!!!!!!!
    }

}
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if(pthread_mutex_lock(&from_me->qlock)!=0)
    {
        printf("mutex_lock error\n");
        pthread_exit(NULL);
    }
    if(from_me->dont_accept==1)
    {
        if(pthread_mutex_unlock(&from_me->qlock)!=0)
        {
            printf("mutex_unlock error\n");
            pthread_exit(NULL);
        }
        return;
    }
    if(pthread_mutex_unlock(&from_me->qlock)!=0)
    {
        printf("mutex_unlock error\n");
        pthread_exit(NULL);
    }
    // the work is not a critical section:
    work_t* new_work=(work_t*)malloc(sizeof(work_t));
    if(new_work==NULL)
    {
        printf("malloc error\n");
        exit(EXIT_FAILURE);
    }
    new_work->arg=arg;
    new_work->routine=dispatch_to_here;
    new_work->next=NULL;
    // Enqueue the new work:
    if(pthread_mutex_lock(&from_me->qlock)!=0)
    {
        printf("mutex_lock error\n");
        pthread_exit(NULL);
    }
    if(from_me->qhead==NULL)// when it's the only job:
    {
        from_me->qhead=new_work;
        from_me->qtail=new_work;
    }
    else {
        from_me->qtail->next = new_work;
        from_me->qtail = from_me->qtail->next;
    }
    from_me->qsize=from_me->qsize+1;
    if(pthread_cond_signal(&from_me->q_not_empty)!=0)
    {
        printf("cond_signal error\n");
        pthread_exit(NULL);
    }
    if(pthread_mutex_unlock(&from_me->qlock)!=0)
    {
        printf("mutex_unlock error\n");
        pthread_exit(NULL);
    }
}
void destroy_threadpool(threadpool* destroyme)
{
    if(pthread_mutex_lock(&destroyme->qlock)!=0)
    {
        printf("mutex_lock error\n");
        pthread_exit(NULL);
    }
    destroyme->dont_accept=1;
    while(destroyme->qsize>0)//wait until there is no more work to do:
    {
        pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock);
    }
    destroyme->shutdown=1;
    //printf("DOING BROADCAST:\n");
    if(pthread_cond_broadcast(&destroyme->q_not_empty)!=0)
    {
        printf("cond_broadcast error\n");
        pthread_exit(NULL);
    }
    if(pthread_mutex_unlock(&destroyme->qlock)!=0)// I added this-not in the flow
    {
        printf("mutex_unlock error\n");
        pthread_exit(NULL);
    }
    for (int i = 0; i <destroyme->num_threads; i++)
    {
        pthread_join(destroyme->threads[i],NULL);
    }
    free(destroyme->threads);
    pthread_cond_destroy(&destroyme->q_empty);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_mutex_destroy(&destroyme->qlock);
    free(destroyme);
}
int task(void* arg)
{
    pthread_t tid = pthread_self();
    for (int i = 0; i <1000;i++)
    {
        printf("Thread ID: %lu\n", tid);
        usleep(100000);
    }
    return 0;
}



