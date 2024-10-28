
`userprog` 폴더 다 봐야함.

`process.c, .h`
- ELF(Executable and Linkable Format) 바이너리 로딩하고 process 시작함.

`pagedir.c, .h`
- **이번 플젝에서는 변경할 필요 x**
- 80x86 hardware page table의 간단한 관리자
	- 함수는 불러 사용해야함. [4.1.2.3](https://web.stanford.edu/class/cs140/projects/pintos/pintos_4.html#SEC59) 참고

`syscall.c, .h`
- 언제든 유저 프로세스가 kernel functionality 접근하고 싶으면 system call을 invoke(호출)함
- skeleton system call handler임.
- 지금은 단지 메세지 출력, user process terminate함.
- 이번 과제에 system call에 필요한 모든 거 여기에 추가하면 됨.

`exception.c, .h`
- user process가 privileged(권한이 필요한) 또는 prohibited(금지된) operation을 실행하려고 하면, 커널에 exception이나 fault로 trap됨.
- 이건 exception 핸들링함.
- 현재 구현상은 모든 exception이 단순히 메세지 출력, 프로세스 종료함.
- 과제에서는 `page_fault()`를 수정해야할 거임

`gdt.c, .h` 
- 80x86은 segmented architecture임.
- GDT(Global Descriptor Table)은 사용 중인 세그먼트를 설명하는 테이블
- 해당 파일은 GDT를 셋업함.
- **어떤 플젝에서는 변경 필요 x**
- 읽고 싶으면 읽어봐

`tss.c, .h`
- TSS(Task-State Segment)는 80x86 architectural task switching에 사용됨.
- 핀토스에서는 User Process가 interrupt handler에 들어갈 때 stack switching할 때만 사용함.
- **이번 플젝에서 변경 필요 x**
- 읽고 싶으면 읽어봐

### Process
#### `process_execute(const char *file_name)`
```c
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  return tid;
}
```
> `file_name`에서 로드한 user program을 실행하는 새로운 thread를 생성하는 함수

`palloc_get_page`를 통해 page를 할당 받고 그 페이지의 주소를 `fn_copy`에 저장한다. 이후 `fn_copy`가 가리키는 위치에 `file_name`을 복사한다.
이후 `thread_create`를 통해 `file_name`을 이름으로 가지고 `fn_copy` argument와 함께 `start_process`를 실행하는 스레드를 생성한다. 성공적으로 생성했다면 이 스레드의 thread id를 반환한다,

이때 주의할 점은 `process_execute()` 반환되기 전에 이로 인해 생성된 스레드가 종료되거나 스케줄링될 수 있다는 것이다.

#### `start_process(void *file_name_)`
```c
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}
```
> `file_name_`로부터 ELF executable을 로드하고 이를 실행하는 함수

`process_execute`에 의해 생성되는 스레드가 실행할 함수이다.
//TODO: intr_frame
`memset`을 통해 `intr_frame`인 `if_`를 0으로 초기화한 뒤 `if_`의 `gs`,`cs`,`eflags` 값을 초기화해준다. 
이후 `load`를 통해 `file_name`으로부터 ELF를 로드하고 `if_.eip`에 entry point를, `if_.esp`에 초기 stack pointer를 저장한다. 만약 이를 실패시 `thread_exit()`를 통해 현재 스레드를 삭제한다.
또한 `file_name_`를 넘기느라 할당했던 page를 free해준다.
```c
asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
```
위 코드를 통해 `intr_exit`을 통해 interrupt에서 돌아오는 것처럼 user process를 실행한다.
- `intr_exit`은 스택에서 `intr_frame` 형태로 값을 가져온다. 즉 위에서 설정한 `if_`을 사용할 것이다.
	- `if_`의 return address, 즉 eip가 user process임.
- `esp`가 위에서 설정한 `if_`를 가르키게 한 뒤 `intr_exit`으로 점프해 `intr_exit`이 실행되도록 한다.

#### `process_wait(tid_t child_tid)`
```c
int
process_wait (tid_t child_tid UNUSED) 
{
  return -1;
}
```
우리가 구현해야 할 함수로 현재는 항상 -1을 리턴한다.
이상적인 작동은 `child_tid`의 스레드가 종료될 때까지 기다리다가 그 스레드의 exit status를 반환하고 kernel에 의해 종료되었거나 잘못된 요청일 경우 -1을 리턴하는 것이다. 

#### `process_exit(void)`
```c
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}
```
> 현재 실행 중인 process에게 할당된 자원을 모두 해제하는 함수.

해당 함수 내 실행 순서는 매우 중요하다. 먼저 현재 실행 중인 스레드의 `pagedir`을 `null`로 변경하여 해당 page directory로 다시 switch되지 않도록 해야한다. 또한 
//TODO: 위에가 뭔소리야. 아래 이유도 추가하면 좋을 듯.
`pagedir_activate(NULL)`을 통해 base page directory를 활성화한 뒤 현재 실행되는 있는 프로세스(스레드)의 `pagedir` page directory를 `pagedir_destory`를 통해 삭제하고 그것이 참조하는 모든 페이지를 모두 free한다.
- 이 때 삭제 전 반드시 base page directory를 먼저 활성화 해야 한다.

#### `process_activate(void)`
```c
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}
```
> context switch마다 실행되어 현재 스레드에서 user code가 실행될 수 있도록 cpu를 설정하는 함수

`pagedir_activate`를 통해 현재 스레드의 `pagedir`, page table를 활성화한다.
`tss_update`를 통해 
//TODO: tss 이해 후 설명 추가


#### `load(const char *file_name, void (**eip) (void), void **esp)`
```c

```
> 주어진 `file_name`으로부터 ELF executable을 현재 스레드에 로드하는 user program에서 가장 핵심적인 함수이다. 

`start_process`에서 호출해 사용하며 주어진 `file_name`으로부터 ELF executable을 현재 스레드에 로드하고 executable의 entry point를 EIP에, 초기 stack pointer를 esp에 저장하는 함수이다. 이 때 성공시 true를, 실패시 false를 반환한다.

`pagedir_create()`를 통해 현재 스레드에 page directory를 할당하고 이를 스레드의 `pagedir`에 저장한다. 성공시 `process_activate()`를 통해 해당 page directory를 활성화하고 /TODO(tss)한다. 

#### `validate_segment(const struct Elf32_Phdr *phdr, struct file *file)`
```c
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}
```
> 주어진 `Elf32_Phdr`인 `phdr` 즉 ELF의 program header가 `file` 내에서 유효한지, load할 수 있는 segment에 대한 것인지 체크하고 그 결과를 반환하는 함수

아래 중 하나라도 만족하지 못하면 false 반환, 모두 만족하면 true 반환환
체크 항목 
- `phdr`의 `p_offset`과 `p_vaddr`이 같은 페이지 offset을 가지는 확인
- `phdr`의 `p_offset`이 `file`의 메모리 범위 내에 위치하는지 확인
- `phdr`의 `p_memsz`가 `p_filesz`보다 크거나 같은지 확인
	- 메모리 상 크기가 파일 내 크기보다 큰가?
- `phdr`의 `p_memsz`가 아닌지 확인 ==> segment가 비어있지 않은지 확인
- segment의 시작, 끝 위치가 모두 user address space 범위 내인지 확인
	- `p_vaddr`(처음),`p_vaddr+p_memsz`(끝) 모두 `is_user_vaddr()`이 참이여야 함.
- `p_vaddr`+`p_memsz` >= `p_vaddr`이여야 함.
- segment에 할당된 페이지0이 아닌지 확인
	- `p_vaddr`>=`PGSIZE`

#### `load_segment
```c
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}
```
> df

#### `setup_stack(void **esp)`
```c
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}
```
> dfdf


#### `install_page(void *upage, void *kpage, bool writable)`
```c
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
```
> user virtual address -> kernel virtual address 매핑을 page table에 추가하는 함수

주어진 user virtual address인 `upage`->  kernel virtual address인 `kpage` 매핑을 page table에 추가하는 함수로 입력받은 `writable` 값에 따라 read-only, writeable을 결정해 page table 추가에 함께 적용한다.

이를 위해 현재 스레드의 `pagedir`의

### ELF
ELF specification에 명시된 것을 동일하게 구현한 것이다.

```c
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;
```
ELF 내에서 헤더를 표현할 때 사용되는 data type이다.

```c
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */
```
//TODO: 먼데 이게
```c
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };
```
ELF binary의 매우 앞 부분에 등장하는 정보에 대한 struct로 Exectable에 대한 정보를 담는 **Executable header**를 표현하는 struct이다.

```c
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };
```
ELF binary 내 정보에 대한 struct로 **Program header**를 표현하는 struct이다.
바이너리 내 `e_phoff`에서 시작하여 `e_phnum`개의 entries를 가지고 있다.

```c
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */
```
`p_type`은 세그먼트의 타입을 표현하는 값으로 위 값들은 `p_type`에 사용되는 값들의 목록이다.

```c
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */
```
`p_flags`에 사용되는 값들의 목록으로 각 segment에 의존적인 값들이다.

