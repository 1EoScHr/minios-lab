/*
 * Desc: Thread Switching Emulation x64
 * Author: Good
 * Date: 2023-11-02
 */

.globl switch_context
.globl restart_restore

switch_context:
        pushq   %rbp
        movq    %rsp, %rbp # already rip,rbp instack
        pushfq
        sub     $0x30, %rsp # six register to save
        movq    %rsp, 0x00(%rsp)
        movq    %r15, 0x08(%rsp)
        movq    %r14, 0x10(%rsp)
        movq    %r13, 0x18(%rsp)
        movq    %r12, 0x20(%rsp)
        movq    %rbx, 0x28(%rsp)

        call    scheduler

        call    get_cur_kernel_stack_end
        sub     $72, %rax       # thread_table[cur].tcb.kernel_stack_end - REGS_START_OFFSET
        
        movq    %rax, %rsp
        movq    0x08(%rax), %r15
        movq    0x10(%rax), %r14
        movq    0x18(%rax), %r13
        movq    0x20(%rax), %r12
        movq    0x28(%rax), %rbx
        add     $0x30, %rsp
        popfq
        # still rbp, rip in stack
        call    get_cur_kernel_stack_end
        pushq   %rax
        call    get_cur_user_rsp_save
        popq    %rdi    # kernel_stack_end
        movq    %rax, %rsp # user saved rsp 
        jmp     *-0x8(%rdi)

restart_restore:
        pushq   %rbp
        movq    %rsp, %rbp
        call    get_cur_kernel_stack_end
        sub     $72, %rax       # thread_table[cur].tcb.kernel_stack_end - REGS_START_OFFSET
        
        movq    %rax, %rsp
        movq    0x08(%rax), %r15
        movq    0x10(%rax), %r14
        movq    0x18(%rax), %r13
        movq    0x20(%rax), %r12
        movq    0x28(%rax), %rbx
        add     $0x30, %rsp
        popfq
        # still rbp, rip in stack
        call    get_cur_kernel_stack_end
        pushq   %rax
        call    get_cur_user_rsp_save
        popq    %rdi    # kernel_stack_end
        movq    %rax, %rsp # user saved rsp
        jmp     *-0x8(%rdi)
