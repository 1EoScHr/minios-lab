/*
    案例七：理解线程的相关概念
*/

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

// (2)
//size_t shared;

void *son_thread1(void *thread_args)
{
    for (size_t i = 10; i > 0; i --)
    {
        // shared ++;
        // printf("son thread %d, shared %d\n", (int)pthread_self(), shared);
    }
}

void *son_thread2(void *thread_args)
{
    size_t *shared = (size_t *)thread_args;
    for (size_t i = 10; i > 0; i --)
    {
        (*shared) ++;
        printf("son thread %d, shared %d\n", (int)pthread_self(), *shared);
    }
}

void *son_thread3(void *thread_args)
{
    size_t x = 0;
    for (size_t i = 10; i > 0; i --)
    {
        x ++;
        printf("son thread %d, x %d\n", (int)pthread_self(), x);
    }
}

void *son_thread4(void *thread_args)
{
    size_t x = 0;
    for (size_t i = 10; i > 0; i --)
    {
        x --;
        printf("son thread %d, x %d\n", (int)pthread_self(), x);
    }
}

// (1)
int main(void)
{
    // size_t shared = 0;
    pthread_t tid;

    // int ret = pthread_create(&tid, NULL, son_thread2, NULL);
    
    /*
    int ret = pthread_create(&tid, NULL, son_thread2, &shared);
    if (ret != 0)
    {
        printf("fail to create son\n");
        exit(1);
    }

    for (size_t i = 10; i > 0; i --)
    {
        shared --;
        printf("master thread %d, shared %d\n", (int)pthread_self(), shared);
    }
    */

    int ret = pthread_create(&tid, NULL, son_thread3, NULL);
    if (ret != 0)
    {
        printf("fail to create son\n");
        exit(1);
    }
    ret = pthread_create(&tid, NULL, son_thread4, NULL);
    if (ret != 0)
    {
        printf("fail to create son\n");
        exit(1);
    }

    pthread_join(tid, NULL);
    pthread_exit(NULL);
}