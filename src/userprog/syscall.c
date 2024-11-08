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

  //printf("시스템콜 디버깅!\n");
  switch(*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_EXIT:
      get_args(f->esp, arg, 1);
      sys_exit(arg[0]);
      break;
    case SYS_WRITE:
      //printf("프린트\n");
      get_args(f->esp, arg, 3);
      // hex_dump(0,f->esp,100,true);
      sys_write(arg[0], (const void *)arg[1], (unsigned)arg[2]);
    default:
    //printf("기본처리\n");
      ;
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