#include <assert.h>
#include <interrupt.h>
#include <keymap.h>
#include <stdint.h>
#include <stdio.h>

#include <arch/x86.h>
#include <keyboard.h>

#define KB_INBUF_SIZE 4

typedef struct kb_inbuf {
  u8* p_head; // 指向缓冲区下一个空闲位置
  u8* p_tail; // 指向键盘任务应该处理的字节
  int count;  // 缓冲区中一共有多少字节
  u8 buf[KB_INBUF_SIZE];  // 缓冲区
} kb_inbuf_t;

static kb_inbuf_t KB_INPUT; // 不用初始化了

void init_keyboard() {
  KB_INPUT.p_head = KB_INPUT.buf,
  KB_INPUT.p_tail = KB_INPUT.buf,
  KB_INPUT.count = 0,
  //unimplemented("keyboard irq handler");
  put_irq_handler(KEYBOARD_IRQ, keyboard_handler);
  enable_irq(KEYBOARD_IRQ);
}

void kb_put_key(u8 ch)
{
  if (KB_INPUT.count < KB_INBUF_SIZE)
  {
    *(KB_INPUT.p_head) = ch;
    KB_INPUT.p_head ++; // 把kb_code加入缓冲区

    if (KB_INPUT.p_head == KB_INPUT.buf + KB_INBUF_SIZE)  // 应该是防止野指针，同时保证是一轮轮遍历缓冲区的
    {
      KB_INPUT.p_head = KB_INPUT.buf;
    }

    KB_INPUT.count ++;
  }
}

char getch()
{
  char ch_ret = 0xff;

  if (KB_INPUT.count > 0)
  {
    //disable_int(); // 关闭中断，保证原子性
    ch_ret = *(KB_INPUT.p_tail);
    KB_INPUT.p_tail ++;
  
    if (KB_INPUT.p_tail == KB_INPUT.buf + KB_INBUF_SIZE)  // 应该是防止野指针，同时保证是一轮轮遍历缓冲区的
    {
      KB_INPUT.p_tail = KB_INPUT.buf;
    }
    KB_INPUT.count --;
    //enable_int(); // 恢复中断
  }

  return ch_ret;
}

void keyboard_handler(int irq) {
  u8 code = inb(KB_PORT);
  if (code & FLAG_BREAK)  // 如果弹起，则忽略
  {
    return;
  }
  u8 ch = keymap[code & 0x7F];

  if (ch >= 'a' && ch <= 'z')
  {
    kb_put_key(ch);
    kprintf("%c", ch);
  }
}
