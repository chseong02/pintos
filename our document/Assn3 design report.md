# Pintos Assn3 Design Report
Team 37 
20200229 김경민, 20200423 성치호 

Basics : the definition or concept, implementations in original pintos (if exists)
• Limitations and Necessity : the problem of original pintos, the benefit of implementation
• Blueprint: how to implement it (detailed data structures and pseudo codes

## 0. Background
TODO: 아래에 넣기 애매한 내용들/기반이 될 내용들 있다면 추가하기.

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
엔트리의 앞선 31~12bit는 각각 다른 page table의 시작 physical address의 31~12bit 부분을 담고 있다. 이 때 뒤의 12bit를 포함하지 않아도 되는 이유는 page table의 시작 위치가 4KB 정렬될 것이 보장되기 때문이다. 이는 추후 나올 Page Table Entry에서도 동일하다. 하위 11~0 bit에는 page directory entry에 대한 flag들이 포함된다. 
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
`pde_create`는 주어진 page table을 가르키는 page directory entry를 생성하는 함수로 base page directory를 초기화하는 `paging_init`에서 kernel virtual memory에 대한 page를 초기화할 때 또는 `lookup_page`에서 virtual address에 대한 page table entry가 없을 때, 생성하는 도중 사용한다.

각 Page Table Entry가 가르키는 Page Table은 1024개의 Page Table Entry로 구성되어 있다. 각 Page Table Entry는 아래 같은 구조를 가진다.
```c
 31                                   12 11 9      6 5     2 1 0
+---------------------------------------+----+----+-+-+---+-+-+-+
|           Physical Address            | AVL|    |D|A|   |U|W|P|
+---------------------------------------+----+----+-+-+---+-+-+-+
```
상위 31~12 비트는 해당 Page와 매핑된 Frame(Physical Memory 단위)의 상위 20비트이다. Frame은 Page와 유사하기 4KB이며 4kb로 aligned되어 있어 Frame의 시작 주소는 하위 12개 비트가 0임이 보장된다. PTE의 하위 12비트에는 Page Table Entry에 대한 Flag가 포함되어 있다.

| Flag    | 없을 때                        | 있을 때                  |
| ------- | --------------------------- | --------------------- |
| `PTE_U` | kernel만 접근 가능               | kernel, user 모두 접근 가능 |
| `PTE_P` | PTE 존재X, 다른 flag 모두 의미 없어짐. | PTE 존재O, 유효           |
| `PTE_W` | read-only                   | read/write 둘 다 가능     |

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

위의 내용들은 Pintos에서 Kernel Virtual Memory를 통해 간접적으로 원하는 Physical Address의 Physical Memory의 Frame에 접근할 수 있도록 한다.
아래 구현은 Pintos에서 User Virtual Address/Page에 Frame을 연결하는 방법이다. 
```
```


#### Page Allocator
TODO:

#### Manage 

### Limitations and Necessity
현재 Pintos에는 Frame이라는 개념이 존재하고 Physical Memory와 매핑된 Virtual Kernel Memory Page를 Frame으로써 사용한다. `pagedir_set_page`에서 `palloc_get_page` 해 얻은 page(kernel virtual address의 page, pintos에서 frame처럼 사용하는 page)를 user virtual address의 page로써 사용함으로써 user virtual page를 세팅한다. 이것이 frame과 관련된 구현의 전부로 frame이나 physical frame에 연결된 kernel virtual page를 별도로 관리하지 않아 frame이 부족할 때 evict할 page를 정하는데 어려움을 겪는다. 이를 개선하기 위해 어떤 Frame이 어떤 Page와 매핑되어 있는지를 관리하는 Frame Table이 필요하다.
### Blueprint
아래 코드들은 c와 유사한 문법을 작성한 대략적인 구조, 알고리즘을 나타낸 pseudo 코드이다.
우리는 Frame Table을 `list` 자료구조를 이용해 설계하기로 결정하였다. 
- 이와 같이 결정한 이유 중 하나는 `pintos`에서 `inode`를 이미 `list`를 이용해 관리하고 있기 때문이다. 또한 `list`를 이용한 구현이 간단하며 실제로 사용하고 있는 frame만 저장하기에 효율적이고 이후 clock 알고리즘을 evict policy로 사용할 시 구현이 상대적으로 편한 이점이 있다.
#### Frame Table
```c
static struct list frame_table;

struct frame_table_entry
{
	tid_t tid;
	void *upage;
	void *kpage;
	bool use_flag;
	struct list_elem elem;
}
```

| 멤버         | 자료형         | 설명                                         |
| ---------- | ----------- | ------------------------------------------ |
| `tid`      | `tid_t`     | 해당 frame을 점유하고 있는 thread의 id               |
| `upage`    | `void *`    | `kpage`와 매핑될 user virtual page             |
| `kpage`    | `void *`    | physical frame과 매칭되는 kernel virtual page   |
| `use_flag` | `bool`      | clock 알고리즘에서 사용할 use flag                  |
| `elem`     | `list_elem` | `frame_table`를 `list`로 구성하기 위한 `list_elem` |

```c
void
frame_table_init()
{
	list_init(&frame_table);
}
```

`palloc_get_page`를 비롯한 page allocator(실제로는 physical memory와 매핑된 kernel virtual page만을 반환하므로 frame allocator 역할을 수행) `palloc`을 대체하기 위한 `falloc` (Frame Allocator)를 추가한다. `falloc`은 기존 `palloc` 역할에 더해 `frame_table`을 함께 변경시킨다.
```c
enum falloc_get_page
{
	FAL_ASSERT = 001,
	FAL_ZERO = 002,
	FAL_USER = 004,
}
```
`vmalloc_get_page`를 위한 flag enum이다.

| Flag         | 없을 때                    | 있을 때                                                                    |
| ------------ | ----------------------- | ----------------------------------------------------------------------- |
| `FAL_ASSERT` | allocation 실패시 null 반환  | allocation 실패시 panic                                                    |
| `FAL_ZERO`   |                         | page 0으로 초기화. `PAL_ZERO`에 대응.                                           |
| `FAL_USER`   | page를 kernel pool에서 가져옴 | page를 user pool에서 가져옴. `PAL_USER`에 대응                                   |

```c
void *
falloc_get_frame_w_upage (enum falloc_flags, void* upage)
{
	void *kpage = palloc_get_page(falloc_flags except FAL_ASSERT);
	if(kpage == null)
	{
		//TODO: evict policy
		*kpage = palloc_get_page(falloc_flags);
		if(kpage == null)
			return null;
	}
	struct frame_table_entry *fte = malloc(sizeof *fte);
	if(fte == null)
	{
		palloc_free_page(kpage);
		if(falloc_flags & FAL_ASSERT)
			PANIC
		return null;
	}
		tid_t tid;
	void *upage;
	void *kpage;
	bool use_flag;
	fte->tid = thread_current()->tid;
	fte->upage = upage;
	fte->kpage = kpage;
	fte->use_flag = false;
	list_push_back(&frame_table, &fte->elem);
}
```

```c
*struct frame_table_entry
find_frame_table_entry_from_frame(void *frame)
{
	struct list_elem *e
	for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
	{
		struct frame_table_entry *fte = list_entry(e,struct frame_table_entry, elem);
		if(fte->kpage == frame)
			return fte;
	}
	return null;
}
```

```c
struct frame_table_entry *
find_frame_table_entry_from_upage(void *upage)
{
	struct list_elem *e
	for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
	{
		struct frame_table_entry *fte = list_entry(e,struct frame_table_entry, elem);
		if(fte->upage == upage)
			return fte;
	}
	return null;
}
```

```c
void
falloc_free_frame (void *frame)
{
	struct frame_table_entry *fte = find_frame_table_entry_from_frame(frame);
	if(fte == null)
	{
		// ERROR!
		return;
	}
	list_remove(&fte->elem);
	palloc_free_page(frame);
	free(&fte);
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