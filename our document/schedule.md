## Scheduler

```c
/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}
```
`schedule` 함수는 현재 실행중인 `cur` 스레드와 다음으로 실행할 `next` 스레드를 `switch_threads`를 통해 컨텍스트 스위칭한다. 

`switch_threads` 함수는 `switch.S`에 정의되어있는 x86 어셈블리로 작성된 함수로, 두 스레드의 레지스터와 스택 공간 정보를 뒤바꾼 뒤 바꾸기 이전 기존 스레드의 정보를 반환한다.
```c
  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
```
`schedule` 함수를 실행한 `cur` 스레드가 `switch_threads` 함수를 호출한 뒤 스레드 정보를 반환받는 것은 `next`로 컨텍스트가 넘어간 뒤 다시 `cur`로 스위칭 된 직후일 것이므로 `prev` 스레드 정보는 `cur`의 것과 동일하다.

```c
/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}
```
다음에 실행할 스레드인 `next`를 선택할 때 호출되는 함수인 `next_thread_to_run`은 현재 `run queue`로 사용되고 있는 스레드 리스트에서 가장 앞에 있는 스레드 하나를 `pop`해 반환한다.

```c
/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}
```
```c
/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void thread_unblock (struct thread *t) 
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
또한 현재 스레드를 `run queue`에 넣는 작업이 필요한 `thread_yield`, `thread_unblock` 등과 같은 경우에는 일괄적으로 `list_push_back (&ready_list, &t->elem);`를 이용하여 `run queue` 리스트의 뒤쪽 끝에다 `push`하고 있다. 위의 `pop` 동작과 연관지어 봤을 때 현재 `run queue`는 우선순위 없이 단일 큐를 이용해 먼저 `pus`h된 스레드가 먼저 `pop`되는 `round-robin` 형식을 사용하고 있는 것을 알 수 있다. 이를 우선순위가 높은 스레드가 먼저 `pop`되도록 `priority scheduler`로 변경하는 것이 이번 Assn1의 목표 중 하나이다.