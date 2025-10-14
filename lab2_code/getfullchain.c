#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// 最大簇号，也就是4096
#define MAXCLUS 0x00001000

// FAT2从0x8000到0xBFFF，一共0x4000，也可以按行来看，也就是共0x400行，每行4个簇号，那么一共4096簇
// 每个簇号为32位，可以用uint放下
typedef unsigned int uint;
uint FAT2[4096];    // 存放4096个FAT项
bool FAT2use[4096]; // 查询对应FAT项是否可用


// 原始输入的簇号信息
struct originCluster
{
    uint ll;    // low low
    uint lh;    // low high
    uint hl;    // …
    uint hh;
};

uint trans2Normal(struct originCluster cluster)
{
    return (cluster.hh << 24 | cluster.hl << 16 | cluster.lh << 8 | cluster.ll);
}

// 根据FAT初始化FATuse表
bool FAT2use_Init(uint cluster)
{
    if(cluster > 0x00000000 && cluster <= MAXCLUS) return true; // 如果是已分配的簇

    if(cluster >= 0x0FFFFFF8) return true;  // 如果是EOF簇

    return false;   // 否则都是不可达（当然这里也可以不写，因为全局变量初始化默认为0）
}

// 把输入的原始FAT项转换为本程序内使用的格式
void FAT2_Init(void)
{
    struct originCluster cluster;

    for(int i = 0; i < 4096; i ++)
    {
        // 搜索出来scanf能通过%x来强行读取十六进制数
        // 输入格式类似"7856 3412"，要把其转换为0x12345678
        scanf("%2x%2x %2x%2x", &cluster.ll, &cluster.lh, &cluster.hl, &cluster.hh);
        FAT2[i] = trans2Normal(cluster);

        FAT2use[i] = FAT2use_Init(FAT2[i]); // 同步配置FAT2use表
    }

    FAT2use[0] = false; // 0簇、1簇为保留簇，不可达
    FAT2use[1] = false;

    return;
}

// 定义要用到的链表结构
struct FAT  // FAT项
{
    unsigned int cluster;   // 当前簇号
    struct FAT *nextcluster;// 下一簇对应的FAT
};
typedef struct FAT FAT;

typedef FAT* ClusterChain;  // 簇链

ClusterChain cc[1000];      // 定义1000个，不知够不够，可以加一个错误是不够时退出
                            // 事实上，最后只会有两个簇链，但是由于相互交错，中间可能会有许多临时簇链
bool ccuse[1000];           // 和FATuse类似，标记其是否为真正的有效簇链，而不是临时簇链

// 创建一个新簇链
ClusterChain createClusterChain(uint fstCluster)
{
    ClusterChain newcc = malloc(sizeof(FAT));   // 手动分配内存
    newcc->cluster = fstCluster;
    newcc->nextcluster = NULL;

    FAT2use[fstCluster] = false;                // 把初始簇标为不可达

    return newcc;
}

// 创建新的FAT对象
FAT* createFAT(uint cluster)
{
    FAT* newFAT = malloc(sizeof(FAT));
    newFAT->cluster = cluster;
    newFAT->nextcluster = NULL;

    return newFAT;
}

// 下面的extend应该要用到合并
void mergeClusterChain(FAT* p, int ccind)
{
    for(int i = 0; i < ccind; i ++) // 遍历前面的所有簇链，看是谁的头
    {
        if(!ccuse[i] || cc[i]->cluster != p->cluster) continue; // 如果已经是子链，那就跳过

        // 把当前簇链与分簇链合并，并将被合并的簇链标为不可达
        p->nextcluster = cc[i]->nextcluster;
        ccuse[i] = false;

        break;
    }

    return;
}

// 根据FAT表拓展簇链
void extendClusterChain(int ccind)
{
    FAT* p = cc[ccind];

    while(1)
    {
        p->nextcluster = createFAT(FAT2[p->cluster]);
        p = p->nextcluster;

        if(p->cluster >= 0x0FFFFFF8) break; // 到簇链尽头则退出循环，这一判断要放在这里，因为跑下去会段错误

        if(FAT2use[p->cluster] == false) //当簇链的下一项不可达，说明该项是另一个簇链的头，需要合并
        {
            mergeClusterChain(p, ccind);
            break;  // 合并后一定有尾
        }

        FAT2use[p->cluster] = false;    // 没有别人占，那就自己占，标为不可达
    }

    return;
}

void showClusterChain(int ccind)
{
    for (int i = 0; i < ccind; i++)
    {
        if(ccuse[i])
        {
            printf("start cluster: %x\n", cc[i]->cluster);
        }
    }

    return;
}

void releseMemory(void)
{

}

int main()
{
    FAT2_Init();
    int ccind = 0;          // 当前总簇链数，也是下一簇链的索引


    for(uint i = 0; i < 4096; i ++)
    {
        if(!FAT2use[i])     // 如果当前簇不可用，则跳过
        {
            continue;
        }

        cc[ccind] = createClusterChain(i);      // 以当前簇为起点，创造一个簇链
                                                // 注意i与FAT[i]是不一样的
        ccuse[ccind] = true;                    // 刚初始化，是有效的

        extendClusterChain(ccind);              // 往下扩展/合并当前簇链

        ccind ++;   // 分配下一个簇号
    }

    // 按理说，到这里时应该就只有两条簇链是有效簇链，在这里显示有关信息
    showClusterChain(ccind);

    // 按理说在结尾还得加个释放内存，但其实不用也行吧……占个空
    releseMemory();
}
