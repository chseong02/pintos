#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

typedef int pid_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process
{
  pid_t pid;
  int exit_code;
  struct semaphore exit_code_sema;
  //TODO: FDT
  struct list children;
  struct list_elem elem;
  struct semaphore exec_load_sema;
};

#endif /* userprog/process.h */
