- 언제든 유저 프로세스가 kernel functionality 접근하고 싶으면 system call을 invoke(호출)함
- skeleton system call handler임.
- 지금은 단지 메세지 출력, user process terminate함.
- 이번 과제에 system call에 필요한 모든 거 여기에 추가하면 됨.

### System call
유저 프로세스가 kernel functionality에 접근하고 싶으면 system call을 invoke(호출)하면 된다. 현재 pintos에 구현된 것은 skeleton system call handler로 system call을 invoke할 시 handler에 의해 `"system call!"` 메세지를 출력하고 user process terminate한다. 이번 assn에서 이를 requirement에 맞게 변경하여 구현하면 된다.

#### `syscall_handler(struct intr_frame *)`
```c
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
```
> system call이 발생시 호출되는 handler 함수이다.

현재는 skeleton 함수로 단지 `"system call!"`을 출력하고 `thread_exit`을 통해 스레드를 종료시킴으로써 user process를 terminate한다.

#### `syscall_init(void)`
```c
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
```
> system call handler를 등록하여 system call 시스템을 세팅하는 함수

`threads/init.c`의 `main`에서 커널을 초기화할 때, 호출되어 system call 시스템을 세팅한다. 
`intr_register_int`를 통해 0x30 interrupt vector에 반응하고 user mode에서도 호출 가능하게 dpl을 3으로 설정하여 `syscall_handler`를 interrupt handler 함수로 등록한다. 
