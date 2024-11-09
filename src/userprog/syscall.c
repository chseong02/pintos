#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

 	
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
bool
get_user_bytes(void *dest, const void *src, size_t num)
{
  uint8_t *_dest = dest;
  const uint8_t *_src = src;
  // printf("src: %p, dest: %p\n", _src, _dest);
  for(size_t i = 0; i < num; i++)
  {
    if(!check_ptr_in_user_space(_src)) return false;
    int res = get_user(_src);
    if(res == -1) return false;
    // printf("%02hhx ", res);
    *_dest = (uint8_t)res;
    _dest++;
    _src++;
  }
  // printf("\n");
  return true;
}

/* Only checks whether its in the user space */
bool
check_ptr_in_user_space(const void *ptr)
{
  return ptr < PHYS_BASE;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
get_args(int *sp, int *dest, size_t num)
{
  for(size_t i = 0; i < num; i++)
  {
    int *src = sp + i + 1;
    if(!check_ptr_in_user_space(src)) sys_exit(-1);
    if(!get_user_bytes(dest + i, src, 4)) sys_exit(-1);
    // dest[i] = *src;
    // printf("Arg %u: %x\n", i, dest[i]);
  }
  // printf("ESP: %p\n", sp);
}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("system call!\n");
  
  int arg[4];
  // hex_dump(0,f->esp,100,true);
  if(!check_ptr_in_user_space(f->esp))
    sys_exit(-1);

  //printf("시스템콜 디버깅!\n");
  switch(*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_EXIT:
      get_args(f->esp, arg, 1);
      sys_exit(arg[0]);
      break;
    case SYS_EXEC:
      get_args(f->esp, arg, 1);
      f->eax = sys_exec((const char *)arg[0]);
      break;
    case SYS_WAIT:
      get_args(f->esp, arg, 1);
      f->eax = sys_wait((pid_t)arg[0]);
      break;
    case SYS_CREATE:
      get_args(f->esp, arg, 2);
      f->eax = sys_create((const char *)arg[0], (unsigned)arg[1]);
      break;
    case SYS_REMOVE:
      get_args(f->esp, arg, 1);
      f->eax = sys_remove((const char *)arg[0]);
      break;
    case SYS_OPEN:
      get_args(f->esp, arg, 1);
      f->eax = sys_open((const char *)arg[0]);
      break;
    case SYS_FILESIZE:
      get_args(f->esp, arg, 1);
      f->eax = sys_filesize(arg[0]);
      break;
    case SYS_READ:
      get_args(f->esp, arg, 3);
      f->eax = sys_read(arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_WRITE:
      get_args(f->esp, arg, 3);
      f->eax = sys_write(arg[0], (const void *)arg[1], (unsigned)arg[2]);
    case SYS_SEEK:
      get_args(f->esp, arg, 2);
      sys_seek(arg[0], (unsigned)arg[1]);
      break;
    case SYS_TELL:
      get_args(f->esp, arg, 1);
      f->eax = sys_tell(arg[0]);
      break;
    case SYS_CLOSE:
      get_args(f->esp, arg, 1);
      sys_close(arg[0]);
      break;
    default:
      sys_exit(-1);
  }
  
  //TODO: 구현 완료시 exit 삭제
  //thread_exit ();
}

void
sys_halt()
{
  shutdown_power_off();
  NOT_REACHED();
}

void
sys_exit(int status)
{
  struct thread *cur = thread_current();
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
  NOT_REACHED();
}

pid_t
sys_exec(const char *cmd_line)
{
  // TODO
  return -1;
}

int
sys_wait(pid_t pid)
{
  // TODO
  return -1;
}

bool
sys_create(const char *file, unsigned initial_size)
{
  if(file == NULL || !check_ptr_in_user_space(file))
    sys_exit(-1);
  return filesys_create(file, initial_size);
}

bool
sys_remove(const char *file)
{
  if(file == NULL || !check_ptr_in_user_space(file))
    sys_exit(-1);
  return filesys_remove(file);
}

int
sys_open(const char *file)
{
  // TODO
  return -1;
}

int
sys_filesize(int fd)
{
  // TODO
  return 0;
}

int
sys_read(int fd, void *buffer, unsigned size)
{
  // TODO
  return -1;
}

int
sys_write(int fd, const void *buffer, unsigned size)
{
  if(!check_ptr_in_user_space(buffer))
    sys_exit(-1);
  if(fd == 0)
  {
    //printf("예외\n");
    sys_exit(-1);
  }
    
  else if(fd == 1)
  {
    // printf("출력 가능\n");
    //printf("%s",(char *)buffer);
    putbuf(buffer, size);
    return size;
  }
  else
  {
    /* TODO: Implement this */
    return -1;
  }
}

void
sys_seek(int fd, unsigned position)
{
  // TODO
  return;
}

unsigned
sys_tell(int fd)
{
  // TODO
  return -1;
}

void
sys_close(int fd)
{
  // TODO
  return;
}