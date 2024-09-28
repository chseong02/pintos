### `main()` 함수
>(참고) User Program과 연관된 `ifdef USERPROG` 코드들은 현재 Assn1을 이해하고 분석하는데 제외하여도 이해 및 구현에 문제가 없기에 이를 제외하고 분석하였으며 관련된 설명을 작성하지 않았다.

해당 함수는 핀토스 커널의 초기화부터 command의 실행까지 핀토스의 메인 프로그램이다.


```c
int
main (void)
{
  char **argv;

  /* Clear BSS. */  
  bss_init ();
```
`bss_init()`을 통해 BSS를 모두 0으로 초기화한다.
```c
  /* Break command line into arguments and parse options. */
  argv = read_command_line ();
  argv = parse_options (argv);
```
`read_command_line()`를 통해 얻은 핀토스 command line 실행과 함께 입력된 arguments을 `\0`을 기준으로 split한 문자열 list를 `argv`에 저장한다. `parse_options(argv)`를 통해 인수로 제공한 `argv`에 구분되어 들어있던 옵션들을 확인하고 이에 따라 커널의 옵션을 변경한다. (TODO: 어떤 옵션 있는지) 또한 비 옵션 문자열이 시작되는 문자열들의 배열 내 시작 주소를 반환하여 이를 `argv`에 저장한다.
```c
  /* Initialize ourselves as a thread so we can use locks,
     then enable console locking. */
  thread_init ();
  console_init ();  
```

```c
  /* Initialize memory system. */
  palloc_init (user_page_limit);
  malloc_init ();
  paging_init ();
```

```c
  /* Initialize interrupt handlers. */
  intr_init ();
  timer_init ();
  kbd_init ();
  input_init ();
```

```c
  /* Start thread scheduler and enable interrupts. */
  thread_start ();
  serial_init_queue ();
  timer_calibrate ();
```

```c
  /* Run actions specified on kernel command line. */
  run_actions (argv);

  /* Finish up. */
  shutdown ();
  thread_exit ();
```