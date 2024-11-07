#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");

  //TODO: pointer 검증 필요할지도?
  switch(*(uint32_t *)(f->esp)) {
    case SYS_EXIT:
    case SYS_HALT:
      thread_exit();
    default:
      ;
  }
  
  //TODO: 구현 완료시 exit 삭제
  //thread_exit ();
}
