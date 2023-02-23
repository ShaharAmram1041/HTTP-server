#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"


//Todo: private function delete the allocated memory and the locks if fail
/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool.
 */
threadpool* create_threadpool(int num_threads_in_pool){
    /* the given number is not legal */
    if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool <= 0)
        return NULL;
    threadpool* t = (threadpool*)malloc(sizeof(threadpool));
    if(t == NULL){
        perror("malloc fail!");
        return NULL;
    }
    t->num_threads = num_threads_in_pool;	//number of active threads
    t->qsize = 0;	        //number in the queue
    t->threads = (pthread_t*) malloc(sizeof(pthread_t) * num_threads_in_pool);
    t->qhead = t->qtail = NULL;		//queue head,tail pointer
    //init the mutex
    if(pthread_mutex_init(&t->qlock, NULL) != 0 ){
        perror("Mutex failed!");
        free(t);
        return NULL;
    }
    //init the condition variables
    if(pthread_cond_init(&t->q_not_empty,NULL) != 0 || pthread_cond_init(&t->q_empty,NULL) != 0){
        perror("Condition variable failed!");
        return NULL;
    }
    t->shutdown = 0;    //1 if the pool is in distruction process
    t->dont_accept = 0;   //1 if destroy function has begun
    for(int i = 0;i < num_threads_in_pool;i++)
        if(pthread_create(&t->threads[i],NULL, do_work,(void*)t) != 0)
            return NULL;
    return t;
}


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
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    /* edge cases */
    if(from_me == NULL || dispatch_to_here == NULL)
        return;
    /* check don't accept */
    if(pthread_mutex_lock(&from_me->qlock) != 0){
        perror("Mutex fail!");
        return;}
    if(from_me->dont_accept == 1){
        pthread_mutex_unlock(&from_me->qlock);
        return;
    }
    pthread_mutex_unlock(&from_me->qlock);

    /* create work */
    work_t* work = (work_t*) malloc(sizeof (work_t));
    if(work == NULL){
        perror("Malloc fail!");
        return;
    }
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;
    if(pthread_mutex_lock(&from_me->qlock) != 0){
        perror("Mutex fail!");
        return;}
    //destroy function has begun
    if(from_me->dont_accept == 1){
        if(pthread_mutex_unlock(&from_me->qlock) != 0){
            perror("Unlock fail!");
            free(work);
            return;}
        free(work);
        work = NULL;
        return;
    }
    /* enqueue to the list */

    /* the list is empty */
    if(from_me->qsize == 0){
        from_me->qhead = work;
        from_me->qtail = work;}

    /* the list is not empty */
    else{
        from_me->qtail->next = work;
        from_me->qtail = work;}
    from_me->qsize++;
    if(pthread_cond_signal(&from_me->q_not_empty) != 0 ){
        perror("Signal fail!");
        if(pthread_mutex_unlock(&from_me->qlock) != 0)
            perror("Unlock fail!");
        pthread_exit(0);
    }
    if(pthread_mutex_unlock(&from_me->qlock) != 0){
        perror("Unlock fail!");
        pthread_exit(0);
    }
}

/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void* do_work(void* p){
    //p is null
    if(p == NULL)
        return NULL;
    //casting to threadpool
    threadpool* t = (threadpool*)(p);
    work_t* work;
    while(1){
        //the thread locks the mutex
        if(pthread_mutex_lock(&t->qlock) != 0){
            perror("Lock fail!");
            return NULL;}

        //the destruction process has begun
        if(t->shutdown == 1) {
            if(pthread_mutex_unlock(&t->qlock) != 0){
                perror("Unlock fail");
                return NULL;}
            return 0;}

        //if there is no work to do
        if(t->qsize == 0){
            if(pthread_cond_wait(&t->q_not_empty,&t->qlock) != 0){
                perror("Wait fail!");
                if(pthread_mutex_unlock(&t->qlock) != 0)
                    perror("Unlock fail!");
                return NULL;}}

        //the destruction process has begun
        if(t->shutdown == 1) {
            if(pthread_mutex_unlock(&t->qlock) != 0){
                perror("Unlock fail");
                return NULL;}
            return 0;}

        /* creating work */
        work = t->qhead;
        if(work == NULL){
            if(pthread_mutex_unlock(&t->qlock) != 0)
                perror("Mutex unlocked fail!");
            return NULL;}

        if(work != NULL){
            t->qhead = t->qhead->next;
            t->qsize--;
            if(t->qhead == NULL)
                t->qtail = t->qhead;
            work->routine(work->arg);
            free(work);
            work = NULL;
        }

        /* If the queue becomes empty and the destruction process
        wait to begin, signal the destruction process */
        if(t->dont_accept == 1 && t->qsize == 0){
            if(pthread_cond_signal(&t->q_empty) != 0)
                perror("Signal fail!");
            if(pthread_mutex_unlock(&t->qlock) != 0){
                perror("Unlock fail!");
                return NULL;}
            return 0;}

        pthread_mutex_unlock(&t->qlock);
    }
}

/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme){
    if(destroyme == NULL)
        return;
    //lock the mutex
    if(pthread_mutex_lock(&destroyme->qlock) != 0){
        perror("Mutex fail!");
        return;}
    destroyme->dont_accept = 1;
    if(destroyme->qsize > 0)
        if(pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock) != 0){
            perror("wait fail!");
            pthread_exit(0);}

    destroyme->shutdown = 1;
    /* signal threads that wait on ‘empty queue’ */
    pthread_cond_broadcast(&destroyme->q_not_empty);
    if(pthread_mutex_unlock(&destroyme->qlock) != 0){
        perror("Mutex fail!");
        return;}
    for(int i = 0;i < destroyme->num_threads;i++){
        if (pthread_join(destroyme->threads[i], NULL) != 0) {
            perror("pthread_create() error");
            exit(3);
        }
       }
    free(destroyme->threads);
    destroyme->threads = NULL;

    //destroying the locks
    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_cond_destroy(&destroyme->q_empty);
    free(destroyme);
    destroyme = NULL;

}
