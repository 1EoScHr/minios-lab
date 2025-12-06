%.c.o: %.c
	gcc $^ -c -Wall -o $@ -fno-stack-protector -g3

%.S.o: %.S
	as $^ -o $@

main: thread_switching_x64.c.o switch_context.S.o
	gcc $^ -o $@

clean:
	rm -f *.o *.txt main

run: main
	@./$<

dasm: main
	@objdump -ald $< > $@.txt
	readelf -a $< > $<-readelf.txt

.DEFAULT_GOAL := main
.PHONY: clean