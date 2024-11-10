#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

#define OPEN_MAX 128

#define FILETYPE_FILE 0
#define FILETYPE_STDIN 1
#define FILETYPE_STDOUT 2

typedef int pid_t;

void process_init(void);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct fd_table_entry
{
  struct file *file;
  bool in_use;
  int type;
};

struct process
{
  pid_t pid;
  tid_t tid;
  int exit_code;
  struct semaphore exit_code_sema;
  //TODO: FDT
  struct list children;
  struct list_elem elem;
  struct semaphore exec_load_sema;
  struct fd_table_entry fd_table[OPEN_MAX];
};

void init_process (struct process*);

int get_available_fd(struct process *p);
bool set_fd(struct process *p, int fd, struct file *_file);
void remove_fd(struct process *p, int fd);

#endif /* userprog/process.h */
