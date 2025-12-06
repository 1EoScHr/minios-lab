// 案例四

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

int main()
{
    int c1_pid = fork();
    if(c1_pid) // parent
    {
        int c2_pid = fork();
        if(c2_pid)  // parent
        {
            printf("new nice of a: %d\n", nice(10));
            printf("a\n");
        }
        else    // child2
        {            
            printf("nice of c: %d\n", nice(0));
            printf("c\n");
        }
    }
    else    // child1
    {
        printf("nice of b: %d\n", nice(0));
        printf("b\n");
    }

    return 0;
}