######################################################
# class name: Build an Operating System From Scratch (x86 base)
# email:classknowledgecoding@gmail.com
######################################################

# Makefile help you to build your project

#prefix for cross tooltrain
TOOL_PREFIX = x86_64-elf-

#gcc flags
#-g (add debug information to your elf)
#-c (only compile *.S *.c to *.o)
#-O0 (do not optimization)
#-m32 (32bit instuctionset)
#-fno-pie (not to make a position independent executable)
#-fno-stack-protecctor
#-nostdlib (don't link standrd libraries)
#-nostdinc (don't use standrd include files)
CFLAGS = -g -c -O0 -m32 -fno-pie -fno-stack-protector -nostdlib -nostdinc

#target all 
#(compile src/start.S src/minios.S)
#(link to minios.elf)
#(get bin file  minios.bin)
#(disassembly to  minios_dis.txt)
#(get sections of elf to minios_elf.txt)
all: src/minios.c src/minios.h src/start.S
	$(TOOL_PREFIX)gcc $(CFLAGS) src/start.S
	$(TOOL_PREFIX)gcc $(CFLAGS) src/minios.c	
	$(TOOL_PREFIX)ld -m elf_i386 -Ttext=0x7c00 start.o minios.o -o minios.elf
	${TOOL_PREFIX}objcopy -O binary minios.elf minios.bin
	${TOOL_PREFIX}objdump -x -d -S  minios.elf > minios_dis.txt	
	${TOOL_PREFIX}readelf -a  minios.elf > minios_elf.txt	
	dd if=minios.bin of=./image/disk.img conv=notrunc

#clean up
clean:
	rm -f *.bin *.elf *.o *.txt
