#include "minios.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#define BUFF_MAPPING 0x80000000
#define PDE_P   (1<<0) // present
#define PDE_W   (1<<1) // write
#define PDE_U   (1<<2) // user
#define PDE_PS  (1<<7) // 4M

uint8_t array_buf[4096] __attribute__(aligned(4096)) = {0x55};

uint32_t pg_table[1024] __attribute__(aligned(4096)) = {0x88};
uint32_t pd_table[1024] __attribute__(aligned(4096)) = {
    [0] = (0)|PDE_P|PDE_W|PDE_U|PDE_PS,
};

// LDT
struct {uint16_t limit_l, base_l, basehl_attr, base_limit;}ldt_task0_table[2] __attribute__(aligned(8)) = {
    // 0x00cf9a000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, code section, 4G range
    [TASK_CODE_SEG/8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    // 0x00cf93000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, data section, 4G range
    [TASK_DATA_SEG/8] = {0xffff, 0x0000, 0xf300, 0x00cf},
};
struct {uint16_t limit_l, base_l, basehl_attr, base_limit;}ldt_task1_table[2] __attribute__(aligned(8)) = {
    // 0x00cf9a000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, code section, 4G range
    [TASK_CODE_SEG/8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    // 0x00cf93000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, data section, 4G range
    [TASK_DATA_SEG/8] = {0xffff, 0x0000, 0xf300, 0x00cf},
};

struct { uint16_t limit_l,base_l,basehl_attr,base_limit;}gdt_table[256] __attribute__((aligned(8))) = {
    // 0x00cf9a000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, code section, 4G range
    [KERNEL_CODE_SEG/8] = {0xffff, 0x0000, 0x9a00, 0x00cf},
    // 0x00cf93000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, data section, 4G range
    [KERNEL_DATA_SEG/8] = {0xffff, 0x0000, 0x9200, 0x00cf},

    // 0x00cf9a000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, code section, 4G range
    [USER_APP_CODE_SEG/8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    // 0x00cf93000000ffff - from 0x00000000, P=1, DPL-0, Type=none system, data section, 4G range
    [USER_APP_DATA_SEG/8] = {0xffff, 0x0000, 0xf300, 0x00cf},

    // TSS for tasks 0 and 1
    [TASK0_TSS_SEG/8] = {0x0068,0,0xe900,0x0},
    [TASK1_TSS_SEG/8] = {0x0068,0,0xe900,0x0},

    // System call
    [SYSCALL_SEG/8] = {0x0000,KERNEL_CODE_SEG,0xec03,0x0},

    // LDT
    [TASK0_LDT_SEG/8] = {sizeof(ldt_task0_table)-1,0x0,0xe200,0x00cf},
    [TASK1_LDT_SEG/8] = {sizeof(ldt_task1_table)-1,0x0,0xe200,0x00cf},
    
};

struct {uint16_t offset_l, segment_selector, attribute, offset_h;}idt_table[256] __attribute__(aligned(8)) = {0x88};

// Use assembly command in c source code file
void outb(uint8_t data, uint16_t port){
    __asm__ __volatile__("outb %[v],%[p]" : : [p]"d" (port), [v]"a" (data));
};

void _timer_interrupt(void);
void _syscall_handler(void);

void init_interrupt_8253(void){
    // init 8259 interrupt chip, enable timer channel
    outb(0x11, 0x20);       // init master 8253 chip
    outb(0x11, 0xA0);       // init slave 8253 chip
    outb(0x20, 0x21);       // write ICW2, tell master start with 0x20 interrupt table
    outb(0x28, 0xa1);       // write ICW2, tell slave start with 0x28 interrupt table
    outb((1 << 2), 0x21);   // write ICW3, tell master there is a slave on IRQ2 line
    outb(2, 0xa1);          // write ICW3, tell slave it connected to master on IRQ2 line
    outb(0x1, 0x21);        // write ICW4, tell master, use 8086 + normal EOI + none buffer mode
    outb(0x1, 0xa1);        // write ICW4, tell slave, use 8086 + normal EOI + none buffer mode
    outb(0xfe1, 0x21);      // tell master only enable timer interrupt on IRQ0 disable others
    outb(0xff, 0xa1);       // tell slave disable all interrupts on slave
}
void init_timer_8259(void){
    // Set timer every 100ms
    int tmo = (1193180);        // frequency 1193180
    outb(0x36, 0x43);           // binary count, mode 3, use internal timer0 (total has 3 timers)
    outb((uint8_t)tmo, 0x40);
    outb(tmo >> 8, 0x40);
}

void do_syscall(int func, char *string, char color){
    static int row = 1; //25 rows total and 80 columns
    if(func == 3){
        unsigned short * dest = (unsigned short *)0xb8000 + 80*row;
        while(*string){
            *dest++ = *string++|(color<<8);
        }
    }
    row = (row >= 25) ? 0: row+1;
    // Delay
    for (int i = 0; i<0xFFFFFF; i++);
}

void sys_display(char *string, char color){
    uint32_t address[]={0,SYSCALL_SEG};
    __asm__ __volatile__ ("push %[color]; push %[string]; push %[func]; lcalll *(%[a])\n\n"
    ::[color]"m"(color), [string]"m"(string), [func]"r"(3), [a]"r"(address));
}

uint32_t user_task_0_dpl3_stack[1024]; // level 3
uint32_t user_task_0_dpl0_stack[1024]; // level 0

uint32_t user_task_1_dpl3_stack[1024]; // level 3
uint32_t user_task_1_dpl0_stack[1024]; // level 0

void user_task_0(void){
    char *string ="task mjj 1234";
    // 80*25
    uint8_t color = 0x00;
    for(;;){
        sys_display(string.color++);
    }
}

void user_task_1(void){
    char *string ="task kjj 1234";
    uint8_t color = 0xff;
    for(;;){
        sys_display(string.color--);
    }
}

uint32_t user_task0_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)user_task_0_dpl0_stack + 4*1024/*system stack*/, KERNEL_DATA_SEG , /*no need to set*/ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pd_table,  (uint32_t)user_task_0/*task entry */, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)user_task_0_dpl3_stack + 4*1024/*user stack */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK0_LDT_SEG, 0x0,
};

uint32_t user_task1_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)user_task_1_dpl0_stack + 4*1024/*system stack*/, KERNEL_DATA_SEG , /*no need to set*/ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pd_table,  (uint32_t)user_task_1/*task entry */, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)user_task_1_dpl3_stack + 4*1024/*user stack */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK1_LDT_SEG, 0x0,
};

void task_scheduler(void)
{
    static int task_tss = TASK0_TSS_SEG;
    // Change tss jump to next task 
    task_tss=(task_tss==TASK0_TSS_SEG) ? TASK1_TSS_SEG:TASK0_TSS_SEG;
    uint32_t address[]={0,task_tss};
    __asm__ __volatile__ ("ljmpl *(%[a])"::[a]"r"(address));

}

void minios_init(void){
    init_interrupt_8253();
    init_timer_8259();
    idt_table[0x20].offset_h = (uint32_t)_timer_interrupt >> 16;
    idt_table[0x20].offset_l = (uint32_t)_timer_interrupt & 0xffff;
    idt_table[0x20].segment_selector = KERNEL_CODE_SEG;
    idt_table[0x20].attribute = 0x8E00;     // p, DPL = 0 system, interrupt gate type

    gdt_table[TASK0_TSS_SEG/8].base_l = (uint16_t)(uint32_t)user_task0_tss;
    gdt_table[TASK1_TSS_SEG/8].base_l = (uint16_t)(uint32_t)user_task1_tss;

    // System call
    gdt_table[SYSCALL_SEG/8].limit_l = (uint16_t)(uint32_t)_syscall_handler;

    // LDT
    gdt_table[TASK0_LDT_SEG/8].base_l = (uint16_t)(uint32_t)ldt_task0_table;
    gdt_table[TASK1_LDT_SEG/8].base_l = (uint16_t)(uint32_t)ldt_task1_table;

    // page directory table first level
    pd_table[BUFF_MAPPING>>22] = (uint32_t)pg_table|PDE_P|PDE_W|PDE_U;
    // page table second level
    pg_table[(BUFF_MAPPING>>12) & 0x3FF] = (uint32_t)array_buf|PDE_P|PDE_W|PDE_U;
}