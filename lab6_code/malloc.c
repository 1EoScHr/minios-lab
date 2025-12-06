#include <arch/x86.h>
#include <assert.h>
#include <layout.h>
#include <malloc.h>
#include <page.h>
#include <schedule.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct memblk_info {
  struct memblk_info *ptr;
  size_t size;  //<! in a unit of sizeof(memblk_info_t)
} memblk_info_t;

// 自定义一个空闲物理页
// 这个结构不行，pass
/*
typedef struct freephy_info {
  struct freephy_info *next;
  phyaddr_t addr;
} freephy_info_t;

static freephy_info_t *PHY_FREE_PAGE; // 照猫画虎
*/
// 换一个逻辑
static phyaddr_t PHY_FREE_PAGE; // 空闲页的物理地址

static u32 PHY_MALLOC_LOCK = 0;
static u32 KMALLOC_LOCK = 0;

static memblk_info_t *KMEM_FREE_SLOT;

/*!
 * \brief allocate a physical page of 4KB size
 * \note 96~128MB is reserved for page allocation
 */
phyaddr_t phy_malloc_4k() {
  static phyaddr_t FREE_MEM_PTR = 96 * NUM_1M;

  while (xchg(&PHY_MALLOC_LOCK, 1) == 1) {
    schedule();
  }

  phyaddr_t addr;
  // assert(FREE_MEM_PTR < 128 * NUM_1M);
  if (FREE_MEM_PTR >= 128 * NUM_1M)
  {
    assert(PHY_FREE_PAGE); // 确保有空闲页
    addr = PHY_FREE_PAGE;
    PHY_FREE_PAGE =  *(phyaddr_t *)K_PHY2LIN(PHY_FREE_PAGE);
  }
  else  // 如果默认的一遍已经满了，就从空闲这里取
  {
    addr = FREE_MEM_PTR;
    FREE_MEM_PTR += PGSIZE;
  }

  xchg(&PHY_MALLOC_LOCK, 0);

  return addr;
}

void phy_free_4k(phyaddr_t phy_addr) {
  assert(phy_addr % PGSIZE == 0);
  // unimplemented();

  // 先加锁，申请和释放都是基于FREE_MEM_PTR
  while (xchg(&PHY_MALLOC_LOCK, 1) == 1) {
    schedule();
  }

  // freephy_info_t* freepage = (freephy_info_t *)kmalloc(sizeof(freephy_info_t));
  // freepage->next = PHY_FREE_PAGE;
  // freepage->addr = phy_addr;
  // PHY_FREE_PAGE = freepage;

  // 在这个要被释放的页起点处加上下一个空闲页的物理地址，也是头插法，无处不在
      // 下面这里这个语义关键，物理地址转换后，得到一个类型为void*，实际上是一个虚拟内存地址，
      // 但为什么又用(phyaddr_t *)变成物理地址？是因为想要让这个地址开始的几个字节装物理地址，所以才这么写，并非是脑子抽了！
  *(phyaddr_t *)(K_PHY2LIN(phy_addr)) = PHY_FREE_PAGE;
  PHY_FREE_PAGE = phy_addr;

  xchg(&PHY_MALLOC_LOCK, 0);
  return;
}

/*!
 * \brief allocate memory in kernel space
 * \note 64~96MB is reserved for page allocation
 */
void *kmalloc(size_t n) {
  void *ret_ptr = NULL;

  while (xchg(&KMALLOC_LOCK, 1) == 1) {
    schedule();
  }

  memblk_info_t *prev = KMEM_FREE_SLOT;
  if (prev == NULL) {
    KMEM_FREE_SLOT = (void *)K_PHY2LIN(64 * NUM_1M);
    KMEM_FREE_SLOT->ptr = KMEM_FREE_SLOT;
    KMEM_FREE_SLOT->size = ((96 - 64) * NUM_1M) / sizeof(memblk_info_t);
    prev = KMEM_FREE_SLOT;
  }

  memblk_info_t *p = prev->ptr;
  const size_t total_units =
      (n + sizeof(memblk_info_t) - 1) / sizeof(memblk_info_t) + 1;

  while (true) {
    if (p->size >= total_units) {
      if (p->size == total_units) {
        prev->ptr = p->ptr;
      } else {
        p->size -= total_units;
        p += p->size;
        p->size = total_units;
      }
      KMEM_FREE_SLOT = prev;
      ret_ptr = (void *)(p + 1);
      break;
    }
    if (p == KMEM_FREE_SLOT) {
      panic("falied to malloc");
    }
    prev = p;
    p = p->ptr;
  }

  xchg(&KMALLOC_LOCK, 0);
  return ret_ptr;
}

void kfree(void *ptr) {
  memblk_info_t *bp = (memblk_info_t *)ptr - 1;

  while (xchg(&KMALLOC_LOCK, 1) == 1) {
    schedule();
  }

  memblk_info_t *p = KMEM_FREE_SLOT;
  while (!(bp > p && bp < p->ptr)) {
    if (p >= p->ptr && (bp > p || bp < p->ptr)) {
      break;
    }
    p = p->ptr;
  }
  if (bp + bp->size == p->ptr) {
    bp->size += p->ptr->size;
    bp->ptr = p->ptr->ptr;
  } else {
    bp->ptr = p->ptr;
  }
  if (p + p->size == bp) {
    p->size += bp->size;
    p->ptr = bp->ptr;
  } else {
    p->ptr = bp;
  }
  KMEM_FREE_SLOT = p;

  xchg(&KMALLOC_LOCK, 0);
}
