/*
 * Desc: Thread Switching Emulation x64
 * Author: Good
 * Date: 2023-11-02
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>   // 轮转，使用定时器

#define N 3                             // number of threads
#define USER_STACK_SIZE (1024 * 1024)   // user-space stack size of each thread
#define KERNEL_STACK_SIZE (1024 * 1024) // kernel-space stack size of each thread

// register offset in struct context
#define EIP_OFFSET (1 * 8)
#define EFLAGS_OFFSET (3 * 8)
#define REGS_START_OFFSET (9 * 8)

/*
Just save callee registers
*/
typedef struct context  // 调用者寄存器上下文
{
    uint64_t rsp;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rflags;
    uint64_t rbp;
    uint64_t rip;
} REGS;

typedef void (*thread_entry)();
typedef unsigned long int gthread_t;
typedef unsigned int gthread_attr_t;

typedef struct thread_control_block // 线程控制块
{
    thread_entry entry; // thread entry point // 入口函数
    char *user_stack_end;   // 用户栈尽头
    char *kernel_stack_end; // 内核栈尽头
    char *user_esp_save; // esp position before switching to kernel stack   // 切换到内核栈前保留
    gthread_t tid;  // tid

    int exited; // 是否退出标记，初始为0，退出为1
} TCB;

typedef union thread_union
{
    TCB tcb;
    char kernel_stack[KERNEL_STACK_SIZE]; // kernel-space stack we prepared for the threads
} Thread;
Thread thread_table[N];

volatile sig_atomic_t cur = -1;       // point to current thread，初始设为-1表示没有
char user_stack[N * USER_STACK_SIZE]; // user-space stack we prepared for the threads

void delay(int n);
void kernel();
void switch_context();
void restart_restore();
void scheduler();

////////////////////////////////////////////////////////////////////////////////////////////////////
/*
    想了想还是把新的这几个放到这里面，因为放到另外的文件还要重新调整，太麻烦，赶作业，时间要紧
*/

int gthread_create(gthread_t *thread, const gthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    // 找一个可用的线程，也就是entry == NULL
    size_t i;
    for (i = 0; i < N; i++) {
        if (thread_table[i].tcb.entry == NULL) break;
    }
    if (i >= N) return -1;

    // 复用main中初始化逻辑
    thread_table[i].tcb.entry = (thread_entry)start_routine;
    thread_table[i].tcb.tid = i;
    thread_table[i].tcb.user_stack_end = user_stack + USER_STACK_SIZE * (i + 1);
    thread_table[i].tcb.kernel_stack_end = thread_table[0].kernel_stack + KERNEL_STACK_SIZE * (i + 1);
    thread_table[i].tcb.user_esp_save = thread_table[i].tcb.user_stack_end - 8;
    thread_table[i].tcb.exited = 0;
    *(uint64_t *)(thread_table[i].tcb.kernel_stack_end - EIP_OFFSET) = (uint64_t)start_routine;
    *(uint64_t *)(thread_table[i].tcb.kernel_stack_end - EFLAGS_OFFSET) = 0x202;
    *thread = i;

    return 0;
}

int gthread_exit(void *retval)
{
    thread_table[cur].tcb.exited = 1;   // 设为退出
    kernel();
    while (1);
}

int gthread_join(gthread_t thread, void **retval)
{
    if (thread < 0 || thread >= N) return -1;
    while (!thread_table[thread].tcb.exited) {
        ; // 忙等
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// a short delay
// 原版太长，也不能为0
void delay(int n)
{
    volatile int k = 0;
    for (int i = 0; i < n * 10000; i++)
        for (int j = 0; j < 10000; j++)
            k++;
}

void threadA()
{
    size_t i = 100;
    while (i)
    {
        printf("A ");
        fflush(stdout);
        delay(1);
        i --;
    }

    gthread_exit(NULL);
}
void threadB()
{
    size_t i = 30;
    while (i)
    {
        printf("B ");
        fflush(stdout);
        delay(1);
        i --;
    }

    gthread_exit(NULL);
}
void threadC()
{
    size_t i = 10;
    while (i)
    {
        printf("C ");
        fflush(stdout);
        delay(1);
        i --;
    }

    gthread_exit(NULL);
}

void scheduler()
{
    // select a new thread
    // 跳过已停止进程
    int next = (cur + 1) % N;
    while (thread_table[next].tcb.exited)
    {
        next = (next + 1) % N;
        if (next == cur)
        {
            if (thread_table[cur].tcb.exited) exit(0);  // 如果所有线程都退出了
            break; // 绕一圈没有可用的，就继续运行当前
        } 
    }

    cur = next;
}

char *get_cur_user_rsp_save()
{
    return thread_table[cur].tcb.user_esp_save;
}

char *get_cur_kernel_stack_end()
{
    return thread_table[cur].tcb.kernel_stack_end;
}

// emulated kernel space
void kernel()
{
    // 进入内核栈前先把当前线程数据保存
    // save esp position before switching to kernel stack
    __asm__("movq %%rsp, %%rax\n\t"
            "movq %%rax, %0"
            : "=r"(thread_table[cur].tcb.user_esp_save)
            : // no input variable
            : "rax");

    // switch to kernel stack of current thread
    __asm__("movq %0, %%rax\n\t"
            "movq %%rax, %%rsp"
            : // no output variable
            : "r"(thread_table[cur].tcb.kernel_stack_end)
            : "rax");

    switch_context();
}

// use the signal SIGINT(Ctrl+C) to simulate interrupt
void signal_handler(int signum)
{
    // clear the blocked state of SIGINT
    sigset_t set;
    sigemptyset(&set);
    // sigaddset(&set, SIGINT);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, 0);

    // enter the emulated kernel space
    kernel();

    // will return to the interrupted running point
}
// re-bind Ctrl+Z (SIGTSTP) to program exit
void tstp_handler(int signum)
{
    exit(0);
}

int main()
{
    // 初始化所有线程
    for (int i = 0; i < N; i++) {
        thread_table[i].tcb.entry = NULL;
        thread_table[i].tcb.exited = 1;
    }

    // signal handler binding   
    // 中断信号绑定
    // signal(SIGINT, signal_handler);
    signal(SIGALRM, signal_handler);// 使用定时器闹钟信号
    signal(SIGTSTP, tstp_handler);

    // 配置定时器
    struct itimerval t = {{0, 10000}, {0, 10000}};  // 每10ms中断一次
    setitimer(ITIMER_REAL, &t, NULL);

    gthread_t t1, t2, t3;
    gthread_create(&t1, NULL, (void*(*)(void*))threadA, NULL);
    gthread_create(&t2, NULL, (void*(*)(void*))threadB, NULL);
    gthread_create(&t3, NULL, (void*(*)(void*))threadC, NULL);

    cur = 0;
    restart_restore(); // 不返回

    return 0;
}