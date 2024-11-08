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
  //printf ("system call!\n");

  //TODO: pointer 검증 필요할지도?
  printf("시스템콜 디버깅!\n");
  switch(*(uint32_t *)(f->esp)) {
    case SYS_EXIT:
    printf("나갑니다!\n");
    case SYS_HALT:
      
      thread_exit();
    default:
    printf("기본처리\n");
      ;
  }
  
  //TODO: 구현 완료시 exit 삭제
  //thread_exit ();
}
