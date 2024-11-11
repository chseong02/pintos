# Pintos Assn2 Final Report
Team 37 
20200229 김경민, 20200423 성치호 

## 구현
### Argument Passing
디자인 레포트에서 제안한 구현 방식에서 벗어나지 않으나 디자인 레포트에서는 세부적인 사항들을 포함하지 않아 이를 바탕으로 구현하는 과정에서 디자인 레포트에서는 언급하지 않은 세부적인 사항이 많이 추가되었다. 아래 사항들이 있다.
- argument string, argument 길이 array들을 저장하기 위해 page allocation을 사용.
- `process_execute`, `start_process`에서만 구현 사항을 추가하는 기존 계획 --> 예상보다 구현의 복잡도와 높아 Argument Passing 기능을 여러 함수에 걸쳐 수행할 수 있게 함. `process_execute`, `start_process`, `parse_args`, `setup_args_stack` 에 걸쳐서 작동.
	- `parse_args`, `setup_args_stack` 함수 추가
- 기존 계획: `process_execute`에서 받은 `file_name`을 parsing하여 앞 부분을 스레드 생성 이름으로, 뒷 부분을 실행 함수의 argument로 `thread_create`에 넘겨주기 
  -->  `process_execute`에서 받은 `file_name`을 두 개로 복사한 뒤 하나를 파싱하여 앞부분을 스레드 생성 이름으로 사용하고 다른 복사본을 argument로 통째로 넘겨준다.
	- 기존 계획에서는 `load`에서 파일 이름을 사용하는 것을 고려하지 못하여서 계획을 수정함.

#### `parse_args` in `userprog/process`
```c
static void
parse_args (char *cmd_line_str, char **argv, size_t *argv_len, uint32_t *argc)
{
  char *arg, *save_ptr;

  for (arg = strtok_r (cmd_line_str, " ", &save_ptr); arg != NULL; 
    arg = strtok_r (NULL, " ", &save_ptr))
  {
    argv_len[*argc] = strlen (arg) + 1;
    argv[*argc] = arg;
    (*argc)++;
  }
}
```
> `cmd_line_str`을 Space를 기준으로 파싱하여 각 문자열의 시작 주소를 `argv` 배열에 집어넣고 각 문자열의 길이를 `argv_len` 배열에, 문자열의 개수를 `argc`에 저장해 전달하는 함수.

`cmd_line_str`을 Space를 기준으로 끊어가면서 끝에 도달할 때까지 순회하며 다음을 수행한다.
- `argv_len[*argc]`에 현재 끊긴 문자열의 길이(`strlen`으로 측정) + 1을 추가한다.
	- 각 문자열 끝의  `NULL`을 고려한 수치이다.
- `argv[*argc]`에는 현재 끊긴 문자열의 시작 주소를 집어 넣는다.
- `*argc`를 1 증가시킨다.
	- `argc`는 순회 중에는 배열의 index로서 사용되며 순회 완료시에는 각 배열의 길이, 문자열의 총 개수를 의미하게 된다.

인수로 받는 `argv`, `argv_len` 등은 올바르게 공간(페이지)가 할당되어 사용할 수 있음을 기대한다.
해당 함수는 인수의 `cmd_line_str`은 변형하며 맨 앞 단어만을 담은 문자열처럼 작동하게 된다.
- 실행할 파일 이름만을 의미하게 된다. 
	- 단어 끝 `\0`에 의해 뒤 문자들은 읽지 않고 멈춤

#### `setup_args_stack` in `userprog/process`
> Argument Passing의 핵심 함수로 80x86 Calling Convention에 맞추어 주어진 argument 정보들을 이용해 stack에 argument 정보를 쌓는 함수

##### 80x86 Calling Convention 예시
`/bin/ls -l foo bar` 명령일 시 해당 함수 리턴시 stack이 다음 꼴을 나타내도록 해야 함.
```c
   Example cmd line: "/bin/ls -l foo bar" 
   Address 	    Name 	            Data 	        Type
   0xbffffffc 	argv[3][...] 	    "bar\0" 	    char[4]
   0xbffffff8 	argv[2][...] 	    "foo\0" 	    char[4]
   0xbffffff5 	argv[1][...] 	    "-l\0" 	        char[3]
   0xbfffffed 	argv[0][...] 	    "/bin/ls\0" 	char[8]
   0xbfffffec 	word-align 	        0 	            uint8_t
   0xbfffffe8 	argv[4] 	        0 	            char *
   0xbfffffe4 	argv[3] 	        0xbffffffc 	    char *
   0xbfffffe0 	argv[2] 	        0xbffffff8 	    char *
   0xbfffffdc 	argv[1] 	        0xbffffff5 	    char *
   0xbfffffd8 	argv[0] 	        0xbfffffed 	    char *
   0xbfffffd4 	argv 	            0xbfffffd8 	    char **
   0xbfffffd0 	argc 	            4 	            int
   0xbfffffcc 	return address 	    0 	            void (*) ()   <- esp
```


```c
static void
setup_args_stack (char **argv, size_t *argv_len, uint32_t argc, 
  void **esp)
{
```
함수 입력 매개변수
`argv`: file name을 포함한 argument string들의 시작 주소를 담은 배열
`argv_len`: file name을 포함한 argument string들의 길이를 담은 배열
`argc`: parsing 후 file name을 포함한 argument string의 개수
`esp`: argument 정보들을 쌓을 스택 point의 주소를 가리키는 포인터

```c
  /* argv string */
  char *ptr_argv = (char*) *esp;
  for (int i = argc - 1; i >= 0; i--)
  {
    ptr_argv = ptr_argv - (char*) (argv_len[i]);
    strlcpy (ptr_argv, (const char*) argv[i], (size_t) (argv_len[i]));
  }

```
`ptr_argv`는 실시간 stack point로 `*esp`로 설정해 둔다. `char *`자료형이므로 1바이트씩 증가한다.
`argv`와 `argv_len`를 뒤에서부터 앞으로 순회하며 다음을 수행한다. (두 배열의 길이는 `argc`로 같다)
- `ptr_argv`에서 `argv_len[i]` 만큼 빼주어 `argv[i]` 문자열이 들어갈 수 있는 공간을 확보한다. 
- [`ptr_argv`,`ptr_argv + argv_len[i]`) 에 `argv[i]` 문자열을 deep copy해준다.
이로써 right->left로 argument 문자열을 스택에 모두 집어넣었다. (Example 기준 `0xbffffffc`~`0xbfffffed`)

```c
  /* Word Align */
  ptr_argv = (char *)(((uint32_t) ptr_argv) - ((uint32_t) ptr_argv) % 4);
```
`char`는 한 바이트이기에 위에서 집어넣은 Argument 문자열의 길이는 모두 제각각이다. 주소는 보통 4바이트 기준으로 읽어들이고 쓰므로 이를 위해 4바이트 단위로 나누어 떨어지도록 패딩을 추가해주어야 한다.

이를 위해 `ptr_argv` 스택 포인트를 4로 나눈 나머지를 빼주어 4로 나누어 떨어지도록 해준다. (Example 기준 `0xbfffffec`)

```c
  ptr_argv -= 4;
  char** ptr_argv_addr = (char **) ptr_argv;
  
  *((uint32_t *) ptr_argv_addr) = 0;
  ptr_argv_addr--;

  /* argv string pointer */
  char* argv_addr_iter_ptr = (char*) *esp; 
  for(int i = argc-1; i >= 0; i--)
  {
    argv_addr_iter_ptr -= argv_len[i];
    *ptr_argv_addr = (char *) argv_addr_iter_ptr;
    ptr_argv_addr--;
  }
```
다음 4바이트를 0으로 비워준다. 이는 마지막 argument 다음에 대한 주소이다. 즉 argument 주소들의 끝을 나타내기 위해 존재하는 공백이라 생각하면 된다. (Example 기준 `0xbfffffe8`)
`char*`자료형이던 `ptr_argv`를 `char **`로 형변환하여 `ptr_argv_addr`에 저장한다.
- `ptr_argv_addr`은 주소를 가리키는 포인터이기에 4바이트 단위로 증감한다.
`argv_len`을 역순으로 순회하며 기존 스택 포인트에서 문자열 길이만큼 빼가며 각 argument 문자열의 시작 주소를 얻어 스택에 right -> left 순서로 집어 넣는다. (Example 기준 `0xbfffffe4` ~ `0xbfffffd8`)

```c
  /* argv address */
  *ptr_argv_addr = (char**) (ptr_argv_addr + 1);
  ptr_argv_addr--;
```
argument 문자열 주소들이 스택 어디부터 시작하는지를 스택에 집어 넣는다. 즉 현재 스택 point의 바로 이전 주소를 집어넣으면 된다. (Example 기준 `0xbfffffd4`)

```c
  /* argument count */
  *(uint32_t*) ptr_argv_addr = argc;
  ptr_argv_addr--;
```
스택에 `argc`, 즉 argument들의 개수를 집어넣는다. (Example 기준 `0xbfffffd0`)

```c
  *ptr_argv_addr = 0;
  
  void* ori_if_esp = *esp;
  *esp = (void*) ptr_argv_addr;
```
스택에 마지막으로 0으로 집어 넣고 현재 스택 포인터 주소를 담고 있는 `ptr_argv_addr`가 담고 있는 주소를 `esp`에 넣는다. 
즉 위에서 쌓은 스택의 stack point가 인수의 `esp`가 가리키는 값이 된다. 

#### `start_process` in `userprog/process` 변경점
```c
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread *t;

  /* argv: process arguments str point array */
  /* argv_len: process arguments str length array */
  /* argc: process arguments count */
  char **argv;
  size_t *argv_len;
  uint32_t argc = 0;
  success = true;
  t = thread_current();

  ...

  argv = palloc_get_page (0);
  if (argv == NULL)
    success = false;

  argv_len = palloc_get_page (0);
  if (argv_len == NULL)
    success = false;
  
  /* file_name stops at the null character only at the end of the pure file name */
  parse_args (file_name, argv, argv_len, &argc);
  success = load (file_name, &if_.eip, &if_.esp) && success;
  if (success)
    setup_args_stack (argv, argv_len, argc, &if_.esp);
  else
    t->process_ptr->pid = PID_ERROR;
  
  palloc_free_page (file_name);
  palloc_free_page (argv);
  palloc_free_page (argv_len);
  
  ...
  
  if (!success) 
    thread_exit ();

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}
```
`start_process`는 유저 프로그램 프로세스 실행을 위해 `process_execute`에서 스레드 생성시 넘겨주는 함수로 스레드가 실행하는 함수이다. 실행해야 할 명령어 문자열 전체를 매개변수 `file_name_`로 받는다.
argument 문자열들의 시작 주소를 담을 `argv`, 그 문자열들의 각 길이를 담을 `argv_len`를 선언하고 해당 배열들의 공간을 위해 `palloc_get_page`를 통해 각각 페이지를 할당해준다.
만약 할당 실패시 `success`를 `false`로 변경한다.
`parse_args`를 통해 `file_name`을 파싱하여 `argv`, `argv_len`, `argc`에 각각 argument 문자열 시작 주소들, 각 argument 길이, argument 개수를 집어 넣는다.
- `parse_args`에 의해 `file_name`은 파일 명만을 담은 문자열처럼 작동하게 된다. 
이후 `file_name`을 로드 이후 성공 여부를 `success`에 저장한다.
만약 그동안 모든 작업을 성공했다면(`success`가 true) `setup_args_stack`을 통해 `if_.esp`를 stack point로 생각하여 80x86 Calling Convention에 맞추어 argument 정보를 스택에 쌓는다. 
만약 그동안 한 번이라도 실패한 작업이 있다면(`success`가 false) 해당 프로세스의 `pid`를 `PID_ERROR`로 설정한다.
- 해당 내용은 뒤의 SystemCall Handler에서 자세히 설명하겠다.

로드 작업, 프로그램 실행시 사용할 스택 설정이 완료되었으므로 `file_name`, `argv`, `argv_len`을 모두 할당 해제해준다.

#### `process_execute` in `userprog/process` 변경점
`process_execute` 내 변경 사항 중 Argument Passing 구현을 위해 변경한 내용만을 포함하여 설명하겠다.
```c
tid_t
process_execute (const char *file_name) 
{
  /* Command: File Name + Arguments */
  char *full_cmd_line_copy;
  /* Just File Name */
  char *file_name_copy;
  char *save_ptr;
  
  ...

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  full_cmd_line_copy = palloc_get_page (0);
  if (full_cmd_line_copy == NULL)
    return TID_ERROR;
  file_name_copy = palloc_get_page (0);
  if (file_name_copy == NULL)
    return TID_ERROR;

  strlcpy (full_cmd_line_copy, file_name, PGSIZE);
  strlcpy (file_name_copy, file_name, PGSIZE);

  strtok_r (file_name_copy, " ", &save_ptr);
  
  ...
```
`full_cmd_line_copy`은 입력 받은 `file_name`를 통째로 복사해 저장할 예정이며, `file_name_copy`는 `file_name` 중 파일 이름만을(파싱 후 맨 앞 단어) 저장할 예정이다.

 문자열을 저장할 수 있게 `palloc_get_page`를 이용해 `full_cmd_line_copy`, `file_name_copy`에 각각 페이지를 할당한다.

만약 페이지 할당 실패시 `TID_ERROR`를 리턴해 프로세스 실행에 실패했음을 나타낸다.

이후 `strlcpy`를 이용해 `full_cmd_line_copy`, `file_name_copy`에 각각 `file_name`을 deep copy한다.

이후 `strtok_r`을 이용해 `file_name_copy`를 첫 공백을 기준으로 나눈다. 이 때 공백은 `\0`로 대체되어 `file_name_copy`는 맨 앞 단어만으로 이루어진 문자열처럼 작동하게 된다.
즉 맨 앞 단어인 파일명이다.

```c
  ...
    
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create_with_pcb (file_name_copy, PRI_DEFAULT, p, start_process, 
    full_cmd_line_copy);
  palloc_free_page (file_name_copy);
  if (tid == TID_ERROR)
  {
    palloc_free_page (full_cmd_line_copy);
    palloc_free_page (p);
    return TID_ERROR;
  }
  
  ...
  
  return tid;
}
```
`thread_create_with_pcb`는 `thread_create`의 확장된 형태로 뒤에서 자세히 후술하겠다.
`thread_create_with_pcb`에 스레드 이름으로 `file_name_copy`를 넘겨주어 실행하는 파일 명으로 스레드 명을 가지게 한다. 또한 `start_process`의 인수로써 `full_cmd_line_copy` 즉 `file_name` 통 복사본을 넘겨준다.

`thread_create_with_pcb`가 완료되면 스레드 생성은 완료되었으므로 스레드 이름으로 사용된 `file_name_copy`의 페이지를 할당 해제한다.

만약 `thread_crate_with_pcb`가 실패하였다면 `start_process`의 인수인 `full_cmd_line_copy` 또한 사용될 일이 없으므로 할당 해제해준다.

위의 작업들을 통해 Argument Passing을 구현하여 올바르게 파일을 오픈하여 유저 프로그램을 수행하고 유저 프로그램 실행시 명령어의 argument들을 올바르게 넘겨줄 수 있게 되었다.

### System Call 기반

#### Virtual Memory User Space
System call 함수들을 구현할 때 가상 메모리 위의 User Space에 있는 주소에만 읽고 쓰는 것이 가능하도록 핸들링을 해줘야 했다. Pintos 문서에는 이를 구현하기 위한 두 가지 방법이 제시되어 있는데, 이번 프로젝트에선 해당 위치가 User space인지 Kernel space인지만 검사한 뒤 User space 내에서의 잘못된 참조는 Page fault handler가 관리하도록 구현하였다.

```c
/* Only checks whether its in the user space */
bool
check_ptr_in_user_space (const void *ptr)
{
  return ptr < PHYS_BASE;
}
```

`exception.c`의 Page fault handler에서도 기존과 다른 동작을 하도록 수정해줘야 했다. 유저가 page fault를 일으켰을 경우 `exit(-1)`로 프로세스를 종료하고, 커널이 일으켰을 경우 `-1`을 반환한다.

```c
static void
page_fault (struct intr_frame *f) 
{
  ...
  /* Kernel caused page fault by accessing user memory */
  if(!user && check_ptr_in_user_space(fault_addr))
  {
   f->eip = (void *)f->eax;
   f->eax = -1;
   return;
  }
  /* User caused page fault */
  else sys_exit(-1);
  ...
}
```

해당 기능을 편리하게 구현하기 위해 User space에서 한 바이트씩 읽거나 쓰는 함수를 Pintos 문서에서 제시해주었다.

```c
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
```
다만 위 함수들로는 한 바이트씩밖에 읽고 쓸 수 없으므로 위의 함수들을 이용하여 임의 길이의 데이터를 읽을 수 있는 함수를 구현했다.

```c
/* Reads NUM bytes at user address SRC, stores at DEST.
   Note that DEST is not a vmem address.
   Returns true if every byte copies are successful. */
static bool
get_user_bytes (void *dest, const void *src, size_t num)
{
  uint8_t *_dest = dest;
  const uint8_t *_src = src;
  for (size_t i = 0; i < num; i++)
  {
    if (!check_ptr_in_user_space (_src)) return false;
    int res = get_user (_src);
    if (res == -1) return false;
    *_dest = (uint8_t) res;
    _dest++;
    _src++;
  }
  return true;
}
```

임의 길이 write 함수는 이번 프로젝트에서 사용처가 없어 구현하지 않았다.

#### System Call Arguments

System call이 호출되었을 때 추가로 전달되는 인자들은 `Stack pointer + 4` 위치부터 4바이트 단위로 순서대로 배치되어있다. 이를 가져오기 위해 원하는 만큼의 인자를 읽어 가져오는 `get_args` 함수를 구현했다.

```c
/* Parse NUM arguments from sp + 4 to dest. */
static void
get_args (int *sp, int *dest, size_t num)
{
  for(size_t i = 0; i < num; i++)
  {
    int *src = sp + i + 1;
    if (!check_ptr_in_user_space (src)) sys_exit (-1);
    if (!get_user_bytes (dest + i, src, 4)) sys_exit (-1);
  }
}
```

전달되는 인자의 수는 호출되는 System call마다 다르므로, System call 핸들러가 호출되고 어떤 함수가 호출되었는지 알아낸 뒤 위의 함수를 통해 인자를 읽어와 실행해준다.

```c
static void
syscall_handler (struct intr_frame *f) 
{
  int arg[4];
  if (!check_ptr_in_user_space (f->esp))
    sys_exit (-1);
  switch(*(uint32_t *) (f->esp))
  {
    ...
    case SYS_FILESIZE:
      get_args (f->esp, arg, 1);
      f->eax = sys_filesize (arg[0]);
      break;
    case SYS_READ:
      get_args (f->esp, arg, 3);
      f->eax = sys_read (arg[0], (void *) arg[1], (unsigned) arg[2]);
      break;
    case SYS_WRITE:
      get_args (f->esp, arg, 3);
      f->eax = sys_write(arg[0], (const void *) arg[1], (unsigned) arg[2]);
      break;
    case SYS_SEEK:
	...
```

### System Call - PCB

### System Call - File Manipulation


## 발전한 점
## 한계
## 배운 점