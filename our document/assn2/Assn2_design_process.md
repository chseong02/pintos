### ELF
ELF specification에 명시된 것을 동일하게 구현한 것이다.

```c
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;
```
ELF 내에서 헤더를 표현할 때 사용되는 data type이다.

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
//TODO: 위에가 뭔소리야.
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
`tss_update`를 통해 현재 스레드 스택 끝을 tss의 stack pointer가 가르키게 한다.  이 이유는 interrupt 발생시 어떤 프로세스에 의해 발생하였는지(user에 의해서인지, kernel에 의해서인지, `exception.c`의 `kill` 참고 ) 알기 위함이다.


#### `load(const char *file_name, void (**eip) (void), void **esp)`
> 주어진 `file_name`으로부터 ELF executable을 현재 스레드에 로드하는 user program에서 가장 핵심적인 함수이다. 

`start_process`에서 호출해 사용하며 주어진 `file_name`으로부터 ELF executable을 현재 스레드에 로드하고 executable의 entry point를 EIP에, 초기 stack pointer를 esp에 저장하는 함수이다. 이 때 성공시 true를, 실패시 false를 반환한다.
```c
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

```
`pagedir_create()`를 통해 현재 스레드에 page directory를 할당하고 이를 스레드의 `pagedir`에 저장한다. 성공시 `process_activate()`를 통해 해당 page directory를 활성화하고 tss를 업데이트해 해당 스레드의 스택 끝을 esp에 저장한다.

```c
  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }
```
루트 디렉토리에 있는 파일 중 이름이 `file_name`인 파일을 찾아 연다. `file_read`를 통해 `file`을 Executable header의 크기만큼 읽어 들여 `ehdr`에 저장한 뒤 올바른 Executable header를 가지고 있는지 검증한다. Executable header은 ELF binary의 매우 앞 부분에 등장하는 헤더로 executable에 대한 정보를 담고 있다.

```c
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }
```
`file_ofs`에 `ehdr.e_phoff`으로부터 얻은 program header의 위치(offset)을 저장한다.
`file_ofs`부터 `file_read`을 통해 program header 크기만큼 `ehdr.e_phnum`(program header 개수)번 읽어들이며 매 헤더에 대해 아래를 수행한다.
- 읽어들인 program header는 `phdr`에 저장된다. `phdr.p_type`, segment 타입을 보고 `PT_LOAD`인 경우 (loadable segment인 경우) 다음을 수행한다.
	- 해당 `phdr` 프로그램 헤더가 올바른 segment를 나타내는지 `validate_segment`를 통해 검증한다. 
	- `phdr`로부터 해당 세그먼트에 대한 각종 정보(writable, 파일 내 위치 등)를 뽑아내고`phdr.p_filesz`가 0 초과라면(파일 내 세그먼트가 차지하는 크기가 0 초과) 일반적인 segment이고 그렇지 않으면 zero로 이루어진 segment일 뿐이다.
	- 일반적인 세그먼트라면 `load_segment`를 통해 세그먼트를 로드한다.

```c
  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}
```
`setup_stack`을 통해 stack을 초기화하고 매개변수의 `eip`에 executable 시작 위치를 저장한다.
모두 완료되면 `file_close`를 통해 파일을 닫고 성공 여부를 반환한다.

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

#### `load_segment`
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
> `file` 내의 `ofs`으로부터 시작하는 segment를 `upage`에 로드하고 매핑을 page table에 추가하는 함수

`file_seek`을 통해 파일 `file`이 가리키는 오프셋을 `ofs`로 변경한다.  
우선 read해 load 또는 0으로 채우는 것은 한 페이지 단위로 진행한다. `palloc_get_page`를 통해 user page를 할당 받고 `file_read`를 통해 읽어와 할당 받은 page에 저장하고 해당 페이지 끝에서부터 `page_zero_byte`(PGSIZE - 이번에 read한 바이트)만큼 0으로 초기화 한다. `install_page`를 통해 `upage`와 `kpage` 매핑을  page table에 추가한다. 이 때 writable 여부는 입력 받은 인수에 따른다. 이를 `read_bytes`만큼 모두 로드할 때까지 페이지 단위로 반복한다.
성공하면 true를 반환하고 중간에 실패했으면 false를 반환한다.
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
> user virtual memory 최상단에 0으로 초기화된 페이지를 매핑하여 stack을 초기화하는 함수

`load`에서 ELF 로드 완료 이후 stack을 초기화할 때 사용한다.
`palloc_get_page`를 통해 0으로 초기화된 user page를 할당하고 `install_page`를 통해 해당 페이지-kernel virtual address 매핑을 page table에 추가한다. 

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

주어진 user virtual address인 `upage`->  kernel virtual address인 `kpage` 매핑을 page table에 추가하는 함수로 입력받은 `writable` 값에 따라 read-only, writeable을 결정해 page table에 추가에 함께 적용한다.
