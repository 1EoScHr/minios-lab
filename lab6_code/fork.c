#include <assert.h>
#include <fork.h>
#include <process.h>
#include <arch/x86.h>
#include <schedule.h>
#include <page.h>
#include <string.h>
#include <sys/errno.h>
#include <exit.h>
#include <interrupt.h>
#include <elf.h>
#include <layout.h>
#include <malloc.h>
#include <stdio.h>

pid_t fork(process_t *father) {
  //! NOTE: set father to current process if father is null

  father = father ? father : p_proc_ready;

  /*!
   * 这应该是你第一次实现一个比较完整的功能
   * 你可能会比较急，但是你先别急，先别想自己要实现一个这么大的东西而毫无思路
   * 这样你在焦虑的同时也在浪费时间，就跟你在实验五中被页表折磨一样
   * 人在触碰到未知的时候总是害怕的，这是天性，所以请你先冷静下来
   * fork 的系统调用会一步步引导你写出来，不会让你本科造火箭的
   *
   * 推荐是边写边想，而不是想一箩筐再 ONE SHOT，这样非常容易计划赶不上变化
   */
  // todo("NO PANIC OR SCARED, JUST CALM DOWN AND THINK");

  // kprintf("begin fork\n");

  /*!
   * 1. 找一个空闲的进程作为你要 fork 为的子进程
   */
  // todo("find an idle process");
  process_t* free_process = (void *)0;  // 空闲进程默认为空指针
  for (size_t i = 0; i < NR_PCBS; i ++)
  {
    pcb_t* p = &pcb_table[i].pcb;

    if (xchg(&p->lock, 1) == 1) // 到手的先锁上，以防同时访问一个
    {
      continue;
    }

    // 这时候就真正抢到了
    if (p->status == PSTATUS_IDLE)  // pcb结构体内的status有一个枚举，注明了process的状态
    {
      // 将空闲进程标记，正在初始化
      p->status = PSTATUS_INITING;
      p->pid = i;
      free_process = &pcb_table[i];
      // xchg(&p->lock, 0); // 放开
      break;
    }

    // 否则用不了，放开，继续
    xchg(&p->lock, 0);
  }

  if (free_process == 0)
  {
    // kprintf("fail to find free proc\n");
    return -EAGAIN;
  }

  /*!
   * 2. 清晰地理解 pcb_t 中个成员的语义，明确各个成员在何处用到，如何使用
   *
   * exec 和 exit 的代码能够帮助你对 pcb 的理解，不先理解好 pcb 你 fork
   * 是无从下手的
   */
  // panic("make insight of pcb");

  /*!
   * 3. 初始化子进程 pcb
   *
   * 在阅读完 pcb 之后终于可以开始 fork 工作了，本质上相当于将父进程的 pcb
   * 内容复制到子进程 pcb 中，但是你需要想清楚，哪些应该复制到子进程，哪些不
   * 应该复制，哪些应该子进程自己初始化
   *
   * **难点**
   *
   * - 子进程 fork 的返回值怎么处理？
   *   这需要你对系统调用整个过程都掌握比较清楚，如果非常清晰这个问题不会很大
   * - 子进程内存如何复制？
   *   别傻乎乎地复制父进程的 cr3，本质上相当于与父进程共享同一块内存，而共享
   *   内存肯定不符合 fork 的语义，这样一个进程写内存某块地方会影响到另一个进
   *   程，这个东西需要你自己思考如何复制父进程的内存
   * - 在 fork 结束后，最终总会调度到子进程，怎么保证子进程能够正常进入用户态？
   *   你肯定会觉得这个问题问的莫名其妙的，只能说你如果遇到那一块问题了就会体会
   *   到这个问题的重要性，这需要你对调度整个过程都掌握比较清楚
   */
  // panic("copy and re-init pcb");

  pcb_t *fat_pcb = &(father->pcb);
  pcb_t *son_pcb = &(free_process->pcb);

  uintptr_t father_cr3 = fat_pcb->cr3;
  page_node_t *father_pages = fat_pcb->pages;
  stack_frame_t father_uregs = fat_pcb->user_regs;
  int father_prio = fat_pcb->priority;

  // xchg(&son_pcb->lock, 0);

  // 根据手册规定，先给老的上锁，再给小的上锁
  /*
  while (xchg(&fat_pcb->lock, 1) == 1) {
    schedule();
  }

  while (xchg(&son_pcb->lock, 1) == 1) {
    schedule();
  }
  */

  // 计算、复制内核栈
  /*
  uintptr_t father_stack_top = (uintptr_t)father + KERNEL_STACK_SIZE; // 真正的高地址栈顶
  uintptr_t son_stack_top    = (uintptr_t)free_process + KERNEL_STACK_SIZE;

  size_t esp_offset = father_stack_top - fat_pcb->kernel_regs.esp;
  size_t ebp_offset = father_stack_top - fat_pcb->kernel_regs.ebp;

  uintptr_t son_esp = son_stack_top - esp_offset;
  uintptr_t son_ebp = son_stack_top - ebp_offset;

  // 复制栈内容
  memcpy((void*)son_esp, (void*)fat_pcb->kernel_regs.esp, esp_offset);

  // 更新子进程寄存器
  son_pcb->kernel_regs = fat_pcb->kernel_regs;
  son_pcb->kernel_regs.esp = son_esp;
  son_pcb->kernel_regs.ebp = son_ebp;
  */

  memset(&son_pcb->kernel_regs, 0, sizeof(son_pcb->kernel_regs)); // 清零
  u32 *frame = &free_process->kernel_stack_bottom[-2];
  frame[0] = (u32)restart;
  frame[1] = (u32)son_pcb;
  son_pcb->kernel_regs.esp = (u32)frame;

  son_pcb->user_regs = father_uregs;  // fork复制状态机信息之一：用户寄存器
  son_pcb->priority = father_prio;
  son_pcb->ticks = father_prio;

  // 初始化函数，参考exec.c
  // init_pagetbl(son_pcb); // 不能用！其中有lcr3，会立刻改变上下文，cpu使用的页表要保持是父进程的，可以魔改一下
  init_pagetbl_no_switch(son_pcb);
  // map_stack(son_pcb); // 这里调用的话，会把栈空间分配了，但是是空的，直接在后面复制cr3时候搞就行
  // map_elf(son_pcb, elf_addr); // 这里的elf_addr并不知道，但父进程已经把其塞到自己的页表里，那么复制其整个页表即可

  // 思路：遍历父进程的cr3，若分配，则同步的分配给子进程，同时往下级子进程去
  // 父子进程各自的虚地址也完全一致，因此把虚地址解算为页表地址的过程也要被考虑
  uintptr_t *fat_pde_ptr = (uintptr_t *)K_PHY2LIN(father_cr3);
  uintptr_t *son_pde_ptr = (uintptr_t *)K_PHY2LIN(son_pcb->cr3);

  for (size_t i = 0; i < 768; i ++) // 0-767对应0-3G虚拟内存，是用户空间；而768-1023对应3G-4G，是内核空间，初始化时已经映射过了
  {
    if ((fat_pde_ptr[i] & PTE_P) == 0) continue;  // 若父进程此页未分配，那么跳过

    // 父进程此页分配，需要给子进程配一个pte页，参考page.c
    // 不能用alloc_phy_page，要用主动暴露出的api
    // phyaddr_t son_pte_phy = alloc_phy_page(&son_pcb->pages);
    // memset((void *)K_PHY2LIN(son_pte_phy), 0, PGSIZE);
    // son_pde_ptr[i] = (son_pte_phy & ~0xfff) | (fat_pde_ptr[i] & 0xfff); // 标志位与父进程一致，但物理地址是自己的
    // uintptr_t *son_pte_ptr = (uintptr_t *)K_PHY2LIN(son_pte_phy);

    phyaddr_t fat_pte_phy = PTE_ADDR(fat_pde_ptr[i]); // 父进程对应pde项、pte表地址获取
    uintptr_t *fat_pte_ptr = (uintptr_t *)K_PHY2LIN(fat_pte_phy);
    u32 pde_attr = fat_pde_ptr[i] & 0xfff;

    for (size_t j = 0; j < NPTENTRIES; j ++)
    {
      if ((fat_pte_ptr[j] & PTE_P) == 0) continue; // 同理

      phyaddr_t fat_page_phy = PTE_ADDR(fat_pte_ptr[j]);  // 父页物理地址
      u32 pte_attr = fat_pte_ptr[j] & 0xfff;

      // kprintf("before alloc page page: %d\n", j);
      // kprintf("after alloc page page: %d\n", j);
      // phyaddr_t son_page_phy = alloc_phy_page(&son_pcb->pages); // 新分配的子页物理地址
      // son_pcb->pages->addr = // 暂不知这里填什么
      // memcpy((void *)K_PHY2LIN(son_page_phy), (void *)K_PHY2LIN(fat_page_phy), PGSIZE); // 直接整页复制

      phyaddr_t son_page_phy = map_laddr(son_pcb->cr3, &son_pcb->pages, (uintptr_t)PGADDR(i, j, 0), 0xFFFFFFFF, pde_attr, pte_attr);  // 自动分配一块物理内存给son，虚地址是一致的
      if (son_page_phy == 0) {
        son_pcb->status = PSTATUS_IDLE;
        xchg(&son_pcb->lock, 0);
        xchg(&fat_pcb->lock, 0);
        return -ENOMEM;
      }
      memcpy((void *)K_PHY2LIN(son_page_phy), (void *)K_PHY2LIN(fat_page_phy), PGSIZE);
    }
  }

  // 返回值的设置，父进程这里直接return，子进程手动写到eax
  // fat_pcb->user_regs.eax = ret;
  son_pcb->user_regs.eax = 0;

  /*!
   * 4. 维护进程树，添加该父子进程关系
   */
  // todo("maintain process tree");

  xchg(&son_pcb->lock, 0);
  while (xchg(&fat_pcb->lock, 1) == 1) {
    schedule();
  }

  while (xchg(&son_pcb->lock, 1) == 1) {
    schedule();
  }

  son_pcb->ptree.fa = fat_pcb;
  son_pcb->ptree.sons = NULL; // 新儿子自己暂时还没有儿子们

  son_node_t* new_node = kmalloc(sizeof(son_node_t));
  // assert(new_node->son == NULL);
  if (!new_node)
  {
    son_pcb->status = PSTATUS_IDLE;
    xchg(&son_pcb->lock, 0);
    xchg(&fat_pcb->lock, 0);
    return -ENOMEM; // 错误处理
  }

  new_node->son = son_pcb;
  new_node->next = fat_pcb->ptree.sons;
  new_node->prev = NULL;
  if (fat_pcb->ptree.sons) fat_pcb->ptree.sons->prev = new_node;
  fat_pcb->ptree.sons = new_node;

  /*!
   * 5. 将子进程的状态置为 READY，说明 fork 已经完成，子进程准备就绪
   */
  // todo("mark as ready");
  son_pcb->status = PSTATUS_READY;
  pid_t ret = son_pcb->pid;

  /*!
   * 6. 别急着跑，先自我灵魂拷问一下
   *
   * - 上锁上了吗？所有临界情况都考虑到了吗？
   *   永远要相信有各种奇奇怪怪的并发问题
   * - 所有错误情况都判断到了吗？错误情况怎么处理？
   *   RTFM -> man 2 fork
   * - 你写的代码真的符合 fork 语义吗？
   */
  // panic("SOUL TORTURE");

  // 解锁，先子再父
  xchg(&son_pcb->lock, 0);
  xchg(&fat_pcb->lock, 0);

/*
kprintf("father pcb=%p, base=%p, top=%p, esp=0x%x, ebp=0x%x\n",
        father, (void*)father, father->kernel_stack_bottom,
        fat_pcb->kernel_regs.esp, fat_pcb->kernel_regs.ebp);

kprintf("son    pcb=%p, base=%p, top=%p, esp=0x%x, ebp=0x%x\n",
        free_process, (void*)free_process, free_process->kernel_stack_bottom,
        son_pcb->kernel_regs.esp, son_pcb->kernel_regs.ebp);

  kprintf("fork done\n");
*/

  return ret;
}
