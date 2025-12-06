#include <assert.h>
#include <layout.h>
#include <wait.h>
#include <process.h>
#include <stdbool.h>
#include <schedule.h>
#include <arch/x86.h>
#include <stdio.h>
#include <sys/errno.h>
#include <malloc.h>

#define	WNOHANG		1

int wait(int *wstatus) {
  //! generally, wait is only allowed to be called from a user program
  assert((void *)wstatus + sizeof(wstatus) < K_PHY2LIN(0));

  /*!
   * 相比于 fork 来说，wait 的实现简单很多
   * 语义实现比较清晰，没有 fork 那么多难点要处理，所以这里并不会给大家太多引导
   * 需要大家自己思考 wait 怎么实现
   * 在实现之前你必须得读一遍文档 man 2 wait 了解到 wait 大概要做什么
   */
  // todo("read the mannual");

  /*!
   * 当然读文档是一方面，最重要的还是代码实现
   * wait 系统调用与 exit 系统调用关系密切，所以在实现 wait 之前需要先读一遍
   * exit 为好，可能读完 exit 的代码你可能知道 wait 该具体做什么了
   */
  // todo("read the impl of exit");

  /*!
   * 接下来就是你自己的实现了，我们在设计的时候这段代码不会有太大问题
   *
   * 在实现完后你任然要对自己来个灵魂拷问
   * - 上锁上了吗？所有临界情况都考虑到了吗？
   *   永远要相信有各种奇奇怪怪的并发问题
   * - 所有错误情况都判断到了吗？错误情况怎么处理？
   *   RTFM -> man 2 wait
   * - 是否所有的资源都正确回收了？
   * - 你写的代码真的符合wait语义吗？
   */
  // todo("impl wait");
  // todo("SOUL TORTURE");

  // 查了一下，wstatus是用来接收子进程退出状态信息
  // wait没有关于调用者的参数，需要自己来获取，或者说谁调用的，谁挂起等着
  process_t *current = p_proc_ready;
  pcb_t *pcb = &current->pcb;
  while (true) {
    // 尝试获取父锁
    while (xchg(&pcb->lock, 1) == 1) {
      schedule();
    }

    // 检查是否有子进程
    if (pcb->ptree.sons == NULL) {
      xchg(&pcb->lock, 0);
      return -ECHILD; // 没有子进程，wait 失败
    }

    // 遍历子进程，找僵尸
    son_node_t **prev = &pcb->ptree.sons;
    son_node_t *node = pcb->ptree.sons;

    int found = 0;
    int pid = -1;
    int status = 0;

    while (node) {
      pcb_t *child = node->son;

      // 锁子进程（按父→子顺序）
      while (xchg(&child->lock, 1) == 1) {
        schedule();
      }

      if (child->status == PSTATUS_ZOMBIE) {
        // 回收僵尸
        pid = child->pid;
        status = child->exit_code;

        // 从链表移除
        son_node_t *next = node->next;

        if (node->prev) {
          node->prev->next = node->next;
        } else {
          pcb->ptree.sons = node->next;
        }
        if (node->next) {
          node->next->prev = node->prev;
        }

        /*
        *prev = node->next;

        node->son = NULL;
        node->next = NULL;
        node->prev = NULL;
        */

        kfree(node);

        child->status = PSTATUS_IDLE; // 标记为空闲
        xchg(&child->lock, 0);
        found = 1;
        break;
      }

      xchg(&child->lock, 0);
      prev = &node->next;
      node = node->next;
    }

    if (found) {
      xchg(&pcb->lock, 0);
      if (wstatus) {
        *wstatus = status;
      }

      return pid;
    }

    // 没有僵尸，但有活着的子进程 → 睡眠
    pcb->status = PSTATUS_SLEEPING;
    xchg(&pcb->lock, 0);
    // 调度出去，等待子进程 exit 唤醒（exit 会设 status = READY）
    schedule();
  }
}
