# Pintos Assn2 Final Report
Team 37 
20200229 김경민, 20200423 성치호 

## 구현
### Argument Passing
디자인 레포트에서 제안한 구현 방식에서 벗어나지 않으나 디자인 레포트에서는 세부적인 사항들을 포함하지 않아 이를 바탕으로 구현하는 과정에서 디자인 레포트에서는 언급하지 않은 세부적인 사항이 많이 추가되었다. 아래 사항들이 있다.
- argument string, argument 길이 array들을 저장하기 위해 page allocation을 사용.
- `process_execute`, `start_process`에서만 구현 사항을 추가하는 기존 계획 --> 예상보다 구현의 복잡도와 높아 Argument Passing 기능을 여러 함수에 걸쳐 수행할 수 있게 함. `process_execute`, `start_process`, `parse_args`, `setup_args_stack` 에 걸쳐서 작동.
	- `parse_args`, `setup_args_stack` 함수 추가
- 기존 계획: `process_execute`에서 받은 `file_name`을 parsing하여 앞 부분을 스레드 생성 이름으로, 뒷 부분을 

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