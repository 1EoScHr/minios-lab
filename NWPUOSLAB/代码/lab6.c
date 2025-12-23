/*
    案例六：理解进程的独立空间
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// (1)
/*
int main(void)
{
    size_t shared = 0;
    while (1)
    {
        shared ++;
        printf("%d\n", shared);
    }
    return 0;
}
*/

// (3)
size_t shared = 0;


// (2)
int main(void)
{
    int pid = fork();
    if (pid == 0)
    {
        while (1)
        {
            shared ++;
            printf("%d\n", shared);
        }
    }
    else
    {
        while (1)
        {
            shared --;
            printf("%d\n", shared);
        }
    }

    return 0;
}
