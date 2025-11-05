#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   // gettimeofday的头文件
#include <pthread.h>

typedef unsigned int uint;

// 定义线程参数结构体，包含所有需要传递的参数
typedef struct {
    uint* data; // 数据所在    
    uint l, r;  // 该线程处理的区间
    uint level_recursion;   // 递归级数
    uint num_thread;
} ThreadArgs;

uint getNumOfRead(char const* size);
void outputAndCheck(FILE* ofp, uint data[], uint n);
uint* inputData(FILE* ifp, uint num_data);
void process_schedule(uint num_process, uint num_thread, uint* data, uint num_data);
void thread_schedule(uint num_thread, uint* data, uint num_data);
void* empty_thread_task(void* threadargs);
void* thread_task(void* threadargs);
void fastSortRecursion(uint data[], uint l, uint r); // 选用双指针法快排，速度快、内存占用较小
uint fastSortSchedule(uint data[], uint l, uint r); // 线程调用版本

int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        printf("Wrong args!\narg1: num of process\narg2: num of thread\narg3: size of read\n");
        exit(EXIT_FAILURE);
    }
    
    char* end = NULL;
    uint num_process = strtoul(argv[1], &end, 10);
    uint num_thread = strtoul(argv[2], &end, 10);
    uint num_data = getNumOfRead(argv[3]);    // 获取要从文件中读取的数据数量
    
    /*
    直接从random读数据会导致每次运行都是随机的，不方便debug
    所以用dd命令读取random，将随机数据固定
    dd if=/dev/random of=./data bs=1024 count=4000000
    */ 
    // char inputPath[] = "/dev/random";
    char inputPath[] = "./data";
    char outputPath[] = "./result.txt";

    FILE* ifp = fopen(inputPath, "rb");
    FILE* ofp = fopen(outputPath, "w");
    if (ifp == NULL)
    {
        printf("fail to read %s\n", inputPath);
        return 1;
    }
    if (ofp == NULL)
    {
        printf("fail to create %s\n", outputPath);
        return 1;
    }

    uint* data = inputData(ifp, num_data);

    /*
        运行效果不佳，原因除了做的一些负优化外，还有是因为使用clock来计算时间
        其是会获取CPU时钟滴答数，会获取所有核的滴答数并求和，因此并非物理时间
        // uint start_tick = clock();
        // uint end_tick = clock();

        要用别的方法来获取物理时间。
    */

    struct timeval start_time, end_time;
    double duration;

    gettimeofday(&start_time, NULL);
    process_schedule(num_process, num_thread, data, num_data);
    gettimeofday(&end_time, NULL);

    outputAndCheck(ofp, data, num_data);

    // 计算耗时
    duration = (end_time.tv_sec - start_time.tv_sec) +  // 秒数差
               (end_time.tv_usec - start_time.tv_usec) / 1000000.0;  // 微秒转秒
    printf("sort cost: %.6f s\n", duration);

    fclose(ifp);
    fclose(ofp);

    return 0;
}

void outputAndCheck(FILE* ofp, uint data[], uint n)
{
    uint count = 1;
    uint last = data[0] - 1;    // 保证第一次一定不会触发

    for (uint i = 0; i < n; i ++)
    {
        if (data[i] < last)     // 检查序列正确性
        {
            printf("sort result error!\n");
            exit(EXIT_FAILURE);
        }
        
        if (data[i] == last)
        {
            count ++;
            continue;
        }
        
        if (count != 1)
        {
            count = 1;
            fprintf(ofp, "(%u)\n", count);
        }

        fprintf(ofp, "%u ", data[i]);
        last = data[i];
    }
}

// 把数据从文件读到堆上，并返回data中元素数量
uint* inputData(FILE* ifp, uint num_data)
{
    uint* data;
    
    /*
    建立缓冲区，存放读取的数据
    但是不能够用栈，栈内存最大8MB，再大就要用堆
    // uint data[n_read];   
    */
    data = (uint *)malloc(num_data * sizeof(uint));

    /*
    // 性能优化：一次性全读完
    for (uint i = 0; i < n_read; i ++)
    {
        fread(&data[i], 4, 1, ifp);     // 从文件中读取，每次4字节，按理来说这里还要做检查，就先跳过了
    }    
    */
    fread(data, 4, num_data, ifp);

    return data;
}

// 进程分配
void process_schedule(uint num_process, uint num_thread, uint* data, uint num_data)
{
    // 目前是单进程
    thread_schedule(num_thread, data, num_data);
}

// 单进程内线程调度
void thread_schedule(uint num_thread, uint* data, uint num_data)
{
    if (num_thread == 1)    // 特殊情况：单线程，后面起码都是双线程
    {
        fastSortRecursion(data, 0, num_data-1);
        return;
    }

    uint mid;
    mid = fastSortSchedule(data, 0, num_data-1);

    pthread_t threads[2];  // 存储一个进程内线程ID
    
    // 配置线程传入参数结构体，用堆，防止被修改
    ThreadArgs* largs = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    largs->data = data;
    largs->level_recursion = 1;
    largs->l = 0;
    largs->r = mid - 1;
    largs->num_thread = num_thread;

    ThreadArgs*rargs = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    rargs->data = data;
    rargs->level_recursion = 1;
    rargs->l = mid + 1;
    rargs->r = num_data-1;
    rargs->num_thread = num_thread;

    /*
    创建线程
    pthread_create：参数1是创建的新线程id，参数2是线程属性（通常NULL表默认）
                    参数3是该线程要执行的函数，参数4是传给线程函数的参数
    */
    int ret = pthread_create(&threads[0], NULL, thread_task, largs);
    if (ret != 0) {
        printf("Fail to create thread\n");
        exit(EXIT_FAILURE);
    }
    ret = pthread_create(&threads[1], NULL, thread_task, rargs);
    if (ret != 0) {
        printf("Fail to create thread\n");
        exit(EXIT_FAILURE);
    }

    // 等待所有线程结束
    for (int i = 0; i < 2; i++) {
        // 类似wait，参数1为线程id，参数2为线程返回值
        pthread_join(threads[i], NULL);
    }

    // 单进程内，不考虑合并进程，线程资源都是共享的
    return;
}

// 空线程，占位作用，已废弃，因为创建空线程开销不如加判断
/*
void* empty_thread_task(void* threadargs) 
{
    pthread_exit(NULL);
}
*/


// 线程内调度
void* thread_task(void* threadargs) 
{
    ThreadArgs args = *(ThreadArgs *)threadargs;
    free(threadargs);

    if (args.num_thread == (1 << args.level_recursion)) // 已经到末端线程
    {
        fastSortRecursion(args.data, args.l, args.r);
    }
    else
    {
        uint mid = fastSortSchedule(args.data, args.l, args.r);
        int ret;
        pthread_t threads[2];  // 存储下一层线程ID
    
        if (args.l < mid)
        {
            ThreadArgs* largs = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            largs->data = args.data;
            largs->level_recursion = args.level_recursion + 1;
            largs->l = args.l;
            largs->r = mid - 1;
            largs->num_thread = args.num_thread;
            ret = pthread_create(&threads[0], NULL, thread_task, largs);

            if (ret != 0) {
                printf("Fail to create thread\n");
                exit(EXIT_FAILURE);
            }
        }
        else threads[0] = 0;    // 空线程标记为0

        if (mid < args.r)
        {
            ThreadArgs*rargs = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            rargs->data = args.data;
            rargs->level_recursion = args.level_recursion + 1;
            rargs->l = mid + 1;
            rargs->r = args.r;
            rargs->num_thread = args.num_thread;
            ret = pthread_create(&threads[1], NULL, thread_task, rargs);

            if (ret != 0) {
                printf("Fail to create thread\n");
                exit(EXIT_FAILURE);
            }
        }
        else threads[1] = 0;


        // 等待所有线程结束
        for (int i = 0; i < 2; i++) {
            // 跳过空线程
            if (threads[i] == 0) continue;

            // 类似wait，参数1为线程id，参数2为线程返回值
            pthread_join(threads[i], NULL);
        }
    }

    // 进程退出
    pthread_exit(NULL);
}

// 排序算法
void fastSortRecursion(uint data[], uint l, uint r) // 选用双指针法快排，速度快、内存占用较小
{
    /*
    有一些边界情况有疑惑的话可以举个例子试一试就解决了
    */

    uint i = l;             // 初始化左指针（从左往右，找大于基准）
    uint j = r;             // 右指针（从右往左，找小于基准）
    uint base = data[l];    // 指定基准，这里选最左侧元素

    for ( ; i < j; )        // 重复，直到左右指针碰面；并且这个判定每一步都要做
    {
        while (data[j] >= base && i < j) // 右指针向左移动到首个小于基准的元素
        {
            j --;
        }

        while (data[i] <= base && i < j) // 左指针向右移动到首个大于基准的元素
        {
            i ++;
        }
        
        if (i < j)
        {
            uint temp = data[i];    // 交换左右指针所指元素
            data[i] = data[j];
            data[j] = temp;
        }
    }

    
    data[l] = data[j];      // 这时右(左)指针以左都小于基准元素，以右都大于基准元素所以将基准元素与右(左)指针元素交换
    data[j] = base;         

    // 结束/进入递归判断
    if (l < j) fastSortRecursion(data, l, j-1);
    if (j < r) fastSortRecursion(data, j+1, r);
}

uint fastSortSchedule(uint data[], uint l, uint r) // 线程调用版本
{
    /*
    有一些边界情况有疑惑的话可以举个例子试一试就解决了
    */

    uint i = l;             // 初始化左指针（从左往右，找大于基准）
    uint j = r;             // 右指针（从右往左，找小于基准）
    uint base = data[l];    // 指定基准，这里选最左侧元素

    for ( ; i < j; )        // 重复，直到左右指针碰面；并且这个判定每一步都要做
    {
        while (data[j] >= base && i < j) // 右指针向左移动到首个小于基准的元素
        {
            j --;
        }

        while (data[i] <= base && i < j) // 左指针向右移动到首个大于基准的元素
        {
            i ++;
        }
        
        if (i < j)
        {
            uint temp = data[i];    // 交换左右指针所指元素
            data[i] = data[j];
            data[j] = temp;
        }
    }

    
    data[l] = data[j];      // 这时右(左)指针以左都小于基准元素，以右都大于基准元素所以将基准元素与右(左)指针元素交换
    data[j] = base;         

    return j;
}

// 判断方式有些丑陋
uint getNumOfRead(char const* size)
{
    uint n_read = 0;

    // 这里使用扁平if else判断，适合于平级逻辑，今天才知道
    if (strcmp(size, "1M") == 0) n_read = 262144;   
    else if (strcmp(size, "10M") == 0) n_read = 2621440;    
    else if (strcmp(size, "100M") == 0) n_read = 26214400;    
    else if (strcmp(size, "1G") == 0) n_read = 268435456;
    else if (strcmp(size, "2G") == 0) n_read = 536870912;
    else if (strcmp(size, "4G") == 0) n_read = 1073741824;
    else
    {
        printf("%s is invalid\n", size);
        exit(EXIT_FAILURE);
    } 
    return n_read;
}