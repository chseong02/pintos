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

OS에서 인터럽트 처리 중 `sema_down`에 의한 스레드 Block이 발생할 경우 OS가 멈추는 등 예기치 못한 결과를 일으킬 수 있으므로 인터럽트 중에는 `sema_down`이 실행되지 않도록 `ASSERT`문을 통해 검사한다. 또한 유사한 이유로 스레드 리스트를 변경하는 중 인터럽트가 발생하여 예기치 못한 동작이 일어날 가능성이 존재하므로 스레드 리스트를 변경하는 도중에는 인터럽트를 비활성화시켜 Atomic하게 실행되도록 한다.

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

### Lock
```c
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
  };
```
Lock은 기본적으로 Semaphore를 기반으로 하되, Lock을 설정한 스레드와 해제하는 스레드가 동일함을 추가적으로 검증한다. 따라서 멤버 변수에 현재 해당 lock을 설정한 스레드가 무엇인지를 저장하는 `holder` 멤버 변수가 추가적으로 선언되었다.

```c
void lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}
```
Lock을 초기화한다. 현재 Lock을 소유 중인 `holder`가 없는 상태이고 Semaphore의 값을 1인 상태로 설정한다.

```c
void lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
}
```
Lock을 획득하는 함수. 앞서 설명한 인터럽트 여부 확인과 더불어 현재 Lock을 보유 중인 스레드가 다시 Lock 획득을 시도하고 있지는 않은지 검사한 뒤 `sema_down`을 호출한다. 이미 해당 Lock을 다른 스레드가 소유 중일 경우 Semaphore의 값이 0일 것이므로 Semaphore가 1이 될 때까지 Block될 것이고, 소유 중이었던 스레드가 Lock 소유를 해제할 경우 현재 스레드가 Unblock되어 Lock 소유를 시도할 것이다. 소유에 성공했거나 기존 소유자가 없었을 경우 lock의 holder를 현재 스레드로 변경한다.

```c
bool lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}
```
`sema_try_down`과 마찬가지로 Lock 소유를 1회 시도한 뒤 성공 여부를 반환한다.

```c
void lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  lock->holder = NULL;
  sema_up (&lock->semaphore);
}
```
Lock 소유를 해제한다. `lock_acquire`와는 반대로, 해제를 시도하고 있는 스레드가 Lock을 소유했던 스레드가 맞는지를 검사한다. `lock->holder`를 초기화한 뒤 `sema_up`으로 Semaphore의 값을 다시 1로 복구한다.

### Condition variable

```c
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };
```
`waiters` 리스트는 `wait`하려는 스레드가 생성한 Semaphore들을 저장한다.

```c
void cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}
```
`waiters` 리스트를 초기화함으로써 condition 객체를 초기화한다.

```c
void cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}
```
Conditional variable에 의해 스레드를 wait하는 동작을 정의한다. Semaphore 하나를 추가로 정의하여 Conditional variable의 리스트에 저장한 뒤, Lock을 해제한 후 (호출 시 해당 스레드가 Lock을 소유하고 있어야 한다) `sema_down`을 시도한다. 해당 Semaphore는 다른 스레드에서 `cond_signal` 혹은 `cond_broadcast`를 호출하여 `sema_up`되기 전까지 `down` 상태를 유지할 것이므로 `cond_wait`을 호출한 본 스레드는 `wait` 상태에 머무르게 된다. `sema_down`이 완료된 후에는 다시 Lock을 획득한다. Lock 관련 동작을 수행해야 하므로 해당 동작 역시 인터럽트 핸들 로직 중 수행되지 않도록 `ASSERT`문으로 확인해준다.

```c
void cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}
```
해당 스레드가 Lock을 보유하고 있고 인터럽트 중이 아닐 경우, `cond`의 Semaphore list중 가장 앞의 Semaphore를 up하여 해당 Conditional variable에 의해 `wait` 상태에 있던 스레드를 깨운다.

```c
void cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
```
해당 스레드가 Lock을 보유하고 있고 인터럽트 중이 아닐 경우, `cond`의  Semaphore list에 있는 모든 Semaphore를 up하여 해당 Conditional variable에 의해 `wait` 상태에 있던 스레드들을 모두 깨운다.