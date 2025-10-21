%include "layout.inc"
%include "mbr.inc"
%include "packet.inc"
%include "msg_helper.inc"

    org     OffsetOfMBR
Start:
    ; init seg reg
    ; 初始化段寄存器
    mov     ax, cs
    mov     ds, ax
    mov     es, ax
    mov     ss, ax

    ; copy self to BaseOfMBR:OffsetOfMBR since 0x7c00 will be overwritten by
    ; boot sector later
    ; 复制CX个字节，从DS:SI到ES:DI
    ; 这里的语义是复制一个扇区的字节，从现在默认加载的BaseBoot:OffsetBoot到应该在的BaseMBR:OffsetMBR
    cld
    mov     cx, SizeOfSector
    mov     si, OffsetOfBoot
    mov     di, OffsetOfMBR
    rep movsb

    jmp     BaseOfMBR:Init

; variales
DriverNumber db 0 ; driver number

; declare messages and introduce ShowMessage
MSG_BEGIN 11
    NewMessage FindPart,     "Find Part  "
    NewMessage Ready,        "Ready      "
    NewMessage FailedToRead, "Read Fail  "
    NewMessage NoBootable,   "No Bootable"
MSG_END

; \brief read sectors from disk and write to the buffer
; \param [in] eax start sector index
; \param [in] cx total sectors to read
; \param [out] es:bx buffer address
ReadSector:
    pushad
    sub     sp, SizeOfPacket    ; allocate packet on stack

    ; now si points to packet and we can fill it with expected data, see lab guide for more details
    mov     si, sp
    mov     word [si + Packet_BufferPacketSize], SizeOfPacket
    mov     word [si + Packet_Sectors], cx
    mov     word [si + Packet_BufferOffset], bx
    mov     word [si + Packet_BufferSegment], es
    mov     dword [si + Packet_StartSectors], eax
    mov     dword [si + Packet_StartSectors + 4], 0

    mov     dl, [DriverNumber]
    mov     ah, 42h
    int     13h
    jc      .fail               ; cf=1 if read error occurs, and we assume that the bios is broken

    add     sp, SizeOfPacket    ; free packet
    popad
    ret

.fail:
    mov     dh, MSG_FailedToRead
    call    ShowMessage
    jmp     $                   ; well, simply halt then, and you should check your env or program before next try

Disp09AF:
    mov     ah, 0x0E            ; 中断标志，写一个字符
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色

    int     10h                 ; 10h 号中断，刚好ax的ah为中断标志，al为要写的字符，不相干扰
    ret

DispASCLL:
    cmp     ax, 0x9                 ; 如果大于9，需要用ABCDE的ASCLL码；0-9是另一套
    ja      .AtoF
    add     ax, 48
    call    Disp09AF
    ret

.AtoF:
    add     ax, 55
    call    Disp09AF
    ret

Disp0x:
    ; 显示十六进制前缀"0x"
    mov     ah, 0x0E            ; 中断标志，写一个字符
    mov     al, '0'
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    int     10h
    mov     al, 'x'
    int     10h
    ret

DispSpSp:
    ; 方便分隔，加两个空格
    mov     ah, 0x0E            ; 中断标志，写一个字符
    mov     al, ' '
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    int     10h
    int     10h
    ret    

DispOneByte:
.up4bits:
    push    ax
    and     ax, 0xF0
    shr     ax, 4
    call    DispASCLL

.down4bits:
    pop     ax
    and     ax, 0x0F
    call    DispASCLL

    ret

DispType:
    push    bx
    push    ax

    call    Disp0x

    pop     ax
    call    DispOneByte

    call    DispSpSp

    pop     bx
    ret

DispLBA:
    push    bx
    push    eax

    call    Disp0x

    pop     eax
    push    eax

    and     eax, 0xFF000000         ; 读取高高字节
    shr     eax, 24
    call    DispOneByte

    pop     eax
    push    eax

    and     eax, 0x00FF0000         ; 读取高低字节
    shr     eax, 16
    call    DispOneByte

    pop     eax
    push    eax

    and     eax, 0x0000FF00         ; 读取低高字节
    shr     eax, 8
    call    DispOneByte

    pop     eax

    and     eax, 0x000000FF         ; 读取低低字节
    call    DispOneByte

    call    DispSpSp

    pop     bx
    ret


; 显示各分区信息
ShowPartInfo:
    mov     bx, 0x11BE
    mov     cx, 0
    
.onpart:
    mov     ax, [bx+4]
    call    DispType
    mov     eax, [bx+8]
    call    DispLBA

    add     bx, 16

    add     cx, 1
    cmp     cx, 4
    jnz     .onpart
    
    ret

Load1stBootable:
    mov     bx, 0x11BE  ; 4096+446
    ; bx没有必要再备份了
    ; push    bx
    mov     cx, 0
    
.onpart:
    mov     al, [bx]
    cmp     al, 0x80
    je      .load      

    add     bx, 16

    add     cx, 1
    cmp     cx, 4
    jnz     .onpart

; 读失败，死循环
.fail:
    mov     dh, MSG_NoBootable
    call    ShowMessage
    jmp     $
    
.load:
    push    ecx

    mov     eax, [bx+8]
    ; mov     ecx, [bx+12]  ; boot只有一个扇区，先不急着读整个磁盘
    mov     ecx, 1
    mov     bx, OffsetOfBoot
    call    ReadSector
    
    pop     ecx    
    ; pop     bx
    ret


; 此时已经被搬到MBR应该在的地方了，大跳到这里
Init:
    ; use temporary stack for fn call
    ; 把栈顶放到0x800处
    mov     sp, OffsetOfMBR

    ; backup driver number
    ; 备份驱动号，从dl读，这回果然比lab2正规了
    mov     [DriverNumber], dl

    ; clear screen for a better view of messages
    ; 清屏，显示FindPart
    mov     ax, 0x0003
    int     10h

    mov     dh, MSG_FindPart
    call    ShowMessage

    ; TODO: search for bootable partition in mbr partition table
    ; 要从分区表中找bootable的分区，那应该先把分区表读出来
    mov     ax, 0
    mov     cx, 1
    mov     bx, 0x1000
    call    ReadSector  ; 读到了es:bx

    ; 读完扇区后，似乎所有中断都无了，应该是读的位置不好，那么现在重新理一下内存空间
    ; 00000-003FF：中断向量表
    ; 00400-004FF：BIOS数据区
    ; 00500-007FF：LBA的Packet栈
    ; 00800-009FF：MBR复制到的区域
    ; 01000-07BFF：可用区域
    ; 07C00-07DFF：初始MBR位置
    ; 07E00-9FBFF：可用区域
    ; ……

    ; 那么这样看的确是把中断向量表给破坏了。。。
    ; 所以改成0x1000看看还有没有事

    ; 打印各分区信息
    call    ShowPartInfo

    ; mov     dh, MSG_NoBootable
    ; call    ShowMessage
    ; jmp     $

    mov     dh, MSG_Ready
    call    ShowMessage

    ; TODO: load boot sector to BaseOfBoot:OffsetOfBoot
    ; TODO: maybe the bootable partition is not a valid boot sector, why not check it?
    ; TODO: transfer info of driver number and partition to boot

    call    Load1stBootable
    jmp     BaseOfBoot:OffsetOfBoot

    times   SizeOfMBR - ($-$$) db 0
