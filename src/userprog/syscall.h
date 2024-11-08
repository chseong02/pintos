#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/stdint.h"
#include "lib/stdbool.h"
#include "lib/stddef.h"

void get_args(int *sp, int *dest, size_t num);
bool check_ptr_in_user_space(const void *ptr);
bool get_user_bytes(void *dest, const void *src, size_t num);

void syscall_init (void);

void sys_halt();
void sys_exit(int status);
int sys_write(int fd, const void *buffer, unsigned size);

#endif /* userprog/syscall.h */
