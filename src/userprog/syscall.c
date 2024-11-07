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
get_args(int *sp, int *dst, int num)
{
  for(int i = 0; i < num; i++)
  {
    int *src = sp + num + 1;
    if(check_ptr_in_user_space(src)) dst[i] = get_user(src);
    else sys_exit(-1);
  }
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");
  
  int arg[4];

  //TODO: pointer 검증 필요할지도?
  switch(*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_EXIT:
      get_args(f->esp, arg, 1);
      sys_exit(arg[0]);
      break;
    default:
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
sys_exit(int exit_code)
{
  struct thread *cur = thread_current();
  printf("%s: exit(%d)\n", cur->name, exit_code);
  thread_exit();
  NOT_REACHED();
}