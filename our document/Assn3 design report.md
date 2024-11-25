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
pintos에서 주로 메모리를 관리할 때 사용하는 단위이다. Virtual Memory에서 Page를 논하게 되며 **Virtual Memory 4KB**이다. Virtual Memory는 Page 단위로 나누어져 할당 받거나 해제하는 등 관리된다. 주로 page를 다룰 때는 kernel virtual address를 `void*` 형식으로 가지고 다루게 된다. 
#### Frame
pintos에서 **Physical Memory**를 관리할 때 사용하는 단위로, page와 동일하게 **4KB**이다. pintos에서 page는 관리하기 위해 page directory, page table 등 을 구현하고, 함수들의 반환 값으로 사용하는 등 빈번하게 사용되는 반면, frame은 `pagedir_set_page`에서 언급되는 것을 제외하고는 다른 곳에서는 거의 사용되거나 언급되지 않는다. 
Pintos에서는 
`pagedir_set_page`는 후술할 Page Directory 및 Page Table에서 사용되는 함수로 주어진 Page directory에 user virtual page와 frame 매핑을 추가하는 함수이다. 이 때 frame은 kernel virtual address로 표현된다. TODO: frame
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
모든 프로세스(스레드)는 각자의 Page Directory를 가지고 있으며 독립적으로 관리하게 된다. 위 `pagedir`은 Page Directory로 사용되게 할당 받은 Page의 주소로, Page Directory 시작 점 위치이다.
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

```c
 31                                   12 11 9      6 5     2 1 0
+---------------------------------------+----+----+-+-+---+-+-+-+
|           Physical Address            | AVL|    |D|A|   |U|W|P|
+---------------------------------------+----+----+-+-+---+-+-+-+
```
0~11 bit
먼저 각 스레드의 `pagedir`에 해당하는 주소를 본다. 이는 page directory 테이블의 시작점이다. 해당 테이블 시작점부터 virtual address의 31~22bit 값, `page directory index`에 해당하는 위치의 값을 읽어 들인다. 해당 위치에는 한 page table의 시작점을 담고 있다. 

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

## 6. Swap Table

## 7. On Process Termination