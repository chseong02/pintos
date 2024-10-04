## Timer

```c
void timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
}
```

기존 `timer_sleep` 함수의 구현 방식은 함수가 호출된 직후부터 `ticks` 틱이 경과할 때까지 스레드를 실행 대기중인 스레드 리스트로 되돌리는 `thread_yield`를 반복 호출한다. 이로 인해 스레드를 sleep시키는 함수임에도 불구하고 CPU가 쉬지 않고 작동하며 sleep 만료 여부를 확인하는 비효율적인 `busy waiting` 방식으로 구현되어있어, 이를 `thread_block`을 이용한 `sleep-awake` 방식의 보다 효율적인 방법으로 개선하는 것이 Assn1의 목표 중 하나이다.

```c
/* Number of timer ticks since OS booted. */
static int64_t ticks;
```
```c
/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}
```
```c
/* Returns the number of timer ticks since the OS booted. */
int64_t timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}
```
Timer는 OS 부팅 시부터 `ticks` 전역 변수를 통해 현재까지 흐른 시간을 갱신하여 관리하며, 이를 이용해 `timer_ticks`를 첫 번째 호출한 시점과 두 번째 호출한 시점의 `ticks` 값의 차이를 계산하여 경과 시간을 알아내는 등의 동작을 구현할 수 있다.

