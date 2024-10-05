
### 변수 소개

### 함수 소개
#### `bss_init(void)`
```c
static void
bss_init (void) 
{
  extern char _start_bss, _end_bss;
  memset (&_start_bss, 0, &_end_bss - &_start_bss);
}
```
> BSS를 0으로 초기화하는 함수

먼저 bss란 "Blocked Started By Symbol"의 약자로 정적으로 할당된 변수를 포함하며, 초기화되지 않은 전역 변수를 포괄하며 이들은 공간 활용을 극대화하기 위해 메모리에서 분리되어 저장된다.(값을 가지고 있지 않기에)
`kernel.lds.S`에서 정의된 `_start_bss`, `_end_bss` 값을 이용한다.
```assembly
  /* BSS (zero-initialized data) is after everything else. */
  _start_bss = .;
  .bss : { *(.bss) }
  _end_bss = .;
```
`_start_bss`는 bss가 저장된 구간의 바로 앞에 위치, `_end_bss`는 bss 바로 뒤에 위치한다.
`memset`을 통해 `_start_bss`의 주소로부터 `_end_bss`의 주소까지의 값을 모두 0으로 초기화하여 bss에 해당하는 변수(메모리)의 값을 0으로 초기화해준다.

#### `paging_init(void)`
```c
static void
paging_init (void)
{
  uint32_t *pd, *pt;
  size_t page;
  extern char _start, _end_kernel_text;

  pd = init_page_dir = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  pt = NULL;
  for (page = 0; page < init_ram_pages; page++)
    {
      uintptr_t paddr = page * PGSIZE;
      char *vaddr = ptov (paddr);
      size_t pde_idx = pd_no (vaddr);
      size_t pte_idx = pt_no (vaddr);
      bool in_kernel_text = &_start <= vaddr && vaddr < &_end_kernel_text;

      if (pd[pde_idx] == 0)
        {
          pt = palloc_get_page (PAL_ASSERT | PAL_ZERO);
          pd[pde_idx] = pde_create (pt);
        }

      pt[pte_idx] = pte_create_kernel (vaddr, !in_kernel_text);
    }

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base Address
     of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (init_page_dir)));
}
```

>df

df
#### `read_command_line(void)`
```c
static char **
read_command_line (void) 
{
  static char *argv[LOADER_ARGS_LEN / 2 + 1];
  char *p, *end;
  int argc;
  int i;

  argc = *(uint32_t *) ptov (LOADER_ARG_CNT);
  p = ptov (LOADER_ARGS);
  end = p + LOADER_ARGS_LEN;
  for (i = 0; i < argc; i++) 
    {
      if (p >= end)
        PANIC ("command line arguments overflow");

      argv[i] = p;
      p += strnlen (p, end - p) + 1;
    }
  argv[argc] = NULL;

  /* Print kernel command line. */
  printf ("Kernel command line:");
  for (i = 0; i < argc; i++)
    if (strchr (argv[i], ' ') == NULL)
      printf (" %s", argv[i]);
    else
      printf (" '%s'", argv[i]);
  printf ("\n");

  return argv;
}
```

> command arguments를 단어 단위로 잘라 그 문자열의 주소들을 리스트를 반환하는 함수

`argc`에 physical 주소 `LOADER_ARG_CNT`에 저장되어 있는 argument 개수를 저장하고 `p`에는 argument들이 저장되어 있는 physical 주소 `LOADER_ARGS`의 가상 주소를 저장한다.
`p`에서부터 시작해 `\0`를 기준으로 문자열을 나누어 생각해 `0`로 구분된 각 문자열의 시작 주소를 `argv`(list 처럼 작동)에 차례대로 저장한다. 조회 중인 주소가 최대 수치의 주소(`end`)를 넘어서면 패닉을 일으킨다.
`argv[argc]`(끝 부분)에 `NULL`을 집어넣어 argument list의 끝을 표시한다.
그리고 커널에 집어넣은 커맨드를 출력하고 args가 단어 단위로 저장된 `argv`를 반환한다. 

#### `parse_options(char **argv)`
```c
static char **
parse_options (char **argv) 
{
  for (; *argv != NULL && **argv == '-'; argv++)
    {
      char *save_ptr;
      char *name = strtok_r (*argv, "=", &save_ptr);
      char *value = strtok_r (NULL, "", &save_ptr);
      
      if (!strcmp (name, "-h"))
        usage ();
      else if (!strcmp (name, "-q"))
        shutdown_configure (SHUTDOWN_POWER_OFF);
      else if (!strcmp (name, "-r"))
        shutdown_configure (SHUTDOWN_REBOOT);
#ifdef FILESYS
      else if (!strcmp (name, "-f"))
        format_filesys = true;
      else if (!strcmp (name, "-filesys"))
        filesys_bdev_name = value;
      else if (!strcmp (name, "-scratch"))
        scratch_bdev_name = value;
#ifdef VM
      else if (!strcmp (name, "-swap"))
        swap_bdev_name = value;
#endif
#endif
      else if (!strcmp (name, "-rs"))
        random_init (atoi (value));
      else if (!strcmp (name, "-mlfqs"))
        thread_mlfqs = true;
#ifdef USERPROG
      else if (!strcmp (name, "-ul"))
        user_page_limit = atoi (value);
#endif
      else
        PANIC ("unknown option `%s' (use -h for help)", name);
    }

  /* Initialize the random number generator based on the system
     time.  This has no effect if an "-rs" option was specified.

     When running under Bochs, this is not enough by itself to
     get a good seed value, because the pintos script sets the
     initial time to a predictable value, not to the local time,
     for reproducibility.  To fix this, give the "-r" option to
     the pintos script to request real-time execution. */
  random_init (rtc_get_time ());
  
  return argv;
}
```

> argument의 주소들이 담긴 주소를 받아 각 option에 맞는 행위를 수행한 뒤 option이 아닌 첫번째 argument를 반환하는 함수

매개변수로 입력 받은 argument의 주소들이 담긴 list를 주소 값이 null이 아니거나 '-' 값으로 시작하는 동안 순회하며 확인한다. 순회한 주소의 문자열을 확인하여 '-h', '-q', '-r' 등에 일치하는지 확인하고 그에 맞는 작업을 수행하여 kernel의 설정을 변경한다. 그리고 '-'값으로 시작하지 않는 문자열의 주소가 담긴 주소(입력 매개변수 list element 주소 중 하나)를 반환한다. 해당 주소는 kernel이 실행할 명령어를 담고 있다.

#### `run_task(char **argv)`
```c
static void
run_task (char **argv)
{
  const char *task = argv[1];
  
  printf ("Executing '%s':\n", task);
#ifdef USERPROG
  process_wait (process_execute (task));
#else
  run_test (task);
#endif
  printf ("Execution of '%s' complete.\n", task);
}
```

>매개변수로 받은 `argv`의 `argv[1]`가 가리키는 명령어를 `run_test` 또는 `process_wait`를 통해 수행하는 함수

매개변수로 받은 `char **argv`의 `argv[1]`가 가리키는 명령어를 `run_test` 또는 `process_wait`를 통해 수행한다.

#### `run_actions(char **argv)`
```c
static void
run_actions (char **argv) 
{
  /* An action. */
  struct action 
    {
      char *name;                       /* Action name. */
      int argc;                         /* # of args, including action name. */
      void (*function) (char **argv);   /* Function to execute action. */
    };

  /* Table of supported actions. */
  static const struct action actions[] = 
    {
      {"run", 2, run_task},
#ifdef FILESYS
      {"ls", 1, fsutil_ls},
      {"cat", 2, fsutil_cat},
      {"rm", 2, fsutil_rm},
      {"extract", 1, fsutil_extract},
      {"append", 2, fsutil_append},
#endif
      {NULL, 0, NULL},
    };

  while (*argv != NULL)
    {
      const struct action *a;
      int i;

      /* Find action name. */
      for (a = actions; ; a++)
        if (a->name == NULL)
          PANIC ("unknown action `%s' (use -h for help)", *argv);
        else if (!strcmp (*argv, a->name))
          break;

      /* Check for required arguments. */
      for (i = 1; i < a->argc; i++)
        if (argv[i] == NULL)
          PANIC ("action `%s' requires %d argument(s)", *argv, a->argc - 1);

      /* Invoke action and advance. */
      a->function (argv);
      argv += a->argc;
    }
  
}
```

>매개변수로 입력 받은 argv를 순회하며 argv 내 명시된 action들을 순차적으로 수행하는 함수

`action` 구조체는 수행 가능한 액션의 이름, 필요한  argument 개수, 해당 액션을 수행하기 위한 function을 저장한다.
`actions`은 수행 가능한 action에 대응대는 `action` list로 현재로서는 `run`과 예외 처리를 위한 값인 `NULL`이 있다.
매개변수로 입력받은 `argv`를 순회하며 주소가 가리키는 문자열과 `actions`의 아이템과 이름이 일치하는 것이 있는지 확인 후 있다면 해당 `action`에 명시된 argument 개수만큼 제공하였는지 확인한다. 개수가 동일하다면 함수에 해당 action에 해당되는 문자열 주소를 가리키는 argv 내 값의 주소를 해당 `action`에 해당되는 함수`action->function`의 매개변수로 두어 해당 함수를 호출해 action을 수행한다. `argv`를 순회하며 NULL 값을 만날 때까지 이를 반복한다.
만약 `actions`에 없는 action을 요구하거나 `action`에 대한 args가 부족할 때는 패닉을 일으킨다.


