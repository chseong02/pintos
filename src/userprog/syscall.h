#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/stdint.h"
#include "lib/stdbool.h"
#include "lib/stddef.h"
#include "lib/user/syscall.h"

void get_args(int *sp, int *dest, size_t num);
bool check_ptr_in_user_space(const void *ptr);
bool get_user_bytes(void *dest, const void *src, size_t num);

void syscall_init (void);

void sys_halt();
void sys_exit(int status);
pid_t sys_exec(const char *cmd_line);
int sys_wait(pid_t pid);
bool sys_create(const char *file, unsigned initial_size);
bool sys_remove(const char *file);
int sys_open(const char *file);
int sys_filesize(int fd);
int sys_read(int fd, void *buffer, unsigned size);
int sys_write(int fd, const void *buffer, unsigned size);
void sys_seek(int fd, unsigned position);
unsigned sys_tell(int fd);
void sys_close(int fd);

#endif /* userprog/syscall.h */
