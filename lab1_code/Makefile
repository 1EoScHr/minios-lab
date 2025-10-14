TARGET ?= 2

PATH_2 := src/boot.asm
PATH_3  := src/newboot.asm

ifeq ($(TARGET),2)
PATH := $(PATH_2)
else ifeq ($(TARGET),3)
PATH := $(PATH_3)
else
$(error TARGET should be 2 or 3)
endif

# 搞不懂为什么必须得用绝对路径，要不就给我报错
boot.bin: $(PATH)
	/usr/bin/nasm $(PATH) -o boot.bin

.PHONY: clean
clean:
	/usr/bin/rm -f boot.bin

.PHONY: run
run: clean boot.bin # 依赖调用
	/usr/bin/qemu-system-i386 -boot order=c -drive file=boot.bin,format=raw
