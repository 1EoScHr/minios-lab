    org     0x7c00              ; 告诉编译器程序加载到 7c00 处

    mov     ax, cs
    mov     ds, ax
    mov     es, ax
    call    DispStr             ; 调用显示字符串例程
    jmp     $                   ; 无限循环

DispStr:
    mov     ax, BootMessage
    mov     bp, ax              ; ES:BP = 串地址

    ; TODO: 参考文档，在此处填充寄存器传参
    mov     cx, 0xF             ; 字符串内字符数量
    mov     bh, 0x0             ; pagenum
    mov     bl, 0x85            ; 10000101，背景灰色，前景紫红
    mov     ah, 0x13            ; 触发中断并显示字符串的固定值
    mov     al, 0x01            ; 00000001，小端，0从右开始，添加一个写后刷新光标

    int     10h                 ; 10h 号中断
    ret

BootMessage:
    db      "Hi OS"

    times   510-($-$$) db 0     ; 填充剩下的空间，使生成的二进制代码恰好为 512 字节
    ;dw      0x55aa              ; 结束标志
    dw      0xaa55
