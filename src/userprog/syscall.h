#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/stdint.h"
#include "lib/stdbool.h"

void get_args(int *sp, int *dst, int num);
bool check_ptr_in_user_space(const void *ptr);

void syscall_init (void);

void sys_halt();
void sys_exit(int exit_code);

#endif /* userprog/syscall.h */
