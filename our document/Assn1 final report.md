
# Pintos Assn1 Final Report
Team37 20200229 김경민, 20200423 성치호 

## 구현
목표 달성을 위해 추가로 구현, 수정한 코드에 대한 설명으로 Design Report과 달라진 점도 포함하고 있다.
### Alarm Clock
design report에서 계획한 구현에서 크게 벗어나지 않았다.
#### `sleep_list` in `devices/timer.c`
```c
/* List of sleep processes.  Processes are added to this list
   when they start sleeping due to timer_sleep() and removed when they wake up. */
static struct list sleep_list;
```
`list` struct 변수인 `sleep_list`를 `timer.c`에 추가하였다. 해당 변수는 현재 sleep하고 있는 `thread`에 대한 리스트로 아래에서 추가한 `struct thread`의 `sleep_elem`을 이용해 list를 구성하고 있다. 기존 계획에는 `thread.c`에 추가하려고 하였으나 기구현된 연관된 함수인  `timer_sleep`이 `timer.c`에 있기에 `sleep_list` 또한 `timer.c`에서만 사용하는 static 변수로 추가하였다.
```c
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  /* Initialize for timer_sleep() */
  list_init(&sleep_list);
}
```
task를 수행하기 전 초반 커널 초기화 때 `timer_init`을 호출하는데 이 때 함께 `sleep_list` 변수를 초기화하기 위해 `timer_init`에 `list_init(&sleep_list);`을 추가하였다. 계획과 달리 `sleep_list`를 `timer.c`에 추가하였기 때문에 초기화도 `thread_init`이 아닌 `timer_init`에 추가하였다.

#### `thread`
```c
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int64_t wake_up_tick;               /* Tick time thread need to wake up */
    
    ...
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    
    struct list_elem sleep_elem;        /* List element for sleep threads list */
	...
    unsigned magic;                     /* Detects stack overflow. */
  };
```
`struct thread`에 속하는 변수로 `wake_up_tick`을 추가하였다. 이는 해당 스레드가 sleep에서 벗어나야 할 tick에 대한 변수이다. 또한 `list_elem sleep_elem`을 추가하였다. 이는 `sleep_list` 리스트를 구성할 때 사용하기 위해 추가한 변수이다.
```c
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ...
  t->stack = (uint8_t *) t + PGSIZE; 
  t->wake_up_tick = 0;
  t->priority = priority;
  ...
}
```
`init_thread`로 thread 생성 후, `thread`를 초기화하는 함수에도 `wake_up_tick` 초기화 로직을 추가해주었다. `thread` 내 변수인 `wake_up_tick`를 `init_thread`에서 다른 변수를 초기화할 때 함께 0으로 초기화해준다. `sleep_list`에 해당 스레드가 포함되지 않는 이상 참조할 일이 없지만 안전, 통일성을 위해 추가해주었다.

#### `timer_sleep`
기존에 busy wait으로 구현된 `timer_sleep`을 thread block, unblock을 이용한 구현으로 개선하기 위해 기존 `yield`를 제거하고 `timer_sleep`을 다음처럼 변경하였다.
```c
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (intr_get_level () == INTR_ON);

  /* Don't have to sleep */
  if (ticks <= 0)
    {
      return;
    }
```
design report에서 계획한 것에 따라 구현하였다. 먼저 sleep 해야 하는 tick이 0보다 작거나 같다면 thread를 sleep할 필요가 없으므로 아무것도 하지 않고 리턴하면 된다. (어떻게 보면 잘못된 인수를 받은 것으로 생각할 수 있지만 테스트 내 0보다 작거나 같은 값을 이용한 호출이 있기에 Assert가 아닌 if문으로 예외 처리)
```c
  
  /* Thread block need Interrupt disabled */
  old_level = intr_disable ();
  cur->wake_up_tick = start + ticks;
  list_insert_ordered (&sleep_list, &(cur->sleep_elem), wake_up_tick_less, NULL);
  thread_block ();

  intr_set_level (old_level);
}
```
기존에는 현재 tick이 sleep 시작 시간 + tick보다 작으면 계속 `thread_yield`를 하는 구현이였다. sleep이 완료될 수 있는(wake up할 수 있는) tick (start+ tick)을 미리 계산한 이후 각 스레드의 변수 `wake_up_tick`에 설정해 집어넣어 주었다. 이후 `wake_up_tick_less`을 사용한`list_insert_ordered`를 이용해 해당 스레드를 `sleep_list`에 `wake_up_tick`을 오름차순으로 정렬하였을 때 기준 올바른 순서에 넣어주었다.
- 이처럼 `wake_up_tick`을 기준으로 오름차순으로 정렬한 순서에 맞게 `sleep_list` 리스트에 집어넣는 이유는 처음에 O(N)의 시간복잡도로 리스트에 집어 넣게 되면 `sleep_list`는 `wake_up_tick`이 가장 빠른, 가장 작은 값의 스레드가 올 것이 보장되기에 이후 틱마다 `check_wake_up`에서 `sleep_list`를 순회하며 일어나야할(unblock해야 할) 스레드를 정할 때 앞에서부터 확인하고 현재 시간보다 늦은 시간의 스레드가 나오면 바로 멈출 수 있다. 왜냐하면 정렬되어있기에 그 뒤의 스레드들은 순회를 멈춘 스레드보다 큰 `wake_up_tick`을 가짐이 보장되기 때문이다. 또한 삭제시에는 항상 맨 앞만 삭제할 것이 보장되기에 삭제에 O(1) 시간복잡도를 가지는 등 insert_ordered는 여러 이점이 있다.

마지막으로 현재 스레드(`timer_sleep`을 호출한 함수)를 block 함으로써 이 후 `check_wake_up`에 의해 wake up하기 전까지는 스케줄링되지 못하도록, cpu를 점유할 수 없도록 하였다. 
- block 되기에 `ready_list`에서도 제거되기에 스케줄 및 cpu 점유할 가능성이 전혀 없다.
- 이로 인해 기존의 busy wait보다 효율적인 wait이 가능해진다.

 `sleep_list`에는 thread가 추가되었지만 thread는 block 되지 않는다면 문제가 생기므로 해당 작업들을 수행할 때는 interrupt를 비활성화하고 sleep으로 인한 block 이후 unblock되어 되돌아올 스레드를 위해 마지막에 interrupt level을 되돌려 주었다. 

#### `wake_up_tick_less`
```c
static bool
wake_up_tick_less (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED) 
{
  const struct thread *a = list_entry (a_, struct thread, sleep_elem);
  const struct thread *b = list_entry (b_, struct thread, sleep_elem);
  
  return a->wake_up_tick < b->wake_up_tick;
}
```
`timer.c`에 `list_insert_ordered`에서 비교하는 함수로 사용하는  `wake_up_tick_less`를 추가하였다. 해당 함수는 2개의 `list_elem`인 `a_`, `b_`를 입력받아 `a_`가 포함된 `thread`의  `wake_up_tick`이 `b_`가 포함된 `thread`의 `wake_up_tick`보다 작을 때 true를 반환하고 그 반대일 때 false를 반환한다. 해당 함수를 이용해 `list_insert_ordered`를 하여 `sleep_list`가 항상 `wake_up_tick` 오름차순으로 list를 유지하게 한다.
- 해당 함수의 구조는 `list_less_func`와 동일하며 해당 함수는 A가 B보다 앞서야 할 때 True, 반대일 때 False를 반환하게 설계해야 한다.

#### `check_wake_up`
```c
void
check_wake_up (void)
{
  int64_t now = timer_ticks ();

  while (list_empty(&sleep_list) == false)
  {
    struct thread *t = list_entry (list_front (&sleep_list), struct thread, sleep_elem);
    if (t-> wake_up_tick > now)
    {
      return;
    }
    list_pop_front (&sleep_list);
    thread_unblock(t);
  }
}
```
`sleep_list`의 thread 중 일어나야 할 스레드를 검사하고 wake up해야하는 스레드들을 unblock하는 함수인 `check_wake_up`을 `timer.c`에 정의를 추가하고 `timer.h`에도 선언을 추가하여 다른 파일들에서 사용할 수 있도록 하였다. 먼저 현재 시간을 얻은 뒤, `sleep_list`가 빌 때까지 다음을 반복한다.
현재 `sleep_list`의 맨 앞 `list_elem`이 가리키는 스레드의 `wake_up_tick`이 현재 시간보다 크다면 아직 일어날 시간이 되지 않았으므로 함수를 리턴하여 while 및 함수 작동을 종료한다. 
- `sleep_list`의 맨 앞 아이템만 확인하고 조건에 만족하지 않으면 리턴하는 이유는 `sleep_list`는 `list_insert_ordered`를 사용해 insert했으므로 `wake_up_tick` 오름차순으로 정렬되었음이 보장되기 때문에 뒤의 아이템들의 스레드 또한 wake_up 조건에 만족하지 않을 것이 보장되기 때문이다.
만약 확인한 `list_elem`이 가리키는 스레드의 `wake_up_tick`이 현재 시간보다 작거나 같다면 이제 일어나야 할 시간이므로` list_pop_front`를 통해 해당 `list_elem`인 `sleep_list`
맨 앞 아이템을 제거하고 `list_elem`이 가리키는 스레드를 unblock한다. 이후 이를 반복한다.

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

  /* Check the thread that needs to wake up. */
  check_wake_up();

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}
```
다음처럼 Timer interrupt가 발생하면 핸들러 함수인 `timer_interrupt`에 의해 실행되는 `thread_tick`에 `check_wake_up()`을 추가하여 **틱마다** 현재 wake up 해야할 스레드가 있는지 확인하고 wake up할 수 있도록 하였다.

### Advanced Scheduler
모든 구현은 핀토스 레퍼런스 [B. 4.4BSD Scheduler](https://web.stanford.edu/class/cs140/projects/pintos/pintos_7.html#SEC131)을 기반으로 하였다.



#### Fixed-Point Real Arithmetic
`load_avg`와 `recent_cpu`는 실수 값을 가지는데, 일반적으로 사용되는 floating-point number는 연산이 느려 커널의 성능에 악영향을 줄 수 있기 때문에 고정 소수점 표기 방식을 이용해야만 한다. 이를 위해 `fp32` 자료형 및 이를 위한 연산 함수들을 구현하였다.

`fp32`는 디자인 레포트에서 채택한 다음 32비트의 fixed point 자료형 구조를 동일하게 채택하였으며 그 구현 또한 디자인 레포트에 명시된 바와 같다.
```c
#include <stdint.h>
/* FP32: Fixed-point number type
    0    00000000000000000 00000000000000
    |    |    Decimal    | | Fractional |
    sign +---(17 bits)---+ +-(14 bits)--+
*/
```
 첫번째 비트는 sign을 표기하게 하고, 이후 17비트는 decimal, 이후 14비트는 소수점 이후 부분을 표현하게 하였다. 쉽게 생각한다면 32비트로 표현된 어떠한 이진 수에 대해 소수점이 맨 뒤에 있다고 생각한 다음, 소수점을 한 칸씩 앞으로 이동시켜 14번 이동 시킨 결과를 표현한 것이며 동일한 원리로 구현하였다. 즉 기존 수를 $2^{14}$로 나누었다고 생각하면 되며 어떠한 bit sequence가 fp32에서 의미하는 값은 int에서 의미하는 값을 $2^{14}$로 나눈 값이다.
```c
/* 1 << 14 = 16384 */
#define FIXED_POINT_F 16384

typedef int32_t fp32;
```
Fixed-Point 자료형의 명칭은 `fp32`으로 정하였고 이는 fixed-point 32bit을 줄인 것이다.
부호가 있는 int 자료형인 int32_t를 base로 하였다. 이는 어떠한 32bit sequence가  `int32_t`에서 의미하는 수를 $2^{14}$로 나눈 수가 fp32가 의미하는 수와 동일하기 때문에 이것만 고려한다면 사칙 연산을 구현하기 편리하기 때문이다.
`FIXED_POINT_F`는 fp32에서 1.0을 의미하는 수이자 위에서 말한 int와 fp32 간의 관계에서 사용되는 상수이다.
```c
#define FP32_TO_FP(N)          (N * FIXED_POINT_F)
#define FP32_TO_INT(F)         (F / FIXED_POINT_F)
#define FP32_TO_INT_ROUND(F)   (F >= 0 ? \
    ((F + FIXED_POINT_F / 2) / FIXED_POINT_F) : ((F - FIXED_POINT_F / 2) / FIXED_POINT_F))
#define FP32_FP32_ADD(A, B)    (A + B)
#define FP32_FP32_SUB(A, B)    (A - B)
#define FP32_INT_ADD(A, B)     (A + FP32_TO_FP(B))
#define FP32_INT_SUB(A, B)     (A - FP32_TO_FP(B))
#define FP32_FP32_MUL(A, B)    (((int64_t) A) * B / FIXED_POINT_F)
#define FP32_INT_MUL(A, B)     (A * B)
#define FP32_FP32_DIV(A, B)    (((int64_t) A) * FIXED_POINT_F / B)
#define FP32_INT_DIV(A, B)     (A / B)
```
다음과 같이 int와 fp32 간 변환, fp32와 fp32간 사칙연산, fp32와 int간 사칙연산을 구현하였다.
이는 Pintos Reference(B. 4.4BSD Scheduler, B.6 Fixed-Point Real Arithmetic)에서 명시한 방식을 참고하여 동일하게 구현하였다.
#### `load_avg`
```c
/* BSD Scheduler */
/* Average number of threads that were in the "ready" and "running" states 
   over the past minute. */
static fp32 load_avg;
```
`thread.c`에 `load_avg` 변수를 추가하였다. 

#### recent_cpu
#### priority
#### tick



## 발전한 점
기존 busy wait으로 구현된 `timer_sleep`을 block, unblock을 이용한 구현으로 변경하면서 thread swtich 과정을 하나 감소킬 수 있었다. 또한 이는 스케줄링을 할 때도 이점이 있다. 자는건 빠지니까

## 한계
`struct thread`에 `wake_up_tick`, `sleep_elem`을 추가하며 Device 중 하나인 Timer와 Thread가 강하게 엮여 있는 코드 의존성을 가지게 된다. 이런 코드 의존성은 timer가 변경되었을 때 `thread`의 변화까지 야기한다. `thread`로부터 해당 변수들을 완벽히 분리하여 alarm clock 및 sleep을 구현할 수 있으면 더 좋았을 것이다.
정렬된 순서로 sleep list


## 배운 점
과제 목표를 달성하기 위해 코드를 작성하고 디버깅하는 과정에서 Assertion을 이용한 디버깅이 매우 유용함을 깨달았다. Assertion을 추가한다면 test에서도 어떤 assertion에 의해 실패하였는지 명확히 보여주기에 디버깅에 이점이 있었다. 그렇기에 어떤 작업을 수행하는 함수를 구현하기 전에 해당 함수가 interrupt를 비활성화한 상황에서 실행되어야 할지, 매개변수에 대해서는 어떤 검증을 해야할지 등을 생각하고 함수를 만드는 습관이 생기게 되고 이는 각 함수의 기능을 명확히 설계하고 이해하고 이후 디버깅하는데 도움이 되었다. 
