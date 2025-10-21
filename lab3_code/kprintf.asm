[SECTION .text]
[BITS 32]

DispChar:
    mov     ah, [DefaultColor]
    mov     word [gs:ecx * 2], ax
    inc     ecx         ; 光标右移
    inc     edx         ; 一个字符一字节

    ret

; 默认样式：黑底白字
DefaultColor:
    db      0x0

; 保存因结构体数据而装不下的临时数据
ecxBuffer:
    dd      0x0

    global kprintf  ; 把kprintf定义为一个全局符号
kprintf:

    ; TODO: impl your code here
    ; NOTE: see terminal.h for the func proto of the method
    ; NOTE: see lab guide step 3 for more details
    ; NOTE: remove the following lines as long as you've finished your impl

    ; 使每次调用时提供默认颜色
    mov     al, 0x0f
    mov     [DefaultColor], al

    ; 栈按4字节对齐
    ; 第0个参数为输出起始位置，是一个u16
    mov     ecx, [esp + 4]
    ; 第1个参数是字符串起始位置，是一个指针，应该是u32
    mov     edx, [esp + 8]
    mov     ebx, 8

.readStr:
    mov     al, [edx]   ; 从[dx]处搬一个byte
    cmp     al, '%'     ; 看是否为特定模式字符
    je      .spicalPattern

    cmp     al, 0       ; 看是否为结束标志
    je      .ret

    call    DispChar    ; 都不是的话就直接打印
    jmp     .readStr

.spicalPattern:
    inc     edx         ; 分析下一个字符
    mov     al, [edx]   ; 读取

    cmp     al, 'c'     
    je      .c      

    cmp     al, 'f'
    je      .f

    cmp     al, 'b'
    je      .b

    cmp     al, 's'
    je      .s

    jmp     $           ; 一个都对应不上，那么就卡死

; 为c的话，对应传入变量是一个char
.c:
    add     ebx, 4

    mov     al, [esp + ebx]
    call    DispChar
    jmp     .readStr

; 为f或b的话，对应传入变量是一个4bit，但是栈按4字节对齐
.f:
    add     ebx, 4

    ; 假如原颜色是默认白底黑字 00001111，现在变成红色前景 0100
    ; al:00000100, bl:00001111
    ; bl:00000000
    ; bl:00000100

    mov     al, [esp + ebx] ; 获取前景颜色
    push    ebx
    mov     bl, [DefaultColor]
    and     bl, 0xf0    ; 保留背景颜色
    or      bl, al      ; 追加前景颜色
    mov     [DefaultColor], bl
    pop     ebx

    inc     ecx         ; 光标右移
    inc     edx         ; 一个字符一字节

    jmp     .readStr

.b:
    add     ebx, 4

    ; 假如原颜色是默认白底黑字 00001111，现在变成红色背景 0100
    ; al:00000100, bl:00001111
    ; al:01000000
    ; bl:00001111
    ; bl:01001111

    mov     al, [esp + ebx] ; 获取背景颜色
    shl     al, 4           ; 变为真正的背景颜色
    push    ebx
    mov     bl, [DefaultColor]
    and     bl, 0x0f        ; 保留前景颜色
    or      bl, al          ; 追加背景颜色
    mov     [DefaultColor], bl
    pop     ebx

    inc     ecx             ; 光标右移
    inc     edx             ; 一个字符一字节
    
    jmp     .readStr

; 值传递，那么就涉及栈对齐、结构体内部对齐问题
; 这里也是debug了半天才发现理解错误
; 对于栈，esp+0是esp+[0,3]，esp+4是esp+[4,7]等
; 而这个结构体塞的很满，前景、背景、ASCLL、pos低、pos高
; 分别是esp+x+4/5/6/7/8

; 0:fg
; 1:bg
; 2:code
; 3:pos0
; 4:pos1

.s:
    mov     [ecxBuffer], ecx
    xor     eax, eax        ; 高效置0
    xor     ecx, ecx

    add     ebx, 4
    mov     al, [esp + ebx]
    mov     ah, [esp + ebx + 1]
    shl     ah, 4
    or      ah, al
    mov     al, [esp + ebx + 2]

    add     ebx, 4
    mov     cx, [esp + ebx]
    mov     word [gs:ecx * 2], ax
    mov     ecx, [ecxBuffer]

    inc     ecx         ; 光标右移
    inc     edx         ; 一个字符一字节

    jmp     .readStr

.ret:
    ret

