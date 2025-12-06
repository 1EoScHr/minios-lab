// 案例5

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

int main()
{
    int pid_c1 = fork();

    if (pid_c1)  // parent
    {
        int pid_c2 = fork();

        if (pid_c2) // parent
        {
            int pid_c3 = fork();

            if (pid_c3) // parent
            {
                printf("Im parent, myPID:%d, myPPID:%d\n", getpid(), getppid());
            }
            else    // child_3
            {
                printf("Im child_3, myPID:%d, myPPID:%d\n", getpid(), getppid());
            }

        }
        else    // child_2
        {
            int pid_c2_c1 = fork();

            if (pid_c2_c1)  // child_2
            {
                int pid_c2_c2 = fork();

                if (pid_c2_c2)  // child_2
                {
                    printf("Im child_2, myPID:%d, myPPID:%d\n", getpid(), getppid());
                }
                else    // child_2'child_2
                {
                    printf("Im child_2's child_2, myPID:%d, myPPID:%d\n", getpid(), getppid());
                }
            }
            else    // child_2'child_1
            {
                printf("Im child_2's child_1, myPID:%d, myPPID:%d\n", getpid(), getppid());
            }
        }
    }
    else        // child_1
    {
        printf("Im child_1, myPID:%d, myPPID:%d\n", getpid(), getppid());
    }
}