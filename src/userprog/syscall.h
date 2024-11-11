#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/stdint.h"
#include "lib/stdbool.h"
#include "lib/stddef.h"
#include "lib/user/syscall.h"

bool check_ptr_in_user_space(const void *ptr);

void syscall_init (void);
void sys_exit(int status);

#endif /* userprog/syscall.h */
