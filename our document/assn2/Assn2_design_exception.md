### Exception
user process가 privileged(권한이 필요한) 또는 prohibited(금지된) operation을 실행하려고 하면, 커널에 exception이나 fault로 trap된다. 실제 Unix-like OS에서는 대부분의 interrupt는 user process에게 signal의 형태로 전달되는데 핀토스 현재 구현상 모든 exception은 단순히 메세지 출력 후 프로세스 종료한다. 현재는 Page fault 또한 다른 exception과 동일하게 다루지만 이를 변경하는 것이 이번 assn 목표이다.

```c
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */
```
Page fault error code는 뒤에서부터 비트 단위로 오류의 이유를 설명한다.
맨 뒷자리
- 0: 존재하지 않는 페이지, 1: 접근 권한 없음.
뒤에서 두번째
- 0: read할 때, write할 때
뒤에서 세번째
- 0: kernel, 1: user process

```c
static long long page_fault_cnt;
```
처리된 page fault 횟수를 나타내는 전역 변수

#### `exception_init(void)`
```c
void
exception_init (void) 
{
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}
```
> page fault를 포함해 각종 exception에 대한 핸들러 함수를 등록하는 함수이다.

해당 함수는 `syscall_init()`과 함께 `threads/init.c`의 `main()`에서 커널을 초기화할 때 호출되어 `exception`에 대한 핸들러를 등록한다.(`syscall_init`은 interrupt 중 system call에 대한 핸들러 등록)
`intr_register_int`를 통해 `BP, OF, BR` exception에 대한 interrupt handler 함수로  `kill`을 등록한다. 이 exception은 dpl이 3으로 user program에 의해서 명시적으로 발생할 수 도 있다.
`intr_register_int`를 통해 `DE, DB, UD, NP, SS, NM, GP, MF ,XF` exception에 대한 interrupt handler 함수로  `kill`을 등록한다. 이 exception은 dpl이 0으로 user process에 의해 명시적으로 발생할 수 없으나 조건에 따라 간접적으로 발생할 수 있다.
위 exception에 대해서는 항상 핸들러 함수인 `kill`을 호출해 해당 user process를 죽인다.
마지막으로 page fault에 대한 핸들러 함수로 `page_fault`를 등록하였고 이는 dpl=0으로 user process가 직접적으로 발생시킬 수 없으며 `INTR_OFF`로 interrupt가 off된 상태로 핸들링한다. 이는 fault address가 CR2에 저장되고 보존되어야 하기 때문이다.

#### `exception_print_stats(void)`
```c
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}
```
> page fault 발생 횟수를 출력하는 함수이다.

#### `kill(struct intr_frame *f)`
```c
static void
kill (struct intr_frame *f) 
{
  switch (f->cs)
    {
    case SEL_UCSEG:
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}
```
> page fault를 제외한 exception에 대한 handler 함수이다.

page fault를 제외한 exception에 대한 handler 함수로 사용된다.(page fault 핸들러 함수 내부에서 해당 함수를 호출함.)
핸들러 함수인 해당 함수를 호출할 때 인수로 들어온 `intr_frame`의 `cs`, 즉 interrupt frame의 eip의 code segment(interrupt 발생 위치)를 보고 user와 kernel 중 어디에서 exception이 발생하였는지 구별한다. `SEL_UCSEG`, user code segment라면 user exception이므로 관련 에러 메세지를 출력하고 디버깅을 위해 interrupt frame을 덤프시킨다. 이후 `thread_exit`을 통해 해당 스레드를 종료시킨다. 즉 해당 user process는 죽는다.
만약 `cs`가 `SEL_KCSEG`라면 kernel에서 발생한 exception이므로 interrupt frame을 덤프시키고 패닉한다. 
이 외의 경우는 발생할 수 없지만 패닉한다.

#### `page_fault(struct intr_Frame *f)`
```c
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  asm ("movl %%cr2, %0" : "=r" (fault_addr));
  
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}
```
> page fault exception에 대한 handler 함수이다.

page fault가 일어나게 한 가상 주소는 CR2 레지스터에 저장되어 있으며 `"movl %%cr2, %0" : "=r"`을 통해 얻어 `fault_addr`에 저장한다.
`fault_addr`을 올바르게 얻었으므로 `intr_enable`을 통해  interrupt를 다시 활성화 한다. page fault가 발생했기에 `page_fault_cnt`를 1 증가시켜준다.
핸들러 함수의 인수로 받은 interrupt frame의 `error_code`는 에러 코드를 포함하고 있고 page fault의 에러코드는 위에서 명시한 대로 오류의 이유에 대해 설명한다. 이를 이용해 세가지 기준에 대해 오류 원인을 추출하고 이에 관해 메세지로 출력한다.
마지막으로 `kill`을 호출해 user process를 죽인다.
page fault의 exception 핸들링과 관련해 가상 메모리를 올바르게 구현을 완료하기 위해서는 해당 함수를 수정해야 할 필요가 있다. 


