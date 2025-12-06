#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   // gettimeofday的头文件
#include <sys/wait.h>   // waitpid
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

typedef unsigned int uint;

// 定义线程参数结构体，包含所有需要传递的参数
typedef struct {
    uint* data; // 数据所在    
    uint l, r;  // 该线程处理的区间
    uint level_recursion;   // 递归级数
    uint num_thread;
} ThreadArgs;

// 最小堆排序节点
typedef struct {
    uint value;
    int idx;
    bool eof;
} HeapNode;

// 输入缓冲区
typedef struct {
    uint* data;       // 缓冲区数据（存储uint数组）
    size_t capacity;  // 缓冲区容量（最多能存多少个uint）
    size_t pos;       // 当前读取位置（已读到第几个元素）
    size_t count;     // 缓冲区中实际有效元素数量
    FILE* fp;         // 对应的文件指针
} InputBuffer;

// 输出缓冲区
typedef struct {
    uint* data;       // 缓冲区数据
    size_t capacity;  // 缓冲区容量（最多能存多少个uint）
    size_t pos;       // 当前写入位置
    FILE* fp;         // 对应的输出文件指针
} OutputBuffer;

uint getNumOfRead(char const* size);
void outputAndCheck(char* originPath, char* outputPath, uint num_data);
uint* inputData(char* inputPath, uint read_start, uint num_data);
void process_schedule(uint num_process, uint num_thread, uint num_data, char* inputPath, char* outputPath);
void thread_schedule(uint num_thread, uint* data, uint num_data);
// void* empty_thread_task(void* threadargs);
void* thread_task(void* threadargs);
void fastSortRecursion(uint data[], uint l, uint r); // 选用双指针法快排，速度快、内存占用较小
uint fastSortSchedule(uint data[], uint l, uint r); // 线程调用版本
void write2TempAndPipe(uint* data, uint num_data, int* pipe_w);
void kMergeSort(char** tempPaths, char* outputPath, uint num_process); // 使用最小堆法，最适合多文件合并排序；这里的堆是数据结构，而不是内存里的堆，是完全二叉树
void minHeapify(HeapNode heap[], int size, int i);
bool is_less(const HeapNode* a, const HeapNode* b);

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
    char outputOriginPath[] = "/tmp/originResult";
    char outputPath[] = "./result.txt";

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
    process_schedule(num_process, num_thread, num_data, inputPath, outputOriginPath);

    gettimeofday(&end_time, NULL);
    duration = (end_time.tv_sec - start_time.tv_sec) +  // 秒数差
               (end_time.tv_usec - start_time.tv_usec) / 1000000.0;  // 微秒转秒
    printf("sort cost: %.6f s\n", duration);

    outputAndCheck(outputOriginPath, outputPath, num_data);

    gettimeofday(&end_time, NULL);
    duration = (end_time.tv_sec - start_time.tv_sec) +  // 秒数差
               (end_time.tv_usec - start_time.tv_usec) / 1000000.0;  // 微秒转秒
    printf("sort + write cost: %.6f s\n", duration);
    
    return 0;
}

// 按照规定要求输出结果到文件
void outputAndCheck(char* originPath, char* outputPath, uint num_data)
{
    FILE* originFile = fopen(originPath, "rb");
    if (originFile == NULL)
    {
        printf("fail to open origin data\n");
        exit(EXIT_FAILURE);
    }
    FILE* outputFile = fopen(outputPath, "w");
    if (outputFile == NULL)
    {
        printf("fail to create output file\n");
        exit(EXIT_FAILURE);
    }

    uint count = 1; // 重复元素数量
    uint data;
    fread(&data, 4, 1, originFile);
    uint last = data - 1;    // 保证第一次一定不会触发

    for (uint i = 0; i < num_data; i ++)
    {
        if (data < last)     // 检查序列正确性
        {
            if (i != 0 || data != 0)    // 防止最小值为0导致bug
            {
                printf("sort result error!\n");
                exit(EXIT_FAILURE);
            }
        }
        
        if (data == last)
        {
            count ++;
            // continue; // 这里原本是直接在数组上有用，因为i会自增，可是现在直接到文件里面读，就会导致死循环直到超过numdata
        }
        
        else
        {
            if (count != 1)
            {
                fprintf(outputFile, "(%u)\n", count);
                count = 1;
            }

            fprintf(outputFile, "%u ", data);
            last = data;
        }

        if (i != num_data - 1)
        {
            fread(&data, 4, 1, originFile);
        }
    }

    // 最后一组元素若有重复，会因跳出循环而不打印重复次数，所以在外面补上
    if (count != 1)
    {
        fprintf(outputFile, "(%u)\n", count);
    }

    // 释放、关闭
    fclose(outputFile);
    fclose(originFile);

    remove(originPath);
}

// 把数据从文件读到堆上，并返回data中元素数量
uint* inputData(char* inputPath, uint read_start, uint num_data)
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
    for (uint i = 0; i < n_read; i ++) fread(&data[i], 4, 1, ifp);  // 从文件中读取，每次4字节，按理来说这里还要做检查，就先跳过了
    */
    
    FILE* inputFile = fopen(inputPath, "rb");   // 独立打开文件，多个进程使用同一个文件指针可能会乱
    if (inputFile == NULL)
    {
        fclose(inputFile);
        printf("fail to open %s\n", inputPath);
        exit(EXIT_FAILURE);
    }

    // fseek要的是字节数！
    if (fseek(inputFile, read_start * 4, SEEK_SET) != 0)    // 把指针指到偏移处
    {
        fclose(inputFile);
        printf("fseek error\n");
        exit(EXIT_FAILURE);
    }

    fread(data, 4, num_data, inputFile);

    fclose(inputFile);

    return data;
}

/*
    进程分配，在已经实现了多线程的基础上，一开始并不理解为何要再增加这一步——
    因为从逻辑上，再另外分几个进程和多开一些线程道理都一样。

    问了问ai，发现多进程是有更高层次的考虑：
    1.  单进程中，所有线程共享一块资源，某一个线程崩溃会导致整个进程所有的线程都挂掉
        而多进程间资源相互独立，某一进程挂掉并不影响其他进程，工作进度得到最大限度的保留
    2.  单进程的内存量是有上限的，一旦要处理的数据超过上限，就无法处理
        而多进程就可以把要处理的数据拆分为小块进行处理（虽然但是我用ulimit -v查看虚拟内存上限，结果是unlimited…）
    3.  单进程里，各线程抢占的是一个线程所分配的系统资源，整个程序的上限是os分配给进程的资源
        而多进程相当于开了多个窗口，极限情况下能逼近整机资源

    基于上面这些，进程分配就应该符合这场景：
        创建多个子进程依次从输入文件读取到内存 -> 每个子进程内使用多线程排序 -> 父进程使用k路归并算法合并子进程结果（局部有序、整体无序适合使用）
    来模拟当有一个极其巨大的文件需要分成多少块来处理、最后归并。
*/
void process_schedule(uint num_process, uint num_thread, uint num_data, char* inputPath, char* outputPath)
{
    pid_t processID[num_process];   // 存放所有子进程的ID
    uint child_size = num_data / num_process;   // 子进程每次从文件读取的数量（最后一个线程除外）
    uint imParentProcess = 1;   // 区分父子进程
    int pipe_rw[2];
    if (pipe(pipe_rw) == -1)    // 建立管道，用于父进程获取子进程创建的临时文件
    {
        printf("fail to create pipe\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_process; i ++)
    {
        processID[i] = fork();
        if (processID[i])   // 父进程
        {
            continue;
        }
        else    // 子进程
        {
            close(pipe_rw[0]);      // 关闭子进程的管道读端，以防后续出现bug:任意一个读端没有关闭
            imParentProcess = 0;    // 子进程打上标记

            uint child_read_start = i * child_size; // 获取读取位置与数量
            uint child_num_data = (i < num_process-1) ? child_size : (num_data - child_read_start);

            uint* data = inputData(inputPath, child_read_start, child_num_data);    // 读取

            thread_schedule(num_thread, data, child_num_data);  // 调用多线程排序

            write2TempAndPipe(data, child_num_data, pipe_rw);   // 把排序结果写进临时文件
            close(pipe_rw[1]);      // 关闭写端

            free(data);
            exit(EXIT_SUCCESS);     // 关闭子进程
        }
    }
    
    if (imParentProcess)    // 父进程
    {
        close(pipe_rw[1]);  // 父进程关闭写端，只读
        
        char temp[num_process][17];    // 子进程创建的临时文件路径为/tmp/temp.XXXXXX，加上末尾，刚好17字节

        /*
        确保所有子进程已经关闭
        管道有原子性，子进程写时不会重叠
        则最后一读就一网打尽
        哪怕开64个进程也才1088 B，一般是小于管道缓冲区大小的
        */
        for (int i = 0; i < num_process; i ++)
        {
            int status;
            waitpid(processID[i], &status, 0);
        }

        int read_len = read(pipe_rw[0], temp, num_process*17);     // 从管道读
        if (read_len == -1)
        {
            printf("fail to read pipe\n");
            exit(EXIT_FAILURE);
        }

        // 函数要求是字符指针的指针，直接得到的是二维数组，要转换一下
        char* tempPaths[num_process];
        for (int i = 0; i < num_process; i++) 
        {
            tempPaths[i] = temp[i];  // 指针数组元素指向二维数组的每一行
        }
        kMergeSort(tempPaths, outputPath, num_process); // 归并排序
    }
}

// 使用系统调用把子进程排序结果写到临时文件
void write2TempAndPipe(uint* data, uint num_data, int* pipe_w)
{
    char tempPath[] = "/tmp/temp.XXXXXX"; // 临时文件模板，要以XXXXXX结尾

    int tempFileD = mkstemp(tempPath);  // 创建临时文件，返回文件描述符，并会把模板修改成唯一
    if (tempFileD == -1)
    {
        printf("fail to create temp file\n");
        exit(EXIT_FAILURE);
    }

    FILE* tempFile = fdopen(tempFileD, "wb");   // 从文件描述符打开文件
    if (tempFile == NULL)
    {
        fclose(tempFile);
        printf("fail to open temp file\n");
        exit(EXIT_FAILURE);
    }

    fwrite(data, 4, num_data, tempFile);    // 写入
    fclose(tempFile);

    // 把临时文件路径写进管道，送给父进程
    int write_len = write(pipe_w[1], tempPath, strlen(tempPath) + 1);
    if (write_len == -1) 
    {
        printf("fail to write pipe\n");
        exit(EXIT_FAILURE);
    }

    return; 
}

// 单进程内多线程调度
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

// 单线程任务
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

// 为提高io效率，建立缓冲区相关：
// 初始化输入缓冲区（为每个临时文件创建缓冲区）
InputBuffer* init_input_buffers(char** tempPaths, uint num_process, size_t buf_size) {
    InputBuffer* buffers = malloc(num_process * sizeof(InputBuffer));
    if (!buffers) { printf("malloc input buffers failed"); exit(1); }

    for (int i = 0; i < num_process; i++) {
        buffers[i].fp = fopen(tempPaths[i], "rb");
        if (!buffers[i].fp) { printf("fopen temp file failed"); exit(1); }

        // 缓冲区大小：buf_size字节 → 转换为uint数量（每个uint 4字节）
        buffers[i].capacity = buf_size / sizeof(uint);
        buffers[i].data = malloc(buffers[i].capacity * sizeof(uint));
        if (!buffers[i].data) { printf("malloc input buffer data failed"); exit(1); }

        // 预读第一批数据到缓冲区
        buffers[i].count = fread(buffers[i].data, sizeof(uint), buffers[i].capacity, buffers[i].fp);
        buffers[i].pos = 0;  // 从0开始读取
    }
    return buffers;
}

// 初始化输出缓冲区
OutputBuffer* init_output_buffer(char* outputPath, size_t buf_size) {
    OutputBuffer* buf = malloc(sizeof(OutputBuffer));
    if (!buf) { printf("malloc output buffer failed"); exit(1); }

    buf->fp = fopen(outputPath, "wb");  // 二进制或文本模式根据需求定
    if (!buf->fp) { printf("fopen output file failed"); exit(1); }

    // 缓冲区大小：buf_size字节 → 转换为uint数量
    buf->capacity = buf_size / sizeof(uint);
    buf->data = malloc(buf->capacity * sizeof(uint));
    if (!buf->data) { printf("malloc output buffer data failed"); exit(1); }

    buf->pos = 0;  // 从0开始写入
    return buf;
}

// 从输入缓冲区取一个元素（若缓冲区空，自动从文件读新数据）
// 返回1：成功取到元素；返回0：文件已读完
int get_next_from_buffer(InputBuffer* buf, uint* out_val) {
    // 缓冲区还有数据 → 直接取
    if (buf->pos < buf->count) {
        *out_val = buf->data[buf->pos++];
        return 1;
    }

    // 缓冲区空了 → 从文件重新读数据
    buf->count = fread(buf->data, sizeof(uint), buf->capacity, buf->fp);
    buf->pos = 0;

    // 如果读到数据 → 取第一个
    if (buf->count > 0) {
        *out_val = buf->data[buf->pos++];
        return 1;
    }

    // 没读到数据 → 文件已读完
    return 0;
}

// 向输出缓冲区写一个元素（若缓冲区满，自动刷新到文件）
void write_to_buffer(OutputBuffer* buf, uint val) {
    // 缓冲区满了 → 先刷新到磁盘
    if (buf->pos >= buf->capacity) {
        fwrite(buf->data, sizeof(uint), buf->pos, buf->fp);
        buf->pos = 0;  // 重置位置
    }

    // 写入缓冲区
    buf->data[buf->pos++] = val;
}

// 刷新输出缓冲区（将剩余数据写入磁盘）
void flush_output_buffer(OutputBuffer* buf) {
    if (buf->pos > 0) {
        fwrite(buf->data, sizeof(uint), buf->pos, buf->fp);
        buf->pos = 0;
    }
}

// 排序算法：快排和归并
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

uint fastSortSchedule(uint data[], uint l, uint r)  // 线程调用版本
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

void kMergeSort(char** tempPaths, char* outputPath, uint num_process) // 使用最小堆法，最适合多文件合并排序；这里的堆是数据结构，而不是内存里的堆，是完全二叉树
{
    const int BUF_SIZE = 8 * 1024;  // 8KB缓冲区大小

    // 初始化缓冲区
    InputBuffer* input_buffers = init_input_buffers(tempPaths, num_process, BUF_SIZE);
    OutputBuffer* output_buffer = init_output_buffer(outputPath, BUF_SIZE);

    HeapNode heap[num_process]; // 堆初始化
    for (int i = 0; i < num_process; i ++)
    {
        uint val;

        if (get_next_from_buffer(&input_buffers[i], &val)) 
        {
            heap[i].value = val;
            heap[i].idx = i;
            heap[i].eof = 0;
        } 
        else 
        {
            printf("temp file %s is empty\n", tempPaths[i]);
            exit(EXIT_FAILURE);
        }
    }

    // 初始化为最小堆，从最下面开始
    for (int i = num_process / 2 - 1; i >= 0; i --)
    {
        minHeapify(heap, num_process, i);
    }

    while (1)
    {
        // 如果堆顶元素对应的文件已经读到头，则退出（说明所有文件都读完了）
        if (heap[0].eof) break; 

        write_to_buffer(output_buffer, heap[0].value);

        if (!get_next_from_buffer(&input_buffers[heap[0].idx], &heap[0].value))
        {
            heap[0].eof = 1;    // 标记EOF
            // heap[0].value;  // 无意义了现在
        }

        minHeapify(heap, num_process, 0);
    }

    // 把剩下的都写进去，该关的关
    flush_output_buffer(output_buffer);
    fclose(output_buffer->fp);
    free(output_buffer->data);
    free(output_buffer);

    for (int i = 0; i < num_process; i++) 
    {
        fclose(input_buffers[i].fp);
        free(input_buffers[i].data);
    }
    free(input_buffers);

    // 删除创建的临时文件
    for (int i = 0; i < num_process; i++) 
    {
        if (remove(tempPaths[i]) != 0) 
        {
            printf("failed to remove temp file %s\n", tempPaths[i]);
            exit(EXIT_FAILURE);
        }
    }

    return;
}

// 归并使用的辅助函数
void minHeapify(HeapNode heap[], int size, int i)
{
    int left = 2 * i + 1;   // 完全二叉树中，按数组记，i的左右子节点
    int right = 2 * i + 2;
    int smallest = i;

    if (left < size && is_less(&heap[left], &heap[smallest]))
    {
        smallest = left;
    }
    if (right < size && is_less(&heap[right], &heap[smallest]))
    {
        smallest = right;
    }

    if (smallest != i)
    {
        HeapNode temp = heap[i];
        heap[i] = heap[smallest];
        heap[smallest] = temp;

        minHeapify(heap, size, smallest);
    }
}

bool is_less(const HeapNode* a, const HeapNode* b) 
{
    // a 已读完（a是无穷大）→ a 不小于 b
    if (a->eof) return false;
    // b 已读完（b是无穷大）→ a 小于 b
    if (b->eof) return true;
    // 都未读完 → 比较实际 value
    return a->value < b->value;
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