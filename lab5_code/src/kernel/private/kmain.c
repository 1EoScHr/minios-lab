#include <arch/x86.h>
#include <assert.h>
#include <fat32.h>
#include <fs.h>
#include <keyboard.h>
#include <layout.h>
#include <page.h>
#include <process.h>
#include <protect.h>
#include <stdbool.h>
#include <terminal.h>
#include <time.h>
#include <elf.h>
#include <string.h>

static char exectuables[NR_PCBS][FAT32_SHORT_NAME_LEN + 1] = {  // 执行程序列表
    "DELAY   BIN",
    "DELAY   BIN",
    "DELAY   BIN",
};

static int priority_table[NR_PCBS] = {1, 1, 1};  // 预计给每个进程分的事件片

void kernel_main(int partition_lba) {
  term_clear();

  init_fs(partition_lba); // 初始化文件系统

  const u32 cs_sel =
      (SELECTOR_FLAT_C & SA_MASK_RPL & SA_MASK_TI) | SA_TIL | RPL_USER;
  const u32 ds_sel =
      (SELECTOR_FLAT_RW & SA_MASK_RPL & SA_MASK_TI) | SA_TIL | RPL_USER;
  const u32 gs_sel = (SELECTOR_VIDEO & SA_MASK_RPL & SA_MASK_TI) | RPL_USER;

  for (int i = 0; i < NR_PCBS; ++i) {
    process_t *const proc = &pcb_table[i];

    //! 初始化进程段寄存器
    proc->pcb.user_regs.cs = cs_sel;
    proc->pcb.user_regs.ds = ds_sel;
    proc->pcb.user_regs.es = ds_sel;
    proc->pcb.user_regs.fs = ds_sel;
    proc->pcb.user_regs.ss = ds_sel;
    proc->pcb.user_regs.gs = gs_sel;

    //! 为进程分配 cr3 物理内存并映射内核页表
    proc->pcb.cr3 = phy_malloc_4k();
    map_kernel(proc->pcb.cr3);

    /*!
     * 在 map_kernel 之后，就内核程序对应的页表已经被映射了，就可以直接
     * lcr3，与此同时执行流不会触发 page fault
     * 如果不先 map_kernel，执行流会发现执行的代码的线性地址不存在爆出
     * page fault
     * 当然选不选择看个人的想法，评价是都行，各有各的优缺点
     */

    //! 将待执行的可执行文件读入内存
    //! NOTE: 推荐将文件加载到 3GB+48MB 处，应用程序保证不会有 16MB 那么大
    read_file(exectuables[i], (void *)K_PHY2LIN(48 * NUM_1M));  // 物理地址转换为线性地址

    // 模仿loaderkernel.c中的实现，这里不能直接用物理地址，实际用的都必须是虚拟地址
    elf32_hdr_t *eh = (void *)K_PHY2LIN(48 * NUM_1M); // elf文件地址
    elf32_phdr_t *ph = (void *)eh + eh->e_phoff;      // 程序头表地址
    for (int j = 0; j < eh->e_phnum; j++, ph++)       // 加载每个段到用户地址
    {
      if (ph->p_type != 1)  // 如果此段不是可加载程序段，则跳过
      {
        continue;
      }

      // todo("parse the loaded elf and load it to vaddr");
      // 计算当前用户段要用到多少页
      int page_num = (ph->p_memsz + NUM_4K - 1) / NUM_4K; // filesz是实际有数据部分，但是elf段总大小为memsz，其还包括未初始化的bss段
                                                          // 并且这个写法更高效
      // 先连续分配对应页数，要把这里映射到虚地址
      phyaddr_t phy_base = phy_malloc_4k();
      for (size_t k = 1; k < page_num; k ++)
      {
        phy_malloc_4k();
      }

      uintptr_t* pde = (void *)K_PHY2LIN(proc->pcb.cr3); // 获取页目录表的虚拟地址
      for (size_t k = 0; k < page_num; k ++)
      {
        uintptr_t vaddr = ph->p_vaddr + k * NUM_4K;  // 获取此页对应虚拟地址
        phyaddr_t paddr = phy_base + k * NUM_4K; // 获取此页对应物理地址

        uint32_t pde_idx = PDX(vaddr);  // 拆解虚地址为页表项
        uint32_t pte_idx = PTX(vaddr);

        if (!(pde[pde_idx] & PTE_P))  // 如果不存在pde_idx对应的pte表
        {
          phyaddr_t pte_phy = phy_malloc_4k(); // 新分配一页
          pde[pde_idx] = pte_phy | PTE_P | PTE_W | PTE_U;
        }

        uintptr_t *pte = (void *)K_PHY2LIN(PTE_ADDR(pde[pde_idx])); // 获取从pde表中的到的pte项的地址，这是物理地址，得转成虚地址
        pte[pte_idx] = paddr | PTE_P | PTE_W | PTE_U; // 把物理页转成pte项
      }

      void *ELF_data = (void *)(K_PHY2LIN(48 * NUM_1M) + ph->p_offset); // ELF中此程序头信息的位置
      void *phy_data = (void *)K_PHY2LIN(phy_base); // 已经构建好映射关系的地方
      memcpy(phy_data, ELF_data, ph->p_filesz);

      memset((void *)phy_data + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);  // 清零.bss数据段
    }

    proc->pcb.user_regs.eip = eh->e_entry;

    // todo("allocate stack of process in user space");
    uintptr_t stack_vaddr = 0xBFF00000; // 用户栈栈底位置
    uint32_t stack_page_num = 4;
    uint32_t stack_size = stack_page_num * NUM_4K; // 用户栈大小，设为4页

    phyaddr_t stack_phy_base = phy_malloc_4k();
    for (size_t j = 1; j < stack_page_num; j ++)
    {
      phy_malloc_4k();
    }

    uintptr_t* pde = (void *)K_PHY2LIN(proc->pcb.cr3);
    for (size_t j = 0; j < stack_page_num; j ++)
    {
      uintptr_t vaddr = stack_vaddr + j * NUM_4K;
      phyaddr_t paddr = stack_phy_base + j * NUM_4K;

      uint32_t pde_idx = PDX(vaddr);
      uint32_t pte_idx = PTX(vaddr);

      if (!(pde[pde_idx] & PTE_P))  // 如果不存在pde_idx对应的pte表
      {
        phyaddr_t pte_phy = phy_malloc_4k(); // 新分配一页
        pde[pde_idx] = pte_phy | PTE_P | PTE_W | PTE_U;
      }

      uintptr_t *pte = (void *)K_PHY2LIN(PTE_ADDR(pde[pde_idx]));
      pte[pte_idx] = paddr | PTE_P | PTE_W | PTE_U;
    }
    proc->pcb.user_regs.esp = stack_vaddr + stack_size; // 设置用户栈顶


    proc->pcb.user_regs.eflags = EFLAGS_RESERVED | EFLAGS_IF | EFLAGS_IOPL(1);

    /*!
     * 接下来初始化内核寄存器，需要初始化内核寄存器原因是加入了系统调用后，
     * 非常有可能出现系统调用执行过程中插入其余中断的情况，如果跟之前一样所
     * 有进程共享一个内核栈会发生不可想象的结果。
     * 为了避免这种情况，就需要给每个进程分配一个进程栈。
     * 当触发时钟中断发生调度的时候，不再是简单的切换 p_proc_ready，而是需
     * 要将内核栈进行切换，而且需要切换执行流到另一个进程的内核栈。所以需要
     * 一个地方存放当前进程的寄存器上下文。
     * 这是一个富有技巧性的活，深入研究会觉得很妙，如果需要深入了解，可以查
     * 看 schedule 函数了解切换细节。
     */

    u32 *frame = &proc->kernel_stack_bottom[-2];
    //! 保证切换内核栈后执行流进入的是 restart 函数
    frame[0] = (u32)restart;
    //! 这里是因为 restart 要用 pop esp 确认 esp 该往哪里跳
    frame[1] = (u32)&proc->pcb;

    proc->pcb.kernel_regs.esp = (u32)frame;

    proc->pcb.pid = i;
    proc->pcb.priority = priority_table[i];
    proc->pcb.ticks = proc->pcb.priority;

    // proc->pcb.sleep_ticks = 0; // 初始化，不准睡
  }

  p_proc_ready = &pcb_table[0];

  //! 切换进程页表和 tss
  lcr3(p_proc_ready->pcb.cr3);
  tss.esp0 = (u32)(&p_proc_ready->pcb.user_regs + 1);

  init_keyboard();
  init_sysclk();

  kernel_context_t dummy;  //<! only for temporary use
  switch_kernel_context(&dummy, &p_proc_ready->pcb.kernel_regs);

  unreachable();
}
