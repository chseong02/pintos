# Pintos Assn3 Design Report
Team 37 
20200229 김경민, 20200423 성치호 

Basics : the definition or concept, implementations in original pintos (if exists)
• Limitations and Necessity : the problem of original pintos, the benefit of implementation
• Blueprint: how to implement it (detailed data structures and pseudo codes

## 0. Background
TODO: 아래에 넣기 애매한 내용들/기반이 될 내용들 있다면 추가하기.

## 정리
`paging_init` in `init.c`
- page directory
`pagedir_set_page` in `pagedir.c`
- page directory PD에 user virtual page UPAGE -> pysical frame(커널 가상 주소 KPAGE로 식별되는) 매핑 추가
- 이 때 kpage는 `palloc_get_page`로 **사용자풀**에서 얻은 페이지
`pagedir_create` in `pagedir.c`
- 새로운 page directory 생성
	- kernel virtual address에 mapping을 가진,(user xxx)
스레드별로 독립적인 page dir

## 1. Frame Table
### Basics
Pintos는 Virtual Memory를 효율적으로 관리/구현하기 위해 Page와 이를 관리하기 위한 Page Directory, Page Table 등을 구현해두었다.
#### Page
pintos에서 주로 메모리를 관리할 때 사용하는 단위이다. Virtual Memory에서 Page를 논하게 되며 **Virtual Memory 4KB**이다. Virtual Memory는 Page 단위로 나누어져 할당 받거나 해제하는 등 관리된다. Virtual Memory 상 4KB를 페이지 하나로 생각하기 때문에 Virtual Address는 다음과 같이 해석된다.
```c
   31               12 11        0
  +-------------------+-----------+
  |    Page Number    |   Offset  |
  +-------------------+-----------+
		   Virtual Address
```
virtual address의 앞의 31~12비트(총 20비트)는 Page Number로, 뒤의 나머지 12비트는 offset으로 취급한다.  왜냐하면 Page는 Page-Aligned되어 있으며 Page가 4KB = $2^{12}$ Byte이기 때문이기에 주소의 하위 12비트가 표현하는 주소들은 모두 같은 페이지 내에 있기 때문이다. 또한 page number는 총 20비트로 1024 * 1024개의 page를 표현할 수 있으며 이는 각각 page directory, page table로 구성된다. 이에 대해 자세히 후술하겠다.
#### Page Directory, Page Table, Page Table Entry
Pintos에서 Virtual Memory는 Page Directory, Page Table, Page Table Entry를 통해 구현된다.
```c
struct thread
  {
    ...
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif
	...
  };
```
모든 프로세스(스레드)는 각자의 Page Directory를 가지고 있으며 독립적으로 관리하게 된다. 위 `pagedir`은 Page Directory로 사용되게 할당 받은 페이지의 시작 주소(kernel virtual Address임)로, Page Directory 시작 위치이다. 추후 이 pagedir은 `pagedir_activate` in `userprog/pagedir.c`의 `asm volatile ("movl %0, %%cr3" : : "r" (vtop (pd)) : "memory");`를 통해 활성화되어 virtual address -> physical address의 변환 및 매핑을 설정하게 된다.
Pintos의 Virtual Memory는 아래 구조처럼 구성되며 Virtual Address는 32bit로 표현되며 다음 구조를 가지고 있다. 
```c
 31                  22 21                  12 11                   0
+----------------------+----------------------+----------------------+
| Page Directory Index |   Page Table Index   |    Page Offset       |
+----------------------+----------------------+----------------------+
             |                    |                     |
     _______/             _______/                _____/
    /                    /                       /
   /    Page Directory  /      Page Table       /    Data Page
  /     .____________. /     .____________.    /   .____________.
  |1,023|____________| |1,023|____________|    |   |____________|
  |1,022|____________| |1,022|____________|    |   |____________|
  |1,021|____________| |1,021|____________|    \__\|____________|
  |1,020|____________| |1,020|____________|       /|____________|
  |     |            | |     |            |        |            |
  |     |            | \____\|            |_       |            |
  |     |      .     |      /|      .     | \      |      .     |
  \____\|      .     |_      |      .     |  |     |      .     |
       /|      .     | \     |      .     |  |     |      .     |
        |      .     |  |    |      .     |  |     |      .     |
        |            |  |    |            |  |     |            |
        |____________|  |    |____________|  |     |____________|
       4|____________|  |   4|____________|  |     |____________|
       3|____________|  |   3|____________|  |     |____________|
       2|____________|  |   2|____________|  |     |____________|
       1|____________|  |   1|____________|  |     |____________|
       0|____________|  \__\0|____________|  \____\|____________|
                           /                      /
```
pintos는 각 스레드마다 각기 다른 page directory를 가지고 있고 독립적으로 관리한다. 이런 page direcotry는 1024개의 page directory entry를 가지며 각 entry는 32bit로 이루어진다.
```c
 31                                   12 11                2 1 0
+---------------------------------------+----+----+-+-+---+-+-+-+
|           Physical Address            |                 |U|W|P|
+---------------------------------------+----+----+-+-+---+-+-+-+
```
엔트리의 앞선 31~12bit는 각각 다른 page table의 시작 physical address의 31~12bit 부분을 담고 있다. 이 때 뒤의 11bit를 포함하지 않아도 되는 이유는 page table의 시작 위치가 4KB 정렬될 것이 보장되기 때문이다. 이는 추후 나올 Page Table Entry에서도 동일하다. 하위 11~0 bit에는 page directory entry에 대한 flag들이 포함된다. 
```c
static inline uint32_t pde_create (uint32_t *pt) {
  ASSERT (pg_ofs (pt) == 0);
  return vtop (pt) | PTE_U | PTE_P | PTE_W;
}
```

| Flag    | 없을 때                        | 있을 때                  |
| ------- | --------------------------- | --------------------- |
| `PTE_U` | kernel만 접근 가능               | kernel, user 모두 접근 가능 |
| `PTE_P` | PTE 존재X, 다른 flag 모두 의미 없어짐. | PTE 존재O, 유효           |
| `PTE_W` | read-only                   | read/write 둘 다 가능     |

```c
 31                                   12 11 9      6 5     2 1 0
+---------------------------------------+----+----+-+-+---+-+-+-+
|           Physical Address            | AVL|    |D|A|   |U|W|P|
+---------------------------------------+----+----+-+-+---+-+-+-+
```
0~11 bit
먼저 각 스레드의 `pagedir`에 해당하는 주소를 본다. 이는 page directory 테이블의 시작점이다. 해당 테이블 시작점부터 virtual address의 31~22bit 값, `page directory index`에 해당하는 위치의 값을 읽어 들인다. 해당 위치에는 한 page table의 시작점을 담고 있다. 
#### Frame
pintos에서 **Physical Memory**를 관리할 때 사용하는 단위로 연속된 공간의 Physical Memory로, page와 동일하게 **4KB**이다. pintos에서 page는 관리하기 위해 page directory, page table 등 을 구현하고, 함수들의 반환 값으로 사용하는 등 빈번하게 사용되는 반면, frame은 `pagedir_set_page`와 `install_page` 등에서 간접적으로 언급되는 것을 제외하고는 다른 곳에서는 거의 사용되거나 언급되지 않는다. 
```c
   31               12 11        0
  +-------------------+-----------+
  |    Frame Number   |   Offset  |
  +-------------------+-----------+
			   Physical Address
```
Physical Address는 앞의 20비트(31~12)는 Frame Number를, 나머지 뒤의 12비트(11 ~ 0)은 Frame 내 offset을 의미한다. 이는 virtual address & page와 유사하게 physical memory 상에서 frame이 frame-aligned 되어있으며 frame이 4KB이기 때문이다.

80x86 프로세서는 단순히 Physical Address를 통해 메모리에 접근하는 방법을 제공하지 않는다. Pintos에서는 이를 kernel virtual memory와 physical memory를 direct mapping하여 간접적으로 방법을 제공한다. 즉 kernel virtual memory의 첫 page는 physical memory의 첫 frame과 매칭된다. kernel virtual memory는 Virtual Memory 상에서 `PHYS_BASE(0xc0000000)`부터 시작하기에 Kernel Virtual Memory 0xc0000000은 physcial Address 0x00000000에 대응된다고 할 수 있다. 
```c
// In threads/vaddr.h
static inline void *
ptov (uintptr_t paddr)
{
  ASSERT ((void *) paddr < PHYS_BASE);

  return (void *) (paddr + PHYS_BASE);
}

static inline uintptr_t
vtop (const void *vaddr)
{
  ASSERT (is_kernel_vaddr (vaddr));

  return (uintptr_t) vaddr - (uintptr_t) PHYS_BASE;
}
```
위의 원리로 Pintos에서는 `ptov`에서는 physical address에 `PHYS_BASE`를 더해서 virtual address를 반환하고, `vtop`에서는 virtual address에 `PHYS_BASE`를 뺌으로써 physcial address를 구해 반환한다. 당연히 이 때 virtual address는 user virtual address가 아닌 (physical memory와 매핑되어 있는) kernel virtual address여야만 한다.

위 같은 작동이 가능하도록 pintos kernel 초기화(`main` in `init.c`)에서 `paging_init` 함수를 호출하여 kernel virtual memory와 physical memory간 Mapping을 생성한다.
```c
// threads/init.c
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

  asm volatile ("movl %0, %%cr3" : : "r" (vtop (init_page_dir)));
}
```
base page directory `init_page_dir`에 page를 할당한다. 물리주소 0부터 page의 크기(4kB)만큼 주소를 늘려가며 해당 Physical Address에 대응되는 kernel virtual address(0xc0000000 이상)에 대한 Page Table Entry를 `pte_create_kernel`을 통해 생성한다. 중간에 Page Directory, Page Table에 대한 공간 할당이 필요하다면 `palloc_get_page`를 통해 공간을 할당한다.
위 과정을 `init_ram_pages`(Physical Memory / 4kB, 가능한 페이지/프레임 개수)만큼 반복한다.
마지막으로 cr3 레지스터가 `init_page_dir`의 물리 주소를 가리키게 한다. 
이로써 Kernel Virtual Memory(존재하는 Physical Memory만큼)와 Physical Memory는 대응되게 된다. **즉 Kernel Virtual Address의 page를 physical frame처럼 취급할 수 있게 된다.**

위의 내용들은 Pintos에서 Kernel Virtual Memory를 통해 간접적으로 원하는 Physical Address의 Physical Memory에 접근할 수 있도록 한다.
아래 구현은 Pintos에서 Ker
```
```


#### Page Allocator
#### Page Directory
`paging_init`에 
##### `paging_init`
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

#### Manage 

### Limitations and Necessity
현재 Pintos에는 Frame이라는 개념이 존재하지만 `pagedir_set_page`에서 단순히 Virtual Page에 연결되는 물리 공간의 단위로만 사용될 뿐 유의미하게 관리되지 않는다. 그렇기에 만약 Page에 할당될 Frame이 부족할 경우(메모리의 물리적 공간의 부족) 어떤 frame을 page의 연결에서 끊어야 할지 결정할 때 대상이 되는 frame을 정할 때() 어려움을 겪는다. 
### Blueprint
아래 코드는 c와 유사한 문법을 작성한 대략적인 구조, 알고리즘을 나타낸 pseudo 코드이다.
#### Frame Table
```c
struct frame_table_entry
{
	uint32_t frame_no;
	tid_t tid;
	void *upage;
	void *kpage;
	bool in_use;
	struct list_elem elem;
}
```

```c
static struct lock frame_lock;
static struct list frame_table;
```

```c
void
frame_table_init()
{
	list_init(&frame_table);
	lock_init(&frame_lock);
}
```

```c
void *
vmalloc_get_page (enum vmalloc_flags)
{
	
}
```

```c
enum vmalloc_get_page
{
	VMAL_ASSERT = 001,
	VMAL_ZERO = 002,
	VMAL_USER = 004,
	VMAL_LAZY = 008
}
```

```c
void
vmalloc_free_page (void *page)
{
	
}
```

## 2. Lazy Loading
```c
```

## 3. Supplemental Page Table
```c
struct s_page_table_entry 
{
	bool in_use;
	bool in_swap;
	bool has_loaded;
	bool writable;
	bool is_dirty;
	bool is_accessed;
	struct frame_table_entry *frame;
	struct hash_elem elem;
}
```

## 4. Stack Growth

## 5. File Memory Mapping

### Basics
File Memory Mapping이란 파일을 가상 메모리 위의 연속적인 공간에 맵핑한 뒤, 일반적인 가상 메모리 접근과 동일하게 데이터에 접근하는 기법을 뜻한다.
```c
mapid_t
mmap (int fd, void *addr)
{
  return syscall2 (SYS_MMAP, fd, addr);
}

void
munmap (mapid_t mapid)
{
  syscall1 (SYS_MUNMAP, mapid);
}
```
Pintos 프로젝트에서 이를 구현하기 위해서는 프로젝트 2에서 구현한 시스템 콜 핸들러에 추가로 `mmap`과 `munmap` 시스템 콜을 구현해야 한다.

### Limitations and Necessity
- 파일의 특정 위치의 데이터를 읽어야 하는데 페이지 테이블 위에 정보가 없을 때만 page fault handler를 통해 해당 페이지 정보를 읽어오는 Lazy loading이 일어나기 때문에 파일을 한 번에 읽어오는 방법보다 오버헤드가 적고, page hit일 경우 기존 데이터를 재사용 가능하다는 장점이 있다.
- 또한 파일에 write 연산을 수행할 경우 디스크에 직접 연산이 일어나는 것이 아니라 메모리 상에만 수정사항이 반영되고, 추후에 페이지가 evict될 때 수정사항을 디스크에 한 번에 반영하여 write할 때도 오버헤드가 줄어드는 효과가 있다.

File Memory Mapping과 디스크 직접 접근에 대한 장단점을 정리하자면 다음과 같다.
| |장점|단점|
|-|-|-|
|File Memory Mapping|Lazy Loading을 통한 오버헤드 감소, Page hit일 때 데이터 재사용 가능|파일 크기 변경 불가, 메모리 부족으로 인한 파일 맵핑 크기 제한
|디스크 직접 접근|메모리 부족 X|잦은 접근에 대한 오버헤드 높음

현재 Pintos 프로젝트의 구현의 경우 File Memory Mapping이 구현되어있지 않기 때문에 대용량의 파일에 접근해야 할 경우 시스템 콜을 통해 디스크에 직접 접근해야 하며, 실행 파일 역시 데이터 전체를 메모리 위로 복사해야 하기 때문에 오버헤드가 크다.

### Blueprint
각 프로세스는 여러 개의 파일에 접근해 `mmap`을 호출할 수 있고, `mmap`으로 생성된 각 맵핑은 메모리 상의 여러 페이지와 대응된다. 따라서 이를 일관적으로 관리하기 위해서는 파일과 페이지의 연결 관계를 2차원 연결 리스트 형태로 나타내어 프로세스 별로 관리하는 것이 옳다고 생각되어 다음과 같이 구현 계획을 세웠다.

```c
struct fmm_data
{
  mapid_t id;
  struct file *file;

  struct list page_list;
  struct list_elem fmm_data_list_elem;
}
```
우선 `mmap`으로 맵핑된 각 파일에 대한 페이지들을 관리하기 위해 위와 같은 구조체를 선언한다. `id`는 각 맵핑마다 할당되는 고유한 넘버링이고, `*file`은 어떤 파일이 맵핑되었는지를 나타낸다. 해당 파일에 대한 접근으로 생성되는 페이지는 `page_list` 리스트에서 일괄적으로 관리한다.

```c
struct process
{
  ...
  struct list fmm_data_list;
  ...
}
```
그리고 프로세스 구조체 아래에 `fmm_data` 구조체들을 관리할 리스트인 `fmm_data_list`를 추가한다. `fmm_data` 구조체의 `fmm_data_list_elem`는 위 리스트의 원소로 사용되기 위해 필요하여 추가하였다. 파일에 대한 접근 및 맵핑 정보는 프로세스 별로 독립적이고, 한 프로세스 아래의 스레드들은 모두 동일한 가상 메모리 공간을 공유하므로 파일들에 대한 맵핑 정보인 `fmm_data_list`는 스레드가 아닌 프로세스에서 관리하는 것이 옳다고 판단하였다.

```c
mapid_t
sys_mmap (int fd, void *addr)
{
  if(
    /* empty file */ ||
    /* addr is not page alligned */ ||
    /* mapped page already exists in the range */ ||
    addr == NULL ||
    fd < 2
    )
      return MAP_FAILED;
    
  /* Create new struct fmm_data, initialize it and push into the list */
  /* allocate new mapid for new fmm */
  return mapid;
}
```
`mmap` 시스템 콜은 위와 같이 validity 확인을 해준 뒤 새 `fmm_data` 구조체를 동적할당받아 맵핑 정보를 설정해주는 식으로 구현할 예정이다.

```c
void
sys_munmap (mapid_t mapid)
{
  if(/* not a valid mapid */) return;

  struct fmm_data *fmm;
  for(/* i in fmm_data_list */)
  {
    if(i->id == mapid) fmm = i, break;
  }

  for(/* p in fmm->page_list */)
  {
    if(/* p is dirty */)
      /* update file on disk */
    /* delete p */
  }

  /* free fmm */
}
```
`munmap` 시스템 콜은 위와 같이 인자로 주어진 `mapid`에 대응하는 맵핑을 찾은 후, 해당 맵핑에서 생성한 페이지들을 모두 삭제해주도록 구현한다. 이때 페이지에 write 연산이 일어난 적이 있어 dirty bit가 설정되어 있을 경우 변경 사항을 디스크 위의 실제 파일에 반영해줘야 한다.

## 6. Swap Table

## 7. On Process Termination