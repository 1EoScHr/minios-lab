#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

typedef unsigned int uint;

// 定义线程参数结构体，包含所有需要传递的参数
typedef struct {
    int thread_id;          // 线程编号
    const char* filename;   // 文件名（字符串）
    unsigned int data_size; // 数据大小
} ThreadArgs;


uint getNumOfRead(char const* size);
void fastSort(uint data[], uint l, uint r);
void singleThread(FILE* ifp, FILE* ofp, unsigned int threads, char const* size);
void outputAndCheck(FILE* ofp, uint data[], uint n);

int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        printf("Wrong args!\narg1: num of process\narg2: num of thread\narg3: size of read\n");
        exit(EXIT_FAILURE);
    }
    
    uint start_tick = clock();
    
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

    char* end = NULL;
    singleThread(ifp, ofp, strtoul(argv[1], &end, 10), argv[2]);

    fclose(ifp);
    fclose(ofp);

    uint end_tick = clock();

    printf("clock ticks: %u\n", end_tick-start_tick);
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

// 进程调度
void process_schedule(FILE* ifp, int num_process, int num_thread)
{
    /*
    先考虑单进程
    */
    pthread_t threads[num_thread];  // 存储一个进程内线程ID

    for (int i = 0; i < num_thread; i ++)
    {
        // 线程传入参数结构体，用堆，防止被修改
        ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    
        // 创建线程
        /*
        pthread_create：参数1是创建的新线程id，参数2是线程属性（通常NULL表默认）
                        参数3是该线程要执行的函数，参数4是传给线程函数的参数
        */
        int ret = pthread_create(&threads[i], NULL, thread_task, task_id);
        if (ret != 0) {
            fprintf(stderr, "创建线程 %d 失败（错误码: %d）\n", i, ret);
            exit(EXIT_FAILURE);
        }
    
    }
}

// 线程调度
void* thread_task(void* arg) {

}

// 单线程处理
void singleThread(FILE* ifp, FILE* ofp, int threads, char const* size)
{
    uint n_read = getNumOfRead(size);    // 获取要从文件中读取的次数
    
    /*
    建立缓冲区，存放读取的数据
    但是不能够用栈，栈内存最大8MB，再大就要用堆
    // uint data[n_read];   
    */
    uint* data = (uint *)malloc(n_read * sizeof(uint));

    /*
    // 性能优化：一次性全读完
    for (uint i = 0; i < n_read; i ++)
    {
        fread(&data[i], 4, 1, ifp);     // 从文件中读取，每次4字节，按理来说这里还要做检查，就先跳过了
    }    
    */
    fread(data, 4, n_read, ifp);

    fastSort(data, 0, n_read - 1);      // 排序

    outputAndCheck(ofp, data, n_read);

    return;
}

void fastSort(uint data[], uint l, uint r) // 选用双指针法快排，速度快、内存占用较小
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
    if (l < j) fastSort(data, l, j-1);
    if (j < r) fastSort(data, j+1, r);
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