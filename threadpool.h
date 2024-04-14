#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H
#include <pthread.h>

/**
 * threadpool.h
 *
 * This file declares the functionality associated with
 * your implementation of a threadpool.
 */

// maximum number of threads allowed in a pool
#define MAXT_IN_POOL 200

/**
 * the pool holds a queue of this structure
 */
typedef struct work_st
{
    int (*routine) (void*);  //the threads process function(a pointer to de work function)
    void * arg;  //argument to the function
    struct work_st* next;// A pointer to the next work,like a linked-List of works
} work_t;



/**
 * The actual pool
 */
typedef struct _threadpool_st {
    int num_threads;	//number of active threads
    int qsize;	        //current number in the queue
    pthread_t *threads;	//pointer to threads that will have all my threads
    work_t* qhead;		//work queue head pointer
    work_t* qtail;		//work queue tail pointer
    pthread_mutex_t qlock;		//lock on the queue list
    pthread_cond_t q_not_empty;	//Non-empty condidtion vairiable for the regular threads
    //the thread that enterd a work to the queue will "signal" the not_empty variable
    pthread_cond_t q_empty;//empty condidtion vairiable for the distroy thread
    //the thread that takes the last work from the queue will signal the distroy do wake up
    int shutdown;            //1 if the pool is in distruction process
    //when the distroy thread will wake up,he will change that flag to 1
    // and then all the wating threads that arn't doing something will pthread_exit.
    int dont_accept;       //1 if destroy function has begun
    //Don't accept any more work flag.
} threadpool;

// will get a funtion to work on and create a new "work_st" with the funtion
// and arg and also will mange the queque becuse a new "Node" was enterd
// "dispatch_fn" declares a typed function pointer.  A
// variable of type "dispatch_fn" points to a function
// with the following signature:
//
//     int dispatch_function(void *arg);
// a new type that is a pointer to a funtion that gets void* and returns an int
typedef int (*dispatch_fn)(void *);


// gets the num of thread that will be the max in the pool
// we have to check that hi isnt bigger then the deffine
/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4.create the threads, the thread init function is do_work and its argument is the initialized threadpool.
 */
threadpool* create_threadpool(int num_threads_in_pool);

// The funtion that enters a work to the line:
/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg);


/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine on the work that was enterd in the queue
 *
 */
void* do_work(void* p);

// will look the mutex,and will raise the dont_accept flag to 1(no more work)
/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme);



#endif //THREADPOOL_THREADPOOL_H