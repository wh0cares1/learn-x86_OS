#include "minios.h"
.global _start, _timer_interrupt, _syscall_handler

.code16

.text
_start:
    mov $0, %ax
    //mov %ax, %cs
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    mov $_start, %esp   // esp point to 0x7c00

read_self_rest:
    mov $_start_32, %bx     // _start_32 this is going to be 0x7E00, because we used -Ttext=0x7c00 to
    mov $0x0002, %cx        // ch: track number, cl start sector number
    mov $0x0240, %ax        // ah: 0x02 read, al=0x40 64 sectors, we are reading 32KB
    mov $0x0080, %dx        // dh: head number, dl drive number 0x80 (hard disk 1)
    int $0x0013             // init a read to bios to read it
    jc read_self_rest       // if read failed, jump to reread it again

    // Load idt and gdt table turn on the protected mode
    cli                                 // disable interrupt
    lgdt gdt_desc                       // load gdt table to register
    lidt idt_desc                       // load idt table to register
    mov $1, %eax
    lmsw %ax                            // set PE bit enable protected mode
    jmp $KERNEL_CODE_SEG, $_start_32    // jump to select 32 bit protect mode, run 32 bit code

// BIOI
.org 0x1fe                  // 510 bytes 0
.byte 0x55,0xaa             // Boot flag just need 511-0x55 512-0xaa

// 32 protected mode, after 512 bytes
.code32
.text
_start_32:
    mov $KERNEL_CODE_SEG, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $_start, %esp   // sp -> 0x0000 - 0x7c00

    // Call init to the os
    call minios_init

    // Enable the paging
    mov $pd_table, %eax
    mov %eax, %cr3

    // support 4M big page PSE
    mov %cr4, %eax
    orl $(1<<4), %eax
    mov %eax, %cr4

    // enable page PG
    mov %cr0, %eax
    orl $(1<<31), %eax
    mov %eax, %cr0

    // Load the TSS of task0 to tr register
    mov $TASK0_TSS_SEG, %ax
    ltr %ax

    mov $TASK0_LDT_SEG, %ax
    lldt %ax
    
    // simulate interrupt return
    push $TASK_DATA_SEG
    push $user_task_0_dpl3_stack+1024*4    // will give it an array later on
    push $0x202     // there is 1 bit inside control the enable interrupt == sti enable interrupt
    push $TASK_CODE_SEG
    push $_user_task_0_entry

    iret

_user_task_0_entry:
    mov $USER_APP_CODE_SEG, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    jmp user_task_0

_timer_interrupt:
    push %ds    // save the cs segment register
    pusha       // save all the eax ebx ecx etc regular registers

    mov $0x20, %al
    outb %al, $0x20

    mov $KERNEL_DATA_SEG, %ax
    mov %ax, %ds

    call task_scheduler

    popa
    pop %ds

    iret

_syscall_handler:
    push %ds    // save the cs segment register
    pusha       // save all the eax ebx ecx etc regular 
    
    mov $KERNEL_DATA_SEG, %ax
    mov %ax, %ds

    mov %esp, %ebp
    push 13*4(%ebp)
    push 12*4(%ebp)
    push 11*4(%ebp)

    call do_syscall

    add $(3*4), %esp

    popa
    pop %ds

    retf $(3*4)

// gdt descriptions by lgdt
gdt_desc: 
.word(256*8)-1
.long gdt_table

// idt descriptions by lidt
idt_desc:
.word(256*8)-1
.long idt_table