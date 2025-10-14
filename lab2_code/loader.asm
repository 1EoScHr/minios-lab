%include "consts.inc"

%macro WriteChar 3
    mov     [gs:((80 * (%1) + (%2)) * 2)], word 0x0f00 + %3
%endmacro

    org     OffsetOfLoader

_start:
    ; gs附加段存放要显示的内容，这里是在显存段中
    mov     ax, 0xb800
    mov     gs, ax

    WriteChar 12 - 1, 28 + 0 + 0 * 2, 'T'
    WriteChar 12 - 1, 28 + 0 + 1 * 2, 'h'
    WriteChar 12 - 1, 28 + 0 + 2 * 2, 'i'
    WriteChar 12 - 1, 28 + 0 + 3 * 2, 's'
    WriteChar 12 - 1, 28 + 0 + 4 * 2, ' '
    WriteChar 12 - 1, 28 + 0 + 5 * 2, 'i'
    WriteChar 12 - 1, 28 + 0 + 6 * 2, 's'

    WriteChar     12, 28 + 2 + 0 * 2, 'l'
    WriteChar     12, 28 + 2 + 1 * 2, 'z'
    WriteChar     12, 28 + 2 + 2 * 2, 't'
    WriteChar     12, 28 + 2 + 3 * 2, "'"
    WriteChar     12, 28 + 2 + 4 * 2, 's'

    WriteChar 12 + 1, 28 + 5 + 0 * 2, 'l'
    WriteChar 12 + 1, 28 + 5 + 1 * 2, 'o'
    WriteChar 12 + 1, 28 + 5 + 2 * 2, 'a'
    WriteChar 12 + 1, 28 + 5 + 3 * 2, 'd'
    WriteChar 12 + 1, 28 + 5 + 4 * 2, 'e'
    WriteChar 12 + 1, 28 + 5 + 5 * 2, 'r'
    WriteChar 12 + 1, 28 + 5 + 6 * 2, '.'

    ; 这里不能再与boot中照猫画虎了，要把每一个段都划分独立的内存空间，形成熟悉的那种内存堆栈结构
    ; 并且还要注意不能盲目使用约定俗成的段，尤其是GPT说的！还需要灵活分配
    ; 因为本来用的是0x9000后面几个，发现其是cs，遂变成0xA000，但是再一看一问，0xA000-0xBFFF是显存内容（也就是上面的gs），也不能占用
    ; 还有0xC0000-0xFFFFF是bios的只读区域，可能在这里操作会导致写入异常，要往低地址放

    mov ax, 0x0050
    mov ds, ax          ; 0x00500-0x07BFF，做数据段

    mov ax, 0x9000      ; 这里空间还大，一般都是loader+栈
    mov ss, ax
    mov sp, 0xFF00      ; 栈顶在0xD000:0xFF00，倒着增长，不在0xffff，留点余地。

    mov ax, 0x0000
    mov gs, ax          ; 下面发现还要用BPB中的数据，可以用0x0000:0x7c00这样来到达

    mov ax, 0x07E0      ; 这一块是0x07E00-0x09FBF
    mov es, ax          ; 读到的簇放在es段，一个簇占用字节数为 8*512=4096
                        ; 那么一个簇用0x07E00到0x08DFF就可以表示（地址都是字节偏移）
                        ; 剩下的空间可以干其他的，见下

    mov ax, 0x08E0      ; 剩余的0x08E00-0x09FBF
    mov fs, ax          ; 存放FAT数据

    jmp     Init    ; 照猫画虎，跳转过去

    FileName        db  "AA1     TXT"   ; 该变量存放要寻找的文件名
    EndOfBuffer     dw  0               ; 该变量存放写进buffer的一个簇的尽头
    SecOfDataZone   dd  0               ; 该变量存放数据区/根目录的起始扇区

    ClusterNum1st   dd  0               ; 文件的第一簇
    ClusterNum      dd  0               ; 一个簇号由32bit/4字节组成
    OneASCLL        db  0               ; 转换为ASCLL，一字节

; MSG宏这里会冲突，所以放弃使用

; 写某扇区开始的若干个扇区
; 要写入的数据在es:bx处，执行结束后eax为起始扇区索引，cx为写入扇区数量
WriteSector:
    push    ds                  ; 保存ds初值，原因见下；之所以放在开头，是为了不干扰接下来手动分配的栈

    pushad  ; 保存现场
    sub     sp, SizeOfPacket    ; 在栈上分配内存

    ; 这里与boot不同，因为要真的用栈了，栈内存用bp更好，会自动基于ss段
    ; 并且用栈的话索引方式就得改变，原本的索引是si + xxx，现在就得是bp - SizeOfPacket + xxx
    ; 现在bp指向了一个新的packet结构，然后填充它

    mov     si, sp
    mov     word  ss:[si + Packet_BufferPacketSize], SizeOfPacket   ; 对应packet_size，0、1，可能包括了reserved
    mov     word  ss:[si + Packet_Sectors], cx                      ; 对应total_sectors，2、3
    mov     word  ss:[si + Packet_BufferOffset], bx                 ; 对应buffer的低2字节，偏移量，4、5
    mov     word  ss:[si + Packet_BufferSegment], es                ; 对应buffer的高2字节，段寄存器值，6、7
    mov     dword ss:[si + Packet_StartSectors], eax                ; 对应start_sector，8、9、10、11
    mov     dword ss:[si + Packet_StartSectors + 4], 0              ; 对应start_sector的高16位，12、13、14、15
                                                                ; 感觉这里的细节和手册不太一样？

    ; 没有仔细看说明，这里还要求ds:si指向这个结构体
    ; 另外注意，pushad、popad不会存储段基址寄存器，要手动恢复！

    mov     dx, ss
    mov     ds, dx              ; 把ds指向ss，这样ds:si=ss:si

    mov     dl, DriverNumber    ; 这应该就是手册说的硬编码，“进入boot时获取并存储dl寄存器的值，它是由BIOS在引导时自动设置的当前驱动器号”
    mov     ah, 43h
    mov     al, 02h             ; 用个验证写入

    int     13h                 ;
    jc      .fail               ; 有进位则跳转，CF标志为1表示读时出错

    add     sp, SizeOfPacket    ; 中断用完packet，释放栈内存

    popad   ; 恢复现场
    pop     ds      ; 恢复ds
    ret

.fail:
    mov     ax, 0xb800      ; 再次调用显存，由于下面就跑死了，所以原来的gs内容直接放弃
    mov     gs, ax

    WriteChar 12 + 3, 28 + 0 + 0 * 2, 'E'
    WriteChar 12 + 3, 28 + 0 + 1 * 2, ':'
    WriteChar 12 + 3, 28 + 0 + 2 * 2, 'W'
    WriteChar 12 + 3, 28 + 0 + 3 * 2, 'R'
    WriteChar 12 + 3, 28 + 0 + 4 * 2, 'T'

    jmp     $                   ; well, simply halt then, and you should check your env or program before next try

; 写入指定簇
; 要写的内容放到es:bx处；执行结束后把内容写到磁盘的eax簇
WriteCluster:
    pushad      ; 公式化保留现场

    ; 计算要写簇的扇区索引，执行结束后ecx是一簇的扇区数，eax是目标簇的起始扇区索引

    sub     eax, CLUSTER_Base               ; 0簇和1簇是保留簇，数据区实际从2号簇开始，簇号要减这个偏移
    movzx   ecx, byte gs:[BPB_SecPerClus]   ; movzx：高位填充0
    mul     ecx                             ; 隐藏操作数，实际为eax*ecx，结果放在edx:eax，此时eax是从数据区开头到对应簇的扇区数量
    add     eax, cs:[SecOfDataZone]         ; 加上初始偏移就是指定簇的起始扇区

    call    WriteSector

    popad
    ret

; 读取某扇区开始的若干个扇区
; eax为起始扇区索引，cx为读取扇区数量，执行结束后读取到的数据放到es:bx处
ReadSector:
    push    ds                  ; 保存ds初值，原因见下；之所以放在开头，是为了不干扰接下来手动分配的栈

    pushad  ; 保存现场
    sub     sp, SizeOfPacket    ; 在栈上分配内存

    ; 这里与boot不同，因为要真的用栈了，栈内存用bp更好，会自动基于ss段
    ; 并且用栈的话索引方式就得改变，原本的索引是si + xxx，现在就得是bp - SizeOfPacket + xxx
    ; 现在bp指向了一个新的packet结构，然后填充它

    mov     si, sp
    mov     word  ss:[si + Packet_BufferPacketSize], SizeOfPacket   ; 对应packet_size，0、1，可能包括了reserved
    mov     word  ss:[si + Packet_Sectors], cx                      ; 对应total_sectors，2、3
    mov     word  ss:[si + Packet_BufferOffset], bx                 ; 对应buffer的低2字节，偏移量，4、5
    mov     word  ss:[si + Packet_BufferSegment], es                ; 对应buffer的高2字节，段寄存器值，6、7
    mov     dword ss:[si + Packet_StartSectors], eax                ; 对应start_sector，8、9、10、11
    mov     dword ss:[si + Packet_StartSectors + 4], 0              ; 对应start_sector的高16位，12、13、14、15
                                                                ; 感觉这里的细节和手册不太一样？

    ; 没有仔细看说明，这里还要求ds:si指向这个结构体
    ; 另外注意，pushad、popad不会存储段基址寄存器，要手动恢复！

    mov     dx, ss
    mov     ds, dx              ; 把ds指向ss，这样ds:si=ss:si

    mov     dl, DriverNumber    ; 这应该就是手册说的硬编码，“进入boot时获取并存储dl寄存器的值，它是由BIOS在引导时自动设置的当前驱动器号”
    mov     ah, 42h
    int     13h                 ; 调用读取磁盘LBA读取中断
    jc      .fail               ; 有进位则跳转，CF标志为1表示读时出错

    add     sp, SizeOfPacket    ; 中断用完packet，释放栈内存

    popad   ; 恢复现场
    pop     ds      ; 恢复ds
    ret

.fail:
    mov     ax, 0xb800      ; 再次调用显存，由于下面就跑死了，所以原来的gs内容直接放弃
    mov     gs, ax

    WriteChar 12 + 3, 28 + 0 + 0 * 2, 'E'
    WriteChar 12 + 3, 28 + 0 + 1 * 2, ':'
    WriteChar 12 + 3, 28 + 0 + 2 * 2, 'L'
    WriteChar 12 + 3, 28 + 0 + 3 * 2, 'B'
    WriteChar 12 + 3, 28 + 0 + 4 * 2, 'A'

    jmp     $                   ; well, simply halt then, and you should check your env or program before next try

; 根据簇链获取下一个簇的簇号（因为一个文件可能很大，超过一个簇，在FAT中有簇链来一一索引）
; 调用时eax装当前簇号，执行结束时eax装下一个簇的簇号
NextCluster:

    ; boot.asm中的一开始没懂，所以自己重写的
    ; compute start sector of the cluster
    ; start-sector = cluster * 4 / bytesPerSec + rsvdSecCnt
    ; compute offset to cluster in the sector
    ; offset = cluster * 4 % bytesPerSec
    ; 这里的公式似乎是那种十分牛逼的简化，能够从簇号倒推回FAT中的对应项

    ; 已知当前簇号，要对应到FAT中去，首先要获取FAT中数据，那就是readSector
    pushad
    push    es

    mov     cx, fs
    mov     es, cx      ; 将es重定向为fs，因为后面要读取扇区到es:bx

    ; 一个扇区512字节，一个FAT项32位4字节，则一个扇区就有128个FAT项，那么簇号对应扇区为cluster/128，对应扇区内FAT项为cluster%128

    mov     di, ax                          ; 簇号备份，下面要用；为什么低16位就够？2^16个簇，再乘8个扇区，再乘512B，远大于16MB
    shr     eax, 7                          ; / 128
    movzx   ecx, word gs:[BPB_RsvdSecCnt]
    add     eax, ecx                        ; + FAT前的保留扇区数

    mov     cx, 1                           ; 读取1个扇区
    mov     bx, 0
    call    ReadSector

    and     di, 0x7f                        ; % 128，此时edx为扇区内FAT项
    shl     di, 2                          ; 写下一行才发现，要把FAT项变为字节寻址，所以乘4 ; 这里竟然有bug，之前都没发现写的是dx而不是di，并且对结果还没影响
    mov     eax, es:[di]                    ; 这时就没问题了，当为0、128等FAT项时，都能对应上字节
    and     eax, CLUSTER_Mask               ; 簇号掩码规范一下（具体是为什么也不清楚，可能是保险起见）

    ; 这里我们得到了新的簇号放到eax里，并且希望能够覆写保留现场中的eax，就需要手动回溯到栈上对应位置进行覆盖
    ; 压栈顺序是eax、ebx、ecx、edx、esp、ebp、esi、edi，完成后新的esp（栈顶指针）指向edi，也就是[esp+0] = edi
    ; 并且这里数据在栈中，就要考虑栈的特性，必须用bp才会用ss来作为基址、并且从栈顶开始记

    pop     es                      ; 把es恢复
    mov     bp, sp
    mov     [bp + 28], eax          ; 栈相关最好都用bp

    popad                           ; 恢复现场
    ret

; 读取指定簇
; eax存放要读取的簇的簇号；执行结束后读取到的内容放到es:bx处
ReadCluster:
    pushad      ; 公式化保留现场

    ; 计算当前簇的扇区索引，执行结束后ecx是一簇的扇区数，eax是指定簇的起始扇区索引

    sub     eax, CLUSTER_Base               ; 0簇和1簇是保留簇，数据区实际从2号簇开始，簇号要减这个偏移
    movzx   ecx, byte gs:[BPB_SecPerClus]   ; movzx：高位填充0
    mul     ecx                             ; 隐藏操作数，实际为eax*ecx，结果放在edx:eax，此时eax是从数据区开头到对应簇的扇区数量
    add     eax, cs:[SecOfDataZone]         ; 加上初始偏移就是指定簇的起始扇区

    call    ReadSector

    popad
    ret

SetCursor:
; 记得把dh设成行，dl设成列
    push    eax
    push    ebx

    mov     ah, 0x02            ; 中断标志
    mov     bh, 0x00            ; 页号

    int     10h

    pop     ebx
    pop     eax
    ret

; 调用下两个函数时都是ax为0x0-0xF
Disp0to9:
    add     ax, 48

    mov     ah, 0x0E            ; 中断标志，写一个字符
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色

    int     10h                 ; 10h 号中断，刚好ax的ah为中断标志，al为要写的字符，不相干扰

    ret

DispAtoF:
    add     ax, 55

    mov     ah, 0x0E            ; 中断标志
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色

    int     10h                 ; 10h 号中断

    ret

; 发现小端是倒着按字节来的，那就分成一个个字节显示
; 输入时，eax是8位的一字节
DispOneByte:
    pushad

.up4bits:
    push    ax
    and     ax, 0xF0
    shr     ax, 4
    cmp     ax, 0x9                 ; 如果大于9，需要用ABCDE的ASCLL码；0-9是另一套
    ja      .AtoF1
    call    Disp0to9
    jmp     .down4bits

.AtoF1:
    call    DispAtoF

.down4bits:
    pop     ax
    and     ax, 0x0F
    cmp     ax, 0x9
    ja      .AtoF2
    call    Disp0to9

    popad
    ret

.AtoF2:
    call    DispAtoF

    popad
    ret

DispClusterNum:
    pushad

    ; 显示十六进制前缀"0x"
    mov     ah, 0x0E            ; 中断标志，写一个字符
    mov     al, '0'
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    int     10h
    mov     al, 'x'
    int     10h

    mov     eax, cs:[ClusterNum]
    and     eax, 0xFF000000         ; 读取高高字节
    shr     eax, 24
    call    DispOneByte

    mov     eax, cs:[ClusterNum]
    and     eax, 0x00FF0000         ; 读取高低字节
    shr     eax, 16
    call    DispOneByte

    mov     eax, cs:[ClusterNum]
    and     eax, 0x0000FF00         ; 读取低高字节
    shr     eax, 8
    call    DispOneByte

    mov     eax, cs:[ClusterNum]    ; 读取簇号
    and     eax, 0x000000FF         ; 读取低低字节
    call    DispOneByte

    ; 方便分隔，加两个空格
    mov     ah, 0x0E            ; 中断标志，写一个字符
    mov     al, ' '
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    int     10h
    int     10h

    popad
    ret

; 显示一整个文件的所有簇
DispClusterChain:
    pushad
    mov     eax, cs:[ClusterNum]; 赋值起始簇号给eax

.NextCluster:
    call    DispClusterNum      ; 显示簇
    call    NextCluster         ; 使用NextCluster从簇链上获取下一簇
    mov     cs:[ClusterNum], eax; 此时eax是下一簇的簇号，先存起来
    cmp     eax, CLUSTER_Last   ; 检查是否到达最后一簇
    jnz     .NextCluster

    popad
    ret

Front10Message:
    db      "File's front 10 char are "

DispFront10Message:
    pushad  ; 保留现场
    push    es

    mov     dx, cs
    mov     es, dx
    mov     dx, Front10Message
    mov     bp, dx              ; ES:BP = 串地址

    mov     ah, 0x13            ; 中断标志
    mov     al, 0x00            ; 边写边动光标
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    mov     cx, 0x19            ; 字符串内字符数量
    mov     dh, 7
    mov     dl, 0

    int     10h                 ; 10h 号中断

    pop     es
    popad   ; 恢复现场
    ret


ClusterMessage:
    db      "Cluster Chain of File is "

DispClusterMessage:
    pushad  ; 保留现场
    push    es

    mov     dx, cs
    mov     es, dx
    mov     dx, ClusterMessage
    mov     bp, dx              ; ES:BP = 串地址

    mov     ah, 0x13            ; 中断标志
    mov     al, 0x00            ; 边写边动光标
    mov     bh, 0x00            ; 页号
    mov     bl, 0x1F            ; 背景、前景颜色
    mov     cx, 0x19            ; 字符串内字符数量
    mov     dh, 4
    mov     dl, 0

    int     10h                 ; 10h 号中断

    pop     es
    popad   ; 恢复现场
    ret

Disp10frontChar:
    pushad

    mov     eax, cs:[ClusterNum1st]
    call    ReadCluster             ; 把文件的起始簇读到es:bx处

    mov     bp, bx              ; ES:BP = 串地址

    mov     ah, 0x13            ; 中断标志
    mov     al, 0x00            ; 边写边动光标
    mov     bh, 0x00            ; 页号
    mov     bl, 0x07            ; 背景、前景颜色
    mov     cx, 0x0A            ; 字符串内字符数量，前10个
    mov     dh, 8
    mov     dl, 0

    int     10h                 ; 10h 号中断

    popad
    ret

ChangeFileGOOD:
    pushad

    ; 一个簇有8个扇区，一个扇区有512字节
    ; 那么512-515四个字节在起始0簇的1扇区开头
    mov     eax, cs:[ClusterNum1st]         ; 把起始簇读到es:bx处
    call    ReadCluster

    mov     byte es:[bx+512],   'G'
    mov     byte es:[bx+512+1], 'O'
    mov     byte es:[bx+512+2], 'O'
    mov     byte es:[bx+512+3], 'D'

    call    WriteCluster                    ; 把es:bx处的写入起始簇

    popad
    ret


; 初始化，要明确目的：让eax中存放要读取的簇（也就是root的第一簇）
Init:

    ; 计算EndOfBuffer这一变量的值，其用来判断是否到达一簇的尽头
    ; 注意这里与boot不一样了，用了gs的段 错❌
    ; 调试时候发现，这里BPB信息在consts.inc中本来就是写成绝对地址的
    ; 因此直接用就好

    movzx   ax, byte gs:[BPB_SecPerClus]    ; 从BPB数据中获取每一簇的扇区数量
    mul     word gs:[BPB_BytsPerSec]        ; 获取每扇区的字节数，相乘即为一簇所占用的字节数
    ; add     ax, OffsetOfBootBuffer ; 淘汰
    mov     cs:[EndOfBuffer], ax            ; 现在存放的是簇buffer的大小

    ; 计算数据区起始扇区，那里也就是根目录
    movzx   eax, byte gs:[BPB_NumFATs]      ; 从BPB数据中获取FAT表数量
    mul     dword gs:[BPB_FATSz32]          ; 获取每个FAT表占用扇区数量，与FAT表数量相乘，即为FAT区域所占用扇区数
    movzx   edx, word gs:[BPB_RsvdSecCnt]   ; 获取引导所占用扇区数
    add     eax, edx                        ; 引导扇区数+FAT扇区数，相加即可得到数据区起始扇区
    mov     cs:[SecOfDataZone], eax

    ; mov     ax, BaseOfBootBuffer ; 淘汰，现在一整个es段供使用
    ; mov     es, ax

    mov     eax, gs:[BPB_RootClus]          ; 根目录簇号

; 获取root的第一簇（第一次执行）/沿着簇链找到的其他簇（后面执行）
.load_dir_cluster:
    ; mov     bx, OffsetOfBootBuffer ; 这个应该可以淘汰，设成0就好了
    mov     bx, 0       ; 读取到的在es:0，刚好
    call    ReadCluster

    ; 读取到的簇内容是在es:bx处，所以把di也赋值bx
    mov     di, bx

.find_file:
    push    ds                  ; 把ds现值入栈，因为FileName是基于CS的偏移，所以要让ds=cs，下面才能正常
    push    di                  ; 将di入栈，下面会更改di的值，要保证di位于每一个目录项的头

    mov     dx, cs
    mov     ds, dx

    mov     si, FileName        ; 把目标文件名地址写入si寄存器
    mov     ecx, 11             ; 把cx寄存器值变为11，因为下一步是要比较文件名字符串，8B+3B=11B
    repe    cmpsb               ; 循环，比较DS:SI(预设的文件名)与ES:DI(当前目录项，起始刚好也是文件名)
    jz      .found              ; 找到则跳转

    ; 说明这一个目录项不匹配，继续下一个
    pop     di                  ; 恢复di
    pop     ds
    add     di, SizeOfDIR       ; di寄存器增加一个目录项的值，也就跳转到下一个目录项
    cmp     di, cs:[EndOfBuffer]; 判断是否到达簇尽头
    jnz     .find_file          ; cmp + jnz组合，不相等(也就是没到当前簇尽头)就继续下一个

    ; 说明当前簇中没有要找的文件，(如果有)去下一个簇
    call    NextCluster         ; 去下一个簇
    cmp     eax, CLUSTER_Last
    jnz     .load_dir_cluster   ; 没有到簇链尽头则跳转

; 走到这里说明遍历结束了也没找到
.fail:
    ;mov     dh, MSG_FileNotFound; MSG内容；这是什么宏？最后要研究一下
    ;call    ShowMessage

    mov     dx, 0xb800      ; 再次调用显存，由于下面就跑死了，所以原来的gs内容直接放弃
    mov     gs, dx

    WriteChar 12 + 3, 28 + 0 + 0 * 2, 'E'
    WriteChar 12 + 3, 28 + 0 + 1 * 2, ':'
    WriteChar 12 + 3, 28 + 0 + 2 * 2, 'E'
    WriteChar 12 + 3, 28 + 0 + 3 * 2, 'O'
    WriteChar 12 + 3, 28 + 0 + 4 * 2, 'F'

    jmp     $

; 找到对应文件处理逻辑
.found:
    pop     di
    pop     ds

    ; 此时簇号还是根目录的簇号，需要从目录项中解析出实际簇号
    ; 此时ES:DI刚好在目录项开头，0x14-0x15是簇号高16位，0x1A-0x1B是簇号的低16位
    mov     ax,  es:[di + 0x14]
    shl     eax, 16
    mov     ax,  es:[di + 0x1A]
    mov     cs:[ClusterNum], eax    ; 这样ClusterNum就存放该文件的起始簇
    mov     cs:[ClusterNum1st], eax ; 同样也保存在1st里面，便于回头

    push    gs

    mov     dx, 0xb800
    mov     gs, dx

    WriteChar 12 + 3, 28 + 0 + 0 * 2, 'Y'
    WriteChar 12 + 3, 28 + 0 + 1 * 2, ':'
    WriteChar 12 + 3, 28 + 0 + 2 * 2, 'H'
    WriteChar 12 + 3, 28 + 0 + 3 * 2, 'I'
    WriteChar 12 + 3, 28 + 0 + 4 * 2, 'T'

    pop     gs

    ; 显示簇号信息，再显示簇号
    call    DispClusterMessage
    mov     dh, 5
    mov     dl, 0
    call    SetCursor
    call    DispClusterChain    ; 显示簇链

    ; 显示提示信息，以及文件的前10个字符
    call    DispFront10Message
    call    Disp10frontChar

    ; 改变相关字符为GOOD
    call    ChangeFileGOOD

    jmp     $
