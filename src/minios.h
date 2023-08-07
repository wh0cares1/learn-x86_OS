#ifndef MINIOS_H
#define MINIOS_H

// GDT
#define KERNEL_CODE_SEG     (1*8)
#define KERNEL_DATA_SEG     (2*8)

#define USER_APP_CODE_SEG   ((3*8)|3)
#define USER_APP_DATA_SEG   ((4*8)|3)

#define TASK0_TSS_SEG       (5*8)
#define TASK1_TSS_SEG       (6*8)

#define SYSCALL_SEG         (7*8)

#define TASK0_LDT_SEG       (8*8)
#define TASK1_LDT_SEG       (9*8)

// LDT
#define TASK_CODE_SEG       (0*0|0x4|3)
#define TASK_DATA_SEG       (1*8|0x4|3)

#endif //MINIOS_H