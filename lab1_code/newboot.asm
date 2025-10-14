    org     0x7c00

    mov     ax, cs
    mov     ds, ax
    mov     es, ax

    call    ClearScreen

    mov     dh, 0x13            ; 19行
    mov     dl, 0x26            ; 38列
    call    SetCursor

    call    DispStr             ; 显示字符串

    mov     dh, 0x0A            ; 10行
    mov     dh, 0x0A            ; 10列
    call    SetCursor

    jmp     $                   ; 无限循环

ClearScreen:
    mov     ah, 0x06            ; 中断标志
    mov     al, 0x00            ; 为0,表示清屏
    mov     bh, 0x00            ; 新区域的填充样式
    mov     ch, 0x00            ; 左上角所在行
    mov     cl, 0x00            ;      所在列
    mov     dh, 0x19            ; 右下角所在行，25=19h
    mov     dl, 0x50            ;      所在列，80=50h

    int     10h
    ret

SetCursor:
; 记得把dh设成行，dl设成列
    mov     ah, 0x02            ; 中断标志
    mov     bh, 0x00            ; 页号

    int     10h
    ret

DispStr:
    mov     ax, BootMessage
    mov     bp, ax              ; ES:BP = 串地址

    mov     ah, 0x13            ; 中断标志
    mov     al, 0x01            ; 边写边动光标
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    mov     cx, 0x04            ; 字符串内字符数量

    int     10h                 ; 10h 号中断
    ret

BootMessage:
    db      "NWPU"

    times   510-($-$$) db 0     ; 填充剩下的空间，使生成的二进制代码恰好为 512 字节
    dw      0xaa55              ; 结束标志
