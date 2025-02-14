#ifndef __USERPROG_SYSCALLINIT_H
#define __USERPROG_SYSCALLINIT_H
#include "stdint.h"
#include "syscall.h"
void syscall_init(void);
uint32_t sys_getpid(void);
uint32_t sys_write(char* str);
#endif