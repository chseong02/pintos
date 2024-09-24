	```
devices/timer.c
threads/fixed-point.h
threads/synch.c
threads/thread.c
threads/thread.h
```
• Thread system initialization
• Thread creation
• Context switching
• Busy wating
• Round-robin scheduler
• Synchronization primitives (i.e., semaphores, locks, condition variables)


### Loading
핀토스가 실행된다면 가장 처음으로

### Thread
```c
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list.*/
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };
```

| 변수         | 자료형                  | 설명                                                                                                                                                                                       |
| ---------- | -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `tid`      | `tid_t`              | 스레드를 서로 구별하는 id                                                                                                                                                                          |
| `status`   | enum `thread_status` | 스레드의 현재 상태                                                                                                                                                                               |
| `name`     | `char`               | 디버깅용 스레드 이름                                                                                                                                                                              |
| `stack`    | `uint8_t`            | 해당 스레드의 stack을 가리키는 포인터<br>- 다른 스레드로 전환될 시 stack pointer를 해당 변수에 저장하여 이후에 thread가 다시 실행될 때 해당 pointer를 이용한다.                                                                             |
| `priority` | `int`                | 스레드 우선 순위(숫자가 클수록 높은 우선순위)                                                                                                                                                               |
| `allelem`  | struct `list_elem`   | 모든 스레드를 포함하는 double linked list를 위한 아이템 하나                                                                                                                                               |
| `elem`     | struct `list_elem`   | 모든 스레드를 포함하는 double linked list를 위한 아이템 하나                                                                                                                                               |
| `magic`    | `unsigned`           | `thread` 구조체의 가장 마지막 멤버 변수로, stack overflow를 감지하는 숫자. 항상 `THREAD_MAGIC`으로 설정되어 있다. 만약 kernel stack이 커지다 thread struct 부분까지 침범하게 되면 magic 숫자가 변경되게 되고 `THREAD_MAGIC`이 아니게 되어 이를 감지할 수 있다. |
pintos는 thread를 thread에 대한 정보를 나타내는 상단 코드의 `thread` 구조체와 kernel stack으로 이루어진다. thread는 이 둘을 저장하기 위해 4kB의 page를 할당받는다.

### Init
Pintos 커널은 `threads/init.c`의 `main()`함수로 시작한다. `main()`은 주로 pintos의 다른 다양한 모듈들의 초기화 함수 호출로 이루어진다. 
1.`bss_init()`
0으로 초기화되어야 하는 세그먼트들을 BSS라고 부르는데 해당 함수는 kernel의 bss를 모두 0으로 초기화시킨다. 
초기화해야하는 세그먼트들의 시작과 끝 주소는 `kernel.lds.S`에 포함되어 있다.

2.`read_command_line()`
command의 argument들이 담긴 주소의 값을 확인하여 argument가 담긴 하나의 문자열을 각argument로 쪼개서 argument `char*`로 이루어진 `char**` 형태로 반환한다.

3.`parse_options(argv)`


4.`thread_init()`
`lock_init()` 의 인수로 `tid_lock`의 주소를 넘겨 넘긴 주소에 담긴 `lock`의 `holder`를 `NULL`로 초기화하고 `sema_init`을 통해 `lock`의 `sempahore`를 1로 초기화해준다. 
이 때 `tid_lock`은 전역변수`next_tid`에 기반하여 Thread의 `tid`에 사용될 새로운 `tid`를 만드는 동안 다른 스레드에서 `tid`를 변형하지 못하도록 락의 상태를 저장하는 변수이다.(TODO: `allocate_tid` 설명 추가)

`list_init()`을 통해 `ready_list`,`all_list`의 `head`와 `tail`을 초기화시킴으로써 준비된 스레드 목록, 전체 스레드 목록을 초기화시켰다.

이후 현재 init을 수행중인 스레드의 주소를 전역변수 `initial_thread`에 저장함으로써 `main()`을 수행 중인 태초의 핵심이 되는 스레드가 무엇인지 저장하였다.
그리고 `init_Thread`를 통해 해당 스레드를 `main`이라 명명했다.
`initial_thread`의 `status`를 `THREAD_RUNNING`으로 초기화하여 현재 실행 중임을 나타내게 하고, `allocate_tid()`를 통해 `tid`를 설정해줌으로써 `main` 스레드의 정보를 설정해주었다.

5.`console_init()`
static lock 변수 `console_lock`을 `lock_init()`을 통해 초기화해주고 `use_console_lock`을 true로 변경함으로서 `console_lock`이 초기화되어 사용될 수 있는 상태임을 나타내게 했다.
`console_lock`은 동시에 일어난 `printf` 등을 이용한 콘솔 출력이 섞이지 않도록 하는 `lock` static 변수이다.

6.`palloc_init()`
7.`malloc_init()`
8.`paging_init()`

9.`intr_init()`
interrupt controller 초기화(TODO: 설명 추가 필요)
10.`timer_init()`
`pit_configure_channel`을 통해 plt가 channel 0 (interrupt line)에 100Hz의 주기로 주기적으로 신호를 보내도록 설정을 변경한다.
`intr_register_ext`를 통해 0x20 interrupt에 대해 `timer_interrupt` 핸들러가 작동하도록 `"8254 Timer"`라는 이름으로 등록한다.

11.`kbd_init()`
12.`input_init()`

13.`thread_start()`
14.`serial_init_queue()
15.`timer_calibrate()`
16.`run_actions(argv)`
17.`shutdown()`
18.`thread_exit()`
### Etc
`struct list`
`struct lock`
``