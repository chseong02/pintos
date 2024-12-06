#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "vm/frame-table.h"
#include <list.h>

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

static void get_args (int *sp, int *dest, size_t num);
static bool get_user_bytes (void *dest, const void *src, size_t num);

static void sys_halt ();
static pid_t sys_exec (const char *cmd_line);
static int sys_wait (pid_t pid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_write (int fd, const void *buffer, unsigned size);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);
static mapid_t sys_mmap (int fd, void *addr);
static void sys_munmap (mapid_t mapping);

 	
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* Reads NUM bytes at user address SRC, stores at DEST.
   Note that DEST is not a vmem address.
   Returns true if every byte copies are successful. */
static bool
get_user_bytes (void *dest, const void *src, size_t num)
{
  uint8_t *_dest = dest;
  const uint8_t *_src = src;
  for (size_t i = 0; i < num; i++)
  {
    if (!check_ptr_in_user_space (_src)) return false;
    int res = get_user (_src);
    if (res == -1) return false;
    *_dest = (uint8_t) res;
    _dest++;
    _src++;
  }
  return true;
}

/* Only checks whether its in the user space */
bool
check_ptr_in_user_space (const void *ptr)
{
  return ptr < PHYS_BASE;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Parse NUM arguments from sp + 4 to dest. */
static void
get_args (int *sp, int *dest, size_t num)
{
  for(size_t i = 0; i < num; i++)
  {
    int *src = sp + i + 1;
    if (!check_ptr_in_user_space (src)) sys_exit (-1);
    if (!get_user_bytes (dest + i, src, 4)) sys_exit (-1);
  }
}

static void
syscall_handler (struct intr_frame *f) 
{
  int arg[4];
  if (!check_ptr_in_user_space (f->esp))
    sys_exit (-1);
  thread_current ()->last_esp = f->esp;
  switch(*(uint32_t *) (f->esp))
  {
    case SYS_HALT:
      sys_halt ();
      break;
    case SYS_EXIT:
      get_args (f->esp, arg, 1);
      sys_exit (arg[0]);
      break;
    case SYS_EXEC:
      get_args (f->esp, arg, 1);
      f->eax = sys_exec ((const char *) arg[0]);
      break;
    case SYS_WAIT:
      get_args (f->esp, arg, 1);
      f->eax = sys_wait ((pid_t) arg[0]);
      break;
    case SYS_CREATE:
      get_args (f->esp, arg, 2);
      f->eax = sys_create ((const char *) arg[0], (unsigned) arg[1]);
      break;
    case SYS_REMOVE:
      get_args (f->esp, arg, 1);
      f->eax = sys_remove ((const char *) arg[0]);
      break;
    case SYS_OPEN:
      get_args (f->esp, arg, 1);
      f->eax = sys_open ((const char *) arg[0]);
      break;
    case SYS_FILESIZE:
      get_args (f->esp, arg, 1);
      f->eax = sys_filesize (arg[0]);
      break;
    case SYS_READ:
      get_args (f->esp, arg, 3);
      f->eax = sys_read (arg[0], (void *) arg[1], (unsigned) arg[2]);
      break;
    case SYS_WRITE:
      get_args (f->esp, arg, 3);
      f->eax = sys_write(arg[0], (const void *) arg[1], (unsigned) arg[2]);
      break;
    case SYS_SEEK:
      get_args (f->esp, arg, 2);
      sys_seek (arg[0], (unsigned) arg[1]);
      break;
    case SYS_TELL:
      get_args (f->esp, arg, 1);
      f->eax = sys_tell (arg[0]);
      break;
    case SYS_CLOSE:
      get_args (f->esp, arg, 1);
      sys_close (arg[0]);
      break;
    case SYS_MMAP:
      get_args (f->esp, arg, 2);
      f->eax = sys_mmap (arg[0], (void *) arg[1]);
      break;
    case SYS_MUNMAP:
      get_args (f->esp, arg, 1);
      sys_munmap ((mapid_t)arg[0]);
      break;
    default:
      sys_exit (-1);
  }
}

static void
sys_halt ()
{
  shutdown_power_off ();
  NOT_REACHED ();
}

static pid_t
sys_exec (const char *cmd_line)
{
  struct list_elem *elem;
  struct process *p;
  tid_t tid = process_execute (cmd_line);
  if (tid == TID_ERROR)
    return PID_ERROR;
  /* Last added child */
  elem = list_back (&thread_current ()->process_ptr->children);
  p = list_entry (elem, struct process, elem);
  return p->pid;
}

void
sys_exit (int status)
{
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  cur->process_ptr->exit_code = status;

  file_close (cur->process_ptr->file_exec);
  for (size_t i = 2; i < OPEN_MAX; i++)
  {
    if(cur->process_ptr->fd_table[i].in_use)
    {
      file_close (cur->process_ptr->fd_table[i].file);
      remove_fd (cur->process_ptr, i);
    }
  }
  sema_up (&(cur->process_ptr->exit_code_sema));
  thread_exit ();
  NOT_REACHED ();
}

static int
sys_wait (pid_t pid)
{
  struct thread *cur = thread_current ();
  struct list* children = &cur->process_ptr->children;

  /* find pid process in children */
  for (struct list_elem *e = list_begin (children); e != list_end (children); 
    e = list_next (e))
  {
    struct process *p = list_entry (e, struct process, elem);
    if (p->pid == pid)
    {
      /* Wait for child exit */
      sema_down (&p->exit_code_sema);
      list_remove (e);
      int exit_code = p->exit_code;
      palloc_free_page (p);
      return exit_code;
    }
  }
  return -1;
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  if (file == NULL || !check_ptr_in_user_space (file))
    sys_exit (-1);
  file_lock_acquire ();
  bool res = filesys_create (file, initial_size);
  file_lock_release ();
  return res;
}

static bool
sys_remove(const char *file)
{
  if(file == NULL || !check_ptr_in_user_space(file))
    sys_exit(-1);
  file_lock_acquire();
  bool res = filesys_remove(file);
  file_lock_release();
  return res;
}

static int
sys_open(const char *file)
{
  if(file == NULL || !check_ptr_in_user_space(file))
    sys_exit(-1);
  
  /* Whole section is critical section due to open-twice test */
  file_lock_acquire();
  struct process *cur = thread_current()->process_ptr;

  int fd = get_available_fd(cur);
  if(fd == -1)
  {
    file_lock_release();
    return -1;
  }

  struct file *target_file = filesys_open(file);
  if(target_file == NULL)
  {
    file_lock_release();
    return -1;
  }

  /* Should verify the return value but seems okay now */
  set_fd(cur, fd, target_file);

  file_lock_release();
  return fd;
}

static int
sys_filesize(int fd)
{
  if(!(0 <= fd && fd < OPEN_MAX))
    return -1;
  
  struct process *cur = thread_current()->process_ptr;

  struct fd_table_entry *fd_entry = &(cur->fd_table[fd]);
  if(!(fd_entry->in_use && 
       fd_entry->type == FILETYPE_FILE && 
       fd_entry->file != NULL))
       return -1;
  
  file_lock_acquire();
  int res = file_length(fd_entry->file);
  file_lock_release();

  return res;
}

static int
sys_read(int fd, void *buffer, unsigned size)
{
  if(!check_ptr_in_user_space(buffer))
    sys_exit(-1);
  if(!(0 <= fd && fd < OPEN_MAX))
    return -1;
  
  struct process *cur = thread_current()->process_ptr;

  if(!cur->fd_table[fd].in_use)
    return -1;
  
  int file_type = cur->fd_table[fd].type;
  if(file_type == FILETYPE_STDIN)
  {
    void *cur_pos = buffer;
    unsigned write_count = 0;
    while(write_count < size)
    {
      if(!check_ptr_in_user_space(cur_pos))
        sys_exit(-1);
      uint8_t c = input_getc();
      if(!put_user((uint8_t *)cur_pos, c))
        sys_exit(-1);
      write_count++;
      cur_pos++;
    }
    return write_count;
  } 
  else if(file_type == FILETYPE_STDOUT)
  {
    /* Actually it also works same as STDIN in LINUX */
    sys_exit(-1);
  }
  else
  {
    file_lock_acquire();
    int res = file_read(cur->fd_table[fd].file, buffer, size);
    file_lock_release();
    return res;
  }
}

static int
sys_write(int fd, const void *buffer, unsigned size)
{
  if(!check_ptr_in_user_space(buffer))
    sys_exit(-1);
  if(!(0 <= fd && fd < OPEN_MAX))
    return -1;
  
  struct process *cur = thread_current()->process_ptr;

  if(!cur->fd_table[fd].in_use)
    return -1;
  
  int file_type = cur->fd_table[fd].type;
  if(file_type == FILETYPE_STDIN)
  {
    /* Actually it also works same as STDOUT in LINUX */
    sys_exit(-1);
  } 
  else if(file_type == FILETYPE_STDOUT)
  {
    putbuf(buffer, size);
    return size;
  }
  else
  {
    file_lock_acquire();
    int res = file_write(cur->fd_table[fd].file, buffer, size);
    file_lock_release();
    return res;
  }
}

static void
sys_seek(int fd, unsigned position)
{
  if(!(0 <= fd && fd < OPEN_MAX))
    return;
  
  struct process *cur = thread_current()->process_ptr;

  struct fd_table_entry *fd_entry = &(cur->fd_table[fd]);
  if(!(fd_entry->in_use && 
       fd_entry->type == FILETYPE_FILE && 
       fd_entry->file != NULL))
       return;
  
  file_lock_acquire();
  file_seek(fd_entry->file, position);
  file_lock_release();
}

static unsigned
sys_tell(int fd)
{
  if(!(0 <= fd && fd < OPEN_MAX))
    return -1;
  
  struct process *cur = thread_current()->process_ptr;

  struct fd_table_entry *fd_entry = &(cur->fd_table[fd]);
  if(!(fd_entry->in_use && 
       fd_entry->type == FILETYPE_FILE && 
       fd_entry->file != NULL))
       return -1;
  
  file_lock_acquire();
  unsigned res = file_tell(fd_entry->file);
  file_lock_release();

  return res;
}

static void
sys_close(int fd)
{
  if(!(0 <= fd && fd < OPEN_MAX))
    return;
  
  struct process *cur = thread_current()->process_ptr;

  struct fd_table_entry *fd_entry = &(cur->fd_table[fd]);
  if(fd_entry->in_use && 
     fd_entry->type == FILETYPE_FILE && 
     fd_entry->file != NULL)
  {
    file_lock_acquire();
    file_close(fd_entry->file);
    file_lock_release();
  }

  remove_fd(cur, fd);
}

static mapid_t
sys_mmap (int fd, void *addr)
{
  /* check file validity */
  int file_size = sys_filesize(fd);
  if(file_size <= 0)
  {
    /* zero or error */
    return MAP_FAILED;
  }

  /* check address align */
  if(addr == NULL || (int)addr % PGSIZE)
  {
    return MAP_FAILED;
  }

  /* check if page already exists in range */
  for(int i = 0; i < file_size; i += PGSIZE)
  {
    if(find_s_page_table_entry_from_upage(addr + i) != NULL)
    {
      return MAP_FAILED;
    }
  }

  file_lock_acquire();

  /* open same file again because original file can be 
     closed after mmap but mmap should stay */
  struct process *cur = thread_current()->process_ptr;
  struct file *f = cur->fd_table[fd].file;
  struct file *new_f = file_reopen(f);
  if(new_f == NULL)
  {
    file_lock_release();
    return MAP_FAILED;
  }

  /* allocate mapping data & setup */
  struct fmm_data *fmm = malloc(sizeof(struct fmm_data));
  if(fmm == NULL)
  {
    file_lock_release();
    return MAP_FAILED;
  }
  fmm->id = cur->mmap_count++;
  fmm->file = new_f;
  fmm->file_size = file_size;
  fmm->page_count = 0;

  /* set page table for every pages in range */
  for(int i = 0; i < file_size; i += PGSIZE)
  {
    int page_data_size = file_size - i >= PGSIZE ? PGSIZE : file_size - i;
    s_page_table_add(true, new_f, i, true, addr + i, NULL, page_data_size, PGSIZE - page_data_size, FAL_USER);
    fmm->page_count++;
  }

  list_push_back(&(cur->fmm_data_list), &(fmm->fmm_data_list_elem));

  file_lock_release();
    
  /* Create new struct fmm_data, initialize it and push into the list */
  /* allocate new mapid for new fmm */
  return fmm->id;
}

static void
sys_munmap (mapid_t mapping)
{

}