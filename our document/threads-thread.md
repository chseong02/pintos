### 변수, 상수, 매크로, 구조체 
#### `THREAD_MAGIC`
```c
#define THREAD_MAGIC 0xcd6abf4b
```
`thread` 에서 kernel stack의 overflow를 감지할 때 사용하는 상수 값. `thread` 구조체 내 thread에 관한 정보를 저장하는 공간 끝(`magic`)에 해당 상수를 저장한다. 만일 kernel stack이 커짐에 따라 overflow되어 `magic` 값이 기존 `THREAD_MAGIC`에서 다른 값으로 변형되어 `magic` 값을 통해 kernel stack의 overflow 여부를 감지할 수 있다. 자세한 내용은 아래 `thread` 구조체에 대한 설명에서 확인할 수 있다.

#### `thread_status`
```c
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };
```
`thread` 구조체에서 `thread`의 상태를 나타낼 때 사용하는 열거형.
TODO: 스레드 각 상태 의미 배경지식 추가 필요

#### `tid_t`
```c
typedef int tid_t;
```
Thread를 구분하는 id를 위한 자료형(Thread identifier type)이다. 

#### `PRI_MIN, PRI_DEFAULT, PRI_MAX`
```c
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
```
스레드(`thread`)가 가질 수 있는 priority의 범위 및 기본 값을 나타내는 상수이다.  `PRI_MIN`, `PRI_MAX`는 각각 스레드의 우선순위(`thread`구조체에서 `priority`)의 하한과 상환이다. 값은 클수록 우선순위가 높은 것(더 중요한 것)이다. `PRI_DEFAULT`는 스레드 생성시(TODO: 기본 함수) 별도로 설정하지 않을 시 기본으로 설정되는 `priority` 값이다.

### `thread`
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
| `tid`      | `tid_t`              | 스레드를 서로 구별하는 id, 스레드 별로 유일한 값을 가진다.                                                                                                                                                      |
| `status`   | enum `thread_status` | 스레드의 현재 상태                                                                                                                                                                               |
| `name`     | `char`               | 디버깅용 스레드 이름                                                                                                                                                                              |
| `stack`    | `uint8_t`            | 해당 스레드의 stack을 가리키는 포인터<br>- 다른 스레드로 전환될 시 stack pointer를 해당 변수에 저장하여 이후에 thread가 다시 실행될 때 해당 pointer를 이용한다.                                                                             |
| `priority` | `int`                | 스레드 우선 순위(숫자가 클수록 높은 우선순위)                                                                                                                                                               |
| `allelem`  | struct `list_elem`   | 모든 스레드를 포함하는 double linked list를 위한 아이템 하나                                                                                                                                               |
| `elem`     | struct `list_elem`   | 모든 스레드를 포함하는 double linked list를 위한 아이템 하나                                                                                                                                               |
| `magic`    | `unsigned`           | `thread` 구조체의 가장 마지막 멤버 변수로, stack overflow를 감지하는 숫자. 항상 `THREAD_MAGIC`으로 설정되어 있다. 만약 kernel stack이 커지다 thread struct 부분까지 침범하게 되면 magic 숫자가 변경되게 되고 `THREAD_MAGIC`이 아니게 되어 이를 감지할 수 있다. |
pintos는 thread를 thread에 대한 정보를 나타내는 상단 코드의 `thread` 구조체와 kernel stack으로 이루어진다. thread는 이 둘을 4kB의 page 내에 저장한다. 각 스레드는 4kB page 내 아래 구조와 같이 저장된다. 
```c
        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+
```
어떤 스레드에 대한 stack을 제외한 정보를 가지고 있는 `thread` 구조체는 해당 page의 가장 바닥(0)부터 위로 값을 저장한다. `thread`를 저장한 부분의 끝에는 `THREAD_MAGIC`가 저장된 `magic`이 저장된다.
반대로 해당 스레드의 커널 스택은 `thread`가 저장된 부분의 반대편인 page의 가장 천장(4kB)부터 저장하여 아래로 늘어난다.
만약 kernel stack이 길어지게 되어 `thread` 저장 공간을 침범하게 되면 `thread` 저장 부분의 가장 끝에 있는 `magic` 값이 기존 `THREAD_MAGIC`에서 다른 값으로 변경되게 되고 이를 통해 kernel stack overflow를 감지하는 것이다.
이처럼 커널 스택을 포함한 스레드가 저장되는 공간은 4kB이기 때문에 스레드에서 이보다 더 큰 non-static local variables를 저장하고 싶다면 `malloc()`, `palloc_get_page()` 같은 dynamic allocation을 사용해야 한다.

#### `thread_mlfqs`
```c
extern bool thread_mlfqs;
```
mlfqs(Multi level feedback queue scheduler)를 사용할지 여부를 담는 전역 변수이다.
기본 값은 false로 설정되는데 해당 값이 false라면(TODO: 누구로 인해?) 이는 round-robin scheduler(TODO: 어디서 이 변수를 참조?)를 사용하고 true라면 Multi level feedback queue scheduler를 사용한다.
해당 변수는 command-line option인 `-mlfqs` 옵션이 주어질 때,  main함수에서 init 중 `parse_options`에 의해 True로 설정된다.

#### `ready_list`
```c
static struct list ready_list;
```
`thread_status`가 `THREAD_READY`인 프로세스 리스트를 저장하는 `list` 구조체 

#### `all_list`
```c
static struct list all_list;
```
지금까지 생성되었던 모든 프로세스를 저장하는 리스트를 저장하는 `list` 구조체

#### `idle_thread`
```c
static struct thread *idle_thread;
```
`ㅓ`

#### `initial_thread`
```c
static struct thread *initial_thread;
```
처음으로 생성되고 running되는 스레드의 `thread`를 가리키는 포인터 변수
즉 커널 실행시 처음으로 작동하는 `threads/initc.c`의 `main` 함수에 해당되는 스레드를 가리킨다.

#### `tid_lock`
```c
static struct lock tid_lock;
```
`allocate_tid()`를 수행할 때 사용되는 `lock`이다.
`allocate_tid()`에서 `tid` 즉 Thread마다 고유한 identifier를 지정할 때 tid를 할당할 때마다 이후 할당할 tid 값을 1씩 증가시키는데 증가, 할당 과정 중 다른 스레드의 개입을 막기위한 `lock`이다. 
더 자세한 설명은 `allocate_tid` 를 참고하면 된다.

#### `kernel_thread_frame`
```c
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };
```

#### `idle_ticks, kernel_ticks, user_ticks`
실제 기능을 담당하는 것이 아닌 분석을 위한 변수들이다,
각각 idel Thread 보낸 총 tick, 커널 스레드에서 소요한 총 tick, user program에서 사용한 총 tick을 의미한다.
### `thread_ticks`
마지막으로 yield한 이후 timer tick이 얼마나 지났는지 정하는 static 변수이다.

### 함수 소개
#### `thread_init(void)`
```c
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}
```
> 스레드 시스템을 초기화하는 함수

스레드를 초기화하는 과정이기에 우선 Interrupts가 비활성화된 상태에서 실행되어야만 한다.
우선 스레드 시스템과 연관된 `list` 구조체 변수들(`ready_list`,`all_list`) 및 스레드 id 할당과 관련된 `lock` 구조체 변수인 `tid_lock`을 초기화한다. 이후에는 현재 해당 함수를 실행 중인 스레드를 최초의 스레드(`initial_thread`)로 생각하여 본 스레드의 주소를 저장한다. 또한 `init_thread`를 통해 이 함수를 실행 중인 스레드, 즉 `initial_thread`의 `thread` 객체의 `name`을 "main"으로 지정하고 `priority`도 기본 값인 `PRI_DEFAULT`로 지정해준다. 이미 해당 스레드는 `thread_init`을 실행 중이기에  `status`는 `THREAD_RUNNING`으로 지정하고 `allocate_tid`를 통해 새로운 `tid`를 얻어 `initial_thread`의 `tid`로 지정한다. 이로써 해당 함수가 완료되면 본 스레드의 정보들이 올바르게 초기화 된다. **`thread_create`를 통해 새로운 `thread`를 생성하기 전에 page allocator를 초기화해주어야만 한다.**

#### `thread_start(void)`
```c
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}
```

> interrupt를 활성화함으로써 preemptive(선점형) 스레드 스케줄링을 시작하는 함수.

interrupt를 활성화함으로써 preemptive(선점형) 스레드 스케줄링을 시작하는 함수로 idle thread도 생성한다. `idle_started`라는 `semaphore`를 생성하고 초기화한다. 
또한 `thread_create`를 통해 idle이라는 이름을 가지고 `priority`는 `PRI_MIN`으로 가장 낮은 thread를 생성한다. 이후에는 `intr_enable`을 통해 interrupts를 허용한다.  TODO: thread create 동작 방식,. sema down

#### `thread_tick(void)`
```c
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}
```

> timer tick마다 timer interrupt 핸들러에 의해 호출되는 함수이다.

현재 running 중인 스레드의 종류(idle, user program, kernel 스레드인지)에 따라 각 스레드가 소요한 시간을 계산하는 variable(`idle_ticks`,`user_ticks`,`kernel_ticks`)를 증가시켜 업데이트한다.
`thread_ticks`를 증가시키고 만약 `TIME_SLICE` 보다 크거다 같다면 `intr_yield_on_return`을 호출함으로써 새로운 프로세스에게 yield 하도록 한다. 즉 스케줄링이 일어나게 된다. 타이머에 의해 주기적으로 `TIME_SLICE` 틱마다 스케줄링이 일어난다.

#### `thread_print_stats`
```c
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}
```

> 스레드의 종류별 소요 tick을 출력하는 함수

#### `thread_create(name, priority, function, aux)`
```c
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);
  ```
  
> 매개변수로 받은 name과 priority를 가지고 function에 aux 인수를 넘겨 실행하는 스레드를 생성하고 ready 큐에 추가한 뒤 새로 생성한 스레드의 id를 반환하는 함수

`name`: 새로 생성할 스레드의 이름
`priority`: 새로 생성할 스레드의 priority
`function`: 스레드가 실행할 함수
`aux`: `function` 실행 때 넘길 argument

```c
  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
```
`palloc_get_page`를 통해 스레드가 사용할 메모리, 페이지를 할당한다.
`init_thread`를 통해 할당 받은 메모리를 0으로 초기화하고 해당 공간에 스레드의 정보를 담은 `thread` 구조체 변수 값을 입력받은 매개변수를 바탕으로 설정한다. 또한 이렇게 생성된 `thread`의 `status`는 처음에는 `THREAD_BLOCKED` 이다. 생성한 스레드를 `all_list`에도 추가한다. 이후 `allocate_tid()`를 통해 스레드의 id를 생성 후 집어 넣는다.
```c
  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}
```

각 스레드가 할당받은 메모리 중 스택의 최상단부터 `alloc_frame`을 이용해  `kernel_thread_frame`, `switch_entry_frame`, `switch_threads_frame` 공간을 차례로 할당한다. 또한 각 frame의 정보를 채워 넣어 초기화한다.
스레드 생성 및 초기화가 완료되었으므로 새로 생성한 스레드를 unblock한 뒤 스레드의 tid를 반환한다.
#### `thread_block(void)`
```c
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}
```

> 현재 실행 중인 스레드의 `status`를 `THREAD_BLOCKED`로 변경하여 블록한 뒤 `schedule()`를 통해 스케줄링하는 함수

`!intr_context()`를 통해 현재 external interrupt를 처리 중이 아님을 확인한다. 또한 해당 함수 실행을 위해서는 interrupt가 비활성화되어 있어야 한다.
#### `thread_unblock(struct thread *t)`
```c
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}
```
> 입력받은 매개변수가 가리키는 `thread`를 unblock시키는 함수

interrupt를 비활성화한 뒤 입력받은 스레드를 다시 `THREAD_READY`상태의 스레드 리스트인 `ready_list`에 집어넣고 스레드의 `status`를 `THREAD_READY`로 변견한 뒤 원래 interrupt 레벨로 복구한다.

#### `thread_name`, `thread_tid`

>현재 실행중인 스레드의 name, tid를 출력하는 함수

#### `thread_current`
```c
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}
```

> 현재 running 중인 스레드의 주소를 반환하는 함수. 

추가로 현재 실행 중인 것이 정말 올바른 형식의 스레드인지 실행 중인 스레드가 맞는지 등의 정합성 검사를 포함한다.

