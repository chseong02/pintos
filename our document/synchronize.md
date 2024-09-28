## Synchronization Variables
멀티스레드 환경에서 Concurrency를 보장하기 위해 필요한 Semaphores, Locks, Condition Variables 구조체에 대한 정의는 `/threads/synch.h`, `/threads/synch.c`에 정의되어 있다.

### Semaphore
```c
/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };
```
현재 semaphore의 값과 후술할 `sema_down`에 의해 Block된 스레드들을 관리하는 리스트를 멤버 변수로 가진다.

```c
void sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}
```
`semaphore` 구조체를 선언한 뒤 `sema_init` 함수를 통해 현재 semaphore의 값과 스레드 리스트를 초기화해줘야 한다.

```c
void sema_down (struct semaphore *sema) 
{
  while (sema->value == 0) wait;
  sema->value--;
}
```
```c
void sema_up (struct semaphore *sema) 
{
  sema->value++;
}
```

`sema_down`, `sema_up`의 정의에 따른 의사 코드는 위와 같다. 하지만 `sema_down`을 위와 같이 구현할 시 `sema->value`가 0이 되기 전까지 루프를 도는 Busy Waiting 상태가 된다. 

```c
void sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}
```
```c
void sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) 
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  sema->value++;
  intr_set_level (old_level);
}
```

따라서 본 코드에선 `sema_down`을 시도하는 스레드를 Block하고 `semaphore` 구조체 내부의 스레드 리스트에서 Block된 스레드들을 관리한다. 이 스레드들은 `sema_up`이 호출될 시 일괄적으로 Unblock된다.

OS에서 인터럽트 처리 중 `sema_down`에 의한 스레드 Block이 발생할 경우 OS가 멈추는 등 예기치 못한 결과를 일으킬 수 있으므로 인터럽트 중에는 `sema_down`이 실행되지 않도록 `ASSERT`문을 통해 검사한다. 또한 유사한 이유로 스레드 리스트를 변경하는 중 인터럽트가 발생하여 예기치 못한 동작이 일어날 가능성이 존재하므로 스레드 리스트를 변경하는 도중에는 인터럽트를 비활성화한다.

```c
bool sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}
```
`sema_try_down` 함수는 `sema->value`가 0보다 커질 때까지 대기하는 대신 `down`을 1회 시도하고 성공/실패 여부를 반환한다. 따라서 실행 중 스레드가 Block되지 않는다.