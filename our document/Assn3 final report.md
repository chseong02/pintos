# Pintos Assn3 Final Report
Team 37 
20200229 김경민, 20200423 성치호

## 구현
### 1. Frame Table
Virtual Memory의 Page에 대응(할당)되는 Physical Memory의 Frame을 체계적으로 관리하는 Frame Table을 `list` 자료구조를 이용하여 관리하게 구현하였다. 
Frame Table은 `frame_table_entry`들로 이루어진 `list`로 기존 디자인 계획에서 `tid`는 `thread`의 포인터로 대체하고, `use_flag`는 제외하였다. `thread`의 포인터를 저장하면 `thread_current()`을 이용하여 현재 스레드가 점유하고 있는 프레임인지 쉽게 확인이 가능하고 `use_flag`를 clock 알고리즘에서 사용할 것이라고 생각한 것과 달리 page table entry의 is_accessed bit를 이용하였기 때문이다.

```c
struct frame_table_entry
{
    struct thread* thread;
    void *upage;
    void *kpage;
    struct list_elem elem;
};
```

| 멤버       | 자료형              | 설명                                                              |
| -------- | ---------------- | --------------------------------------------------------------- |
| `thread` | `struct thread*` | 해당 frame을 점유하고 있는 thread, `upage`가 어떤 스레드의 user virtual page인지. |
| `upage`  | `void *`         | `kpage`와 매핑될 user virtual page                                  |
| `kpage`  | `void *`         | physical frame과 매칭되는 kernel virtual page                        |
| `elem`   | `list_elem`      | `frame_table`를 `list`로 구성하기 위한 `list_elem`                      |
```c
static struct list frame_table;
static struct lock frame_table_lock;
static struct frame_table_entry *clock_hand;
```
`frame_table`은 global로 관리되는 Frame Table 변수이며, `frame_table_lock`은 `frame`을 할당, 할당 해제하거나 frame table을 관리 등의 `sychronization`을 담당하는 `lock`이다. 디자인 계획에서는 `frame_table_lock`이 포함되지 않았으나 여러 스레드가 동시에 frame table 변경, frame 할당/할당 해제할 경우 많은 race condition이 발생 가능하기에 이를 예방하기 위해 추가하였다.

```c
void
frame_table_init (void)
{
    clock_hand = NULL;
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}
```
`frame_table_init`은 Frame Table 관리를 위한 초기화 함수로, 추후 설명할 evict policy(clock 알고리즘)에서 사용할 `clock_hand`, Frame Table `frame_table`, 이를 관리할 때 사용하는 락인 `frame_table_lock`을 초기화한다. 
```c
int
main (void)
{
  ...
  /* Initialize memory system. */
  palloc_init (user_page_limit);
  malloc_init ();
  paging_init ();
  frame_table_init();
  ...
}
```
`main`함수에서 호출하여 커널 초기화 과정에서 함께 초기화해준다.

```c
static struct frame_table_entry*
find_frame_table_entry_from_upage (void *upage)
{
    lock_acquire (&frame_table_lock);

    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); 
        e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        if (entry->upage == upage)
        {
            lock_release (&frame_table_lock);
            return entry;
        }
    }
    lock_release (&frame_table_lock);
    return NULL;
}
```
`frame_table`에서 entry의 `upage`가 입력 받은 `upage`인 frame table entry를 찾아 그 포인터를 반환하는 함수이다. 만약 그런 entry가 table에 없다면 NULL을 반환한다.
`frame_table` list를 처음부터 끝까지 순회하며 `entry->upage == upage`인 entry 주소를 반환한다. 이 때 `synchronization`을 위해 `frame_table_lock`을 이용한다.

거의 동일한 로직을 가진 함수들로 `find_frame_table_entry_from_frame (void *frame)`가 있으며 해당 함수는 입력 받은 `frame`에 대한 entry 주소를 반환한다. 

```c
static struct frame_table_entry*
find_frame_table_entry_from_frame_wo_lock (void *frame)
{
    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); 
        e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        if (entry->kpage == frame)
            return entry;
    }
    return NULL;
}
```
다음의 `find_frame_table_entry_from_frame_wo_lock`는 이름에서 알 수 있듯이 `find_frame_table_entry_from_frame`와 동일한 역활을 하나 lock을 사용하지 않는 함수이다. 해당 함수는 반드시 이미 락을 점유한 상황에서만 호출해야 한다. (caller가 락을 점유, 해제하는 경우) 이와 동일한 원리의 `find_frame_table_entry_from_upage_wo_lock`가 존재하며 해당 함수들은 디자인에서는 포함되지 않았으나 swap out과 같은 넓은 범위에서 `frame_table_lock`을 점유해야 하는 작업시 필요하여 추가하였다.

```c
/* Flags for falloc_get_frame_w_upage */
enum falloc_flags
{
    FAL_ASSERT = 001,  /* Panic When fail to allocate frame */
    FAL_ZERO = 002,    /* Fill Frame with Zero */
    FAL_USER = 004,    /* Get Frame from User pool */
};
```
아래의 `falloc_get_frame_w_upage`에서 사용하는 frame allocation시 사용하는 옵션이다. 기존 page allocator의 `enum palloc_flags`와 동일한 역할과 구성이다.

| Flag         | 없을 때                     | 있을 때                                   |
| ------------ | ------------------------ | -------------------------------------- |
| `FAL_ASSERT` | allocation 실패시 null 반환   | allocation 실패시 panic                   |
| `FAL_ZERO`   |                          | frame 0으로 초기화. `PAL_ZERO`에 대응.         |
| `FAL_USER`   | frame를 kernel pool에서 가져옴 | frame를 user pool에서 가져옴. `PAL_USER`에 대응 |

```c
void*
falloc_get_frame_w_upage (enum falloc_flags flags, void *upage)
{
    lock_acquire (&frame_table_lock);
    void *kpage;
    struct frame_table_entry *entry;
    enum palloc_flags _palloc_flags = 000;

    if (flags & FAL_ZERO)
        _palloc_flags |= PAL_ZERO;
    if (flags & FAL_USER)
        _palloc_flags |= PAL_USER;

    kpage = palloc_get_page (_palloc_flags);
    if (!kpage)
    {        
        page_swap_out ();
        if (flags & FAL_ASSERT)
            _palloc_flags |= PAL_ASSERT;
        kpage = palloc_get_page (_palloc_flags);
        if (!kpage)
        {
            lock_release(&frame_table_lock);
            return kpage;
        }
            
    }
    entry = malloc (sizeof *entry);
    if (!entry)
    {
        palloc_free_page (kpage);
        lock_release(&frame_table_lock);
        if (flags & FAL_ASSERT)
            PANIC ("NO Memory for Frame Table Entry!");
        return NULL;
    }
    entry->thread = thread_current ();
    entry->upage = upage;
    entry->kpage = kpage;
    list_push_back (&frame_table, &entry->elem);
    lock_release (&frame_table_lock);
    return kpage;
}
```
`falloc_get_frame_w_upage`는 `flag` 옵션에 맞게 `upage` page에 프레임을 할당하고 프레임의 주소를 반환하는 함수이다. 이는 `frame_table` 변경을 수반한다. 
`palloc_get_page`를 통해 입력 받은 옵션(`PAL_ASSERT`는 제외)에 대한 **frame(명확히는 kernel virtual page이지만 frame과 동일한 역할이므로 앞으로도 frame이라 명명하겠다.)을** 할당 시도한다. 
만약 실패시 이는 물리적 메모리가 부족한 것이므로 `page_swap_out`을 통해 Swap out으로 물리적 메모리를 확보 시도한다. 이후 다시 `palloc_get_page`를 통해 다시 프레임 할당을 시도하고 실패시 `FAL_ASSERT`라면 panic, 아니라면 `NULL`을 반환한다.
frame 할당 성공시 `malloc`을 통해 해당 frame에 대한 `frame_table_entry` 공간을 할당 받고 `frame_table_entry`의 멤버 `thread`, `upage`, `kpage`를 채워놓고 `list_push_back`을 통해 `frame_table`에 추가한다. 
기존 디자인 계획에서와 다르게 과정 중 frame table lock을 사용하였으며 특히 swap out하여 해제된 메모리를 로직상 곧바로 이어진 frame 할당에서 바로 사용할 수 있도록 강제할 수 있도록 유의하였다.
`falloc_get_frame_w_upage`는 기존에 `PAL_USER`옵션을 포함한 `palloc_get_page`을 대체하여 사용된다. 하지만 이 중 lazy loading의 대상이 되는 경우는 별도로 뒤에서 설명하겠다.

```c
void
falloc_free_frame_from_upage (void *upage)
{
    lock_acquire (&frame_table_lock);
    struct frame_table_entry *entry = find_frame_table_entry_from_upage_wo_lock (upage);
    if (!entry)
        return;
    
    if (clock_hand == entry)
    {
        struct list_elem *next = list_next (&entry->elem);
        if (next == list_end (&frame_table))
            clock_hand = NULL;
        else
            clock_hand = list_entry(next, struct frame_table_entry, elem);
    }
    list_remove (&entry->elem);
    palloc_free_page (entry->kpage);
    free (entry);
    lock_release (&frame_table_lock);
}
```
`falloc_free_frame_from_upage`는 입력받은 `upage`에 매핑된 `frame`을 할당해제하고 매핑을 `frame_table`에서 제거하는 함수이다. `find_frame_table_entry_from_upage_wo_lock`을 이용해 `upage`와 매핑된 frame table entry를 찾고 이런 entry가 존재할 시 `list_remove`를 통해 `frame_table`에서 제거하고 `palloc_free_page`를 통해 frame을 할당 해제한다. 그리고 entry가 할당하고 있었던 공간을 할당 해제한다. `clock_hand`의 조절은 "Swap Table"에서 설명하도록 하겠다.

비슷한 함수로 `falloc_free_frame_from_frame`이 존재하며 해당 함수는 입력 받은 `frame`을 할당해제하고 해당 프레임이 가지던 매핑을 `frame_table`에서 제거하는 함수이다.

```c
void
falloc_free_frame_from_frame_wo_lock (void *frame)
{
    struct frame_table_entry *entry = find_frame_table_entry_from_frame_wo_lock (frame);
    if (!entry)
        return;
    if (clock_hand == entry)
    {
        struct list_elem *next = list_next (&entry->elem);
        if (next == list_end (&frame_table))
            clock_hand = NULL;
        else
            clock_hand = list_entry(next, struct frame_table_entry, elem);
    }
    list_remove (&entry->elem);
    palloc_free_page (entry->kpage);
    free (entry);  
}
```
`falloc_free_frame_from_frame`와 동일한 역할을 수행하지만 `lock`을 사용하지 않는 함수이다. caller함수가 `frame_table_lock`을 점유한 상태에서만 사용되어야만 한다. 디자인에서는 없었지만 추가된 함수이다.

위의 함수들을 통해 Frame Table이 관리되며 이를 이용해 Frame을 쉽게 관리할 수 있어 Swap out시 Evict policy를 사용할 수 있는 기반이 된다. 또한 `falloc`이라는 `palloc`을 대체할 allocator를 제공하여 당장의 물리적 메모리가 부족하더라도 Swap out을 통해 물리 메모리를 확보한 후 할당 재시도를 할 수 있다.

### 3. Supplemental Page Table
순서상 Lazy Loading을 설명하여야 하나 Lazy Loading의 구현은 Supplemental Page Table이 구현되어 있어야만 가능하므로 Supplemental Page Table 구현부터 설명하고자 한다.

Supplemental Page Table은 기존 단순한 정보들만을 저장하는 Page Table/Page Directory의 보강판으로 주 목적으로는 Frame과 매핑되지 않은 Page들(Lazy Loading, Swap Out)이 존재 가능하게 하고 이들을 관리하기 위함이다.
Supplemental Page Table은 핀토스에서 기본적으로 제공하는 hash table 자료 구조 `hash`를 이용해 구현하였다.

```c
struct s_page_table_entry 
{
	bool in_swap;
    bool is_lazy;
	bool has_loaded;

	bool writable;
	
	struct file* file;
	off_t file_ofs;
	uint32_t file_read_bytes;
	uint32_t file_zero_bytes;
    enum falloc_flags flags;

	size_t swap_idx;

	void *upage;
	void *kpage;
	
	struct hash_elem elem;
};
```
supplemental page table을 이루는 entry인 `s_page_table_entry`이다.

| 멤버                | 자료형            | 설명                                                                                                                                                     |
| ----------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `in_swap`         | `bool`         | 현재 swap disk에 있는지 여부<br>swap disk에 있다면 true,                                                                                                           |
| `is_lazy`         | `bool`         | lazy loading의 대상인지 여부                                                                                                                                  |
| `has_loaded`      | `bool`         | lazy loading, swap in/out에서 사용되며 실제로 frame을 할당받았는지 여부.                                                                                                 |
| `writable`        | `bool`         | 기존 pte의 writable bit                                                                                                                                   |
| `file`            | `struct file*` | 어떤 파일을 loading해야하는지에 대한 변수<br>lazy loading에서 사용한다.<br>`is_lazy`가 참일 때 유효한 값.                                                                           |
| `file_ofs`        | `off_t`        | `file`의 어디부터 담은 page인지, lazy loading시 활용하기 위해 page만 미리 할당받을 때 해당 page가 file 중 어느 부분에 대한 것인지 정할 때 사용.<br>`is_lazy`가 참일 때 유효한 값.                         |
| `file_read_bytes` | `uint32_t`     | 기존 pintos 구현상 `load_segment`에서 `page`에 segment 저장시 사용하는 값.<br>`file` 값이 존재할 때만 유효함.                                                                    |
| `file_zero_bytes` | `uint32_t`     | 기존 pintos 구현상 `load_segment`에서 `page`에 segment 저장시 사용하는 값.<br>`file` 값이 존재할 때만 유효함.                                                                    |
| `flags`           | `falloc_flags` | frame할당시 `falloc_get_page`에서 사용할/사용한 옵션                                                                                                                |
| `swap_idx`        | `size_t`       | swap table의 index<br>`in_swap`이 `true`일 때만 유효한 값.                                                                                                      |
| `upage`           | `void *`       | `user virtual page`                                                                                                                                    |
| `kpage`           | `void *`       | `in_swap`이 거짓이고 `has_loaded`이 참일 때, 해당 user page `upage`에 매핑된 frame(kernel virtual page)<br>swap out, lazy loading에 의해 frame이 할당되지 않아 유효한 값이 아닐 수도 있다. |
| `elem`            | `hash_elem`    | `s_page_table`를 `hash`로 구성하기 위한 `hash_elem`                                                                                                            |
디자인 대비 `present`,`is_dirty`,`is_accessed`가 제외되었는데 `present`는 supplemental page table 내 존재 여부로, `is_dirty`, `is_accessed`는 기존 page table로 확인이 가능한 정보이기에 같은 정보가 중복되어 존재하는 상황을 피하기 위해 제거하였다.

```c
void
init_s_page_table (void)
{
    struct thread *thread = thread_current ();
    hash_init (&thread->s_page_table, s_page_table_hash_func, 
        s_page_table_hash_less_func, NULL);
}
```
스레드/프로세스별 `s_page_table`을 초기화하는 함수이다. supplemental page table은 기존 page table인 `pagedir`처럼 스레드별로 관리되어야만 하며 아래처럼 현재 `pagedir`을 초기화하는 `load`에서 함께 호출되어 초기화된다.
```c
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  ...
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  init_s_page_table();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();
```

```c
static unsigned
s_page_table_hash_func (const struct hash_elem *e, void *aux)
{
	struct s_page_table_entry *entry = hash_entry (e, struct s_page_table_entry, elem);
	return hash_bytes (&entry->upage, 4);
}
```
위 `s_page_table` 초기화에 사용되는 `hash_func`로 hash 값을 계산할 때 사용되는 함수이다. 각 스레드의 supplemental page table 내에서 반드시 유일한 값일 수밖에 없는 `upage`를 이용하여 hash 값을 생성하였다.

```c
static bool
s_page_table_hash_less_func (const struct hash_elem *a, 
    const struct hash_elem *b, void *aux)
{
	struct s_page_table_entry *_a = hash_entry(a, struct s_page_table_entry, elem);
	struct s_page_table_entry *_b = hash_entry(b, struct s_page_table_entry, elem);
	return _a->upage < _b->upage;
}
```
위 `s_page_table` 초기화에 사용되는 `hash_less_func`로 위와 동일하게 `upage` 값을 이용하였다.

```c
struct s_page_table_entry*
find_s_page_table_entry_from_upage (void* upage)
{
	struct s_page_table_entry entry;
    struct thread *thread = thread_current ();

	entry.upage = upage;
	struct hash_elem *finded_elem = hash_find(&thread->s_page_table, &(entry.elem));
	if (!finded_elem)
		return NULL;
	return hash_entry (finded_elem, struct s_page_table_entry, elem);
}
```
`find_s_page_table_entry_from_upage`는 입력 받은 page `upage`의 supplemental page table entry가 supplemental page table `s_page_table`에 존재하는지 확인하여 존재한다면 해당 entry의 pointer를, 존재하지 않는다면 NULL을 반환한다.
해당 함수가 검색하는 `s_page_table`은 현재 스레드의 `s_page_table`에서이다. 
`upage`가 입력받은 `upage`와 동일한 가상의 entry를 생성한 뒤 해당 entry의 hash 값과 동일한 entry가 있는지 `hash_find`를 통해 검색한다.

```c
struct s_page_table_entry*
find_s_page_table_entry_from_thread_upage (struct thread *t, void* upage)
{
	struct s_page_table_entry entry;

	entry.upage = upage;
	struct hash_elem *finded_elem = hash_find(&t->s_page_table, &(entry.elem));
	if (!finded_elem)
		return NULL;
	return hash_entry (finded_elem, struct s_page_table_entry, elem);
}
```
해당 함수는 위 함수와 다르게 `upage`를 어떤 스레드 `t`의 `s_page_table`에서 검색할지 조절할 수 있는 함수이다.

```c
bool
s_page_table_add (bool is_lazy, struct file *file, off_t file_ofs, 
	bool writable, void *upage, void *kpage, uint32_t file_read_bytes, 
	uint32_t file_zero_bytes, enum falloc_flags flags)
{
    struct s_page_table_entry *entry = malloc (sizeof *entry);
    if (!entry)
		return false;
        
    if (find_s_page_table_entry_from_upage (upage))
	{
		free (entry);
		return false;
	}
        
    struct thread *thread = thread_current ();
    entry->in_swap = false;
    entry->is_lazy = is_lazy;
    entry->has_loaded = !is_lazy;
	
	entry->writable = writable;
    
	entry->file = file;
	entry->file_ofs = file_ofs;
	entry->file_read_bytes = file_read_bytes;
	entry->file_zero_bytes = file_zero_bytes;
	entry->flags = flags;
	
	entry->upage = upage;
	entry->kpage = kpage;

	hash_insert (&thread->s_page_table, &entry->elem);

    return true;
}
```
`s_page_table` Supplemental Page Table에 Entry를 추가하는 가장 기본이 되는 함수로 가장 높은 커스터마이징 수준을 가지고 있다. `s_page_table`에 추가된 entry의 page는 기본 page table에 없거나 할당 받은 frame이 없을 수 있다. 아래에서 소개할 함수들은 내부적으로 `s_page_table_add`를 사용하여 구현된다. entry를 위한 공간을 `malloc`을 이용해 할당받고 입력받은 추가하고 싶은 엔트리의 `upage`와 중복되는 `upage`를 가진 엔트리는 없는지 `find_s_page_table_entry_from_upage`를 이용해 검사한다. 이미 있다면 entry를 위해 할당 받은 공간을 해제하고 false를 반환한다. 아직 존재하지 않는다면 입력 받은 정보를 바탕으로 entry member 변수를 설정한 뒤 `hash_insert`를 통해 해당 entry를 추가한다.
해당 함수는 여러 곳에서 사용하기에는 커스터마이징 수준이 너무 높고 호출이 복잡하기에 디자인 레포트와 다르게 아래 3가지의 새로운 인터페이스 함수들을 추가하였다. 

```c
bool
s_page_table_binded_add (void *upage, void *kpage, bool writable, enum falloc_flags flags)
{
    return s_page_table_add (false, NULL, 0, writable, upage, kpage, 0, 0, flags);
}
```
page에 frame이 이미 할당 매핑된 page에 대한 entry를 supplemental page table에 추가하는 함수이다. `s_page_table_add`를 활용해 구현되었다. 
이 때 이미 매핑되어 있어 lazy loading하지 않더라도 frame에 대한 `flags`를 입력 받는 이유는 추후 해당 페이지가 swap out되고 swap in될 때 frame을 초기화할 때 사용하기 위함이다.

```c
bool
s_page_table_file_add (void *upage, bool writable, struct file *file, 
	off_t file_ofs, uint32_t file_read_bytes, uint32_t file_zero_bytes, 
	enum falloc_flags flags)
{
    return s_page_table_add (true, file, file_ofs, writable, upage, NULL, 
        file_read_bytes, file_zero_bytes, flags);
}
```
File Lazy Loading에 사용되는 page table entry를 추가하는 함수이다. `file`, `file_ofs`,`file_read_bytes`, `file_zero_bytes`를 통해 추후 실제로 load시 어떤 파일을 어디부터 얼마큼 읽고 얼마큼 0으로 채울지를 받는다. 또한 추후 할당될 frame을 어떤 옵션을 초기화할지 `flags`를 받는다.

```c
bool
s_page_table_lazy_add (void *upage, bool writable, enum falloc_flags flags)
{
    return s_page_table_add (true, NULL, 0, writable, upage, NULL, 0, 0, flags);
}
```
File의 Lazy Loading이 아니라 추후 Lazy하게 frame을 할당하고 싶은 page를 위한 함수이다. 하지만 현재 코드 상에서 해당 함수를 사용하는 곳은 없다. stack growth에서 사용할 것으로 예상하였지만 현재 구현 방식상 stack growth가 가능한 범위에 대해 미리 page를 할당하지 않기 때문이다.

```c
void
s_page_table_delete_from_upage (void *upage)
{
    struct s_page_table_entry *entry;
    struct thread *thread = thread_current ();

    entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return;

	hash_delete (&thread->s_page_table, &entry->elem);
	free (entry);
}
```
`s_page_table`에서 entry의 `upage`가 입력 받은 `upage`인 entry를 삭제하는 함수이다. `find_s_page_table_entry_from_upage`를 통해 entry를 찾고 존재한다면 `hash_delete`를 이용해 table에서 entry를 제거한 뒤 `free`를 통해 entry에 할당되었던 공간을 해제한다.

활용사례...
### 2. Lazy Loading
Lazy Loading은 User program 파일을 프로세스/스레드 생성시 물리 메모리에 올리는 것이 아니라 실제로 실행하려고 할 때, 관련 내용이 메모리에 없어 page fault를 일으킬 때가 되어서야 해당 페이지의 프레임을 할당하고 로드함으로써 1 페이지씩 뒤늦게 메모리에 로드하는 것이다. 그렇기에 더 이상 page fault는 잘못된 메모리 접근의 의미만 가지는 것이 아니고 로드해야 하나 아직 메모리에 로드하지 않은 곳에 대한 접근 시도의 의미를 가질 수 도 있게 되었다. 그러므로 page fault handler 함수를 수정해주어야만 한다.

먼저 기존에 유저 프로그램을 실행하는 스레드에서 곧바로 실행 파일 전체를 로드하는 함수(`load_segment`)를 변경하였다.

```c
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  off_t new_ofs = ofs;
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      bool success = s_page_table_file_add(upage, writable, file, new_ofs, 
        page_read_bytes, page_zero_bytes, FAL_USER);
      if (!success)
      {
        return false;
      }
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      new_ofs += page_read_bytes;
    }
  return true;
}
```
기존에 loadable한 segment를 실제 memory에 load하는 함수인 `load_segment`를 실제 메모리에 load하는 대신 `s_page_table_fild_add` 함수를 통해 `s_page_table`에 supplemental page table entry만 추가하도록 변경하였다.
`page_read_bytes`, `page_zero_bytes`는 기존 공식대로 계산하며 해당 값들을 `s_page_table_file_add`에 넘겨 추후에 실제로 load할 때 해당 정보를 바탕으로 load할 수 있도록 했다. 또한 기존에는 `file_read`를 통해 자동으로 file을 읽는 포인트가 이동하였지만 현재는 실제로 파일을 읽지 않으므로 `new_ofs`를 통해 추후 읽어야할 위치를 기록해주었다. 또한 매 순회마다 `new_ofs`에 `page_read_bytes`를 더해 `file_read`와 동일하게 파일 내 읽어야할 위치를 계산하였다. 
```c
static void
page_fault (struct intr_frame *f) 
{
   ...
   if(user && !check_ptr_in_user_space (fault_addr))
      sys_exit (-1);
   /* User caused page fault */
   void* upage = find_page_from_uaddr (fault_addr);
   if (!upage)
   {
     ...
   }
   if (!is_writable_page (upage) && write)
      sys_exit (-1);
   
   bool success = make_page_binded (upage);
   if (!success)
      sys_exit (-1);
   return;
   ...
}
```
실행 파일 내용이 있어야 할 페이지에 대해 page fault가 일어나면 이를 감지하고 실제로 frame을 할당하고 파일 내용을 load해 주어야 하므로 해당 케이스를 page fault handler에서 감지하고 처리할 수 있어야 한다. 이를 위해 page fault handler함수를 다음처럼 변경하였다.
page fault 발생시 해당 page fault가 user의 kernel virtual address를 접근에 의한 것인지 검사한다. 그렇다면 이는 완전히 잘못된 접근이므로 `sys_exit(-1)`한다. 해당 경우가 아니라면 다음으로는 `find_page_from_uaddr (fault_addr);`을 통해 `s_page_table`에 존재하는 page 속 uaddr인지 확인한다. 만약 `s_page_table`에 존재하는 페이지에 대한 접근이라면 올바른 접근(lazy loading 또는 swap out된 page, frame이 할당된 page)이며 `find_page_from_uaddr`은 `fault_addr`가 포함된 `upage`(`s_page_table`에 엔트리가 존재하는)를 반환한다. 하지만 이 때 frame에 할당된 page라고 하더라도 read-only page에 대해 write를 시도하였다면 이는 잘못된 접근이다. 이를 `is_writable_page`를 통해 얻은 해당 page의 writable 여부와 `write`(page fault를 일으킨 접근의 write/read 여부)를 비교하여 잘못된 접근인지 검사한다. 만약 올바른 접근이였다고 판단되면 page를 `make_page_binded` 함수에 넘겨 해당 페이지가 frame을 할당 받고 의도한/예상하는 정보를 가지고 있게 만든다.

```c
void* find_page_from_uaddr (void *uaddr)
{
    void *upage = pg_round_down (uaddr);
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return NULL;
    return entry->upage;
}
```
입력받은 virtual address `uaddr`이 속한 page가 `s_page_table`에 존재하는지 검사하고 존재한다면 해당 페이지 주소를, 존재하지 않는다면 NULL을 반환하는 함수이다.


```c
bool is_writable_page (void *upage)
{
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return false;
    return entry->writable;
}
```
`is_writable_page`는 입력 받은 `upage` page가 writable한 page인지 반환하는 함수이다. 
우선 `find_s_page_table_entry_from_upage`를 통해 `upage` 에 대한 entry를 `s_page_table`에서 찾고 해당 entry의 `writable`을 반환한다.

```c
bool make_page_binded (void *upage)
{
    file_lock_acquire();
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
    {
        file_lock_release();
        return false;
    }
        
    if (entry->in_swap)
    {
        ...
    }
    if (!entry->has_loaded)
    {
        // NOT FOR FILE, just Lazy loading
        if (!entry->file)
        {
            file_lock_release();
            return false;
        }
        // File Lazy Loading
        
        uint8_t *kpage = falloc_get_frame_w_upage (entry->flags, entry->upage);

        if (!kpage)
        {
            file_lock_release();
            return false;
        }
            
        struct file *file = entry->file; 
        off_t ofs = entry->file_ofs;
        off_t page_read_bytes = entry->file_read_bytes;
        off_t page_zero_bytes = entry->file_zero_bytes;
        off_t writable = entry->writable;

        // We don't have to `file_open`, because It's not closed.
        file_seek (file, ofs);
        /* Load this page. */
        if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
            falloc_free_frame_from_frame (kpage);
            file_lock_release();
            return false; 
        }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);
        /* Add the page to the process's address space. */
        if (!install_page (upage, kpage, writable)) 
        {
            falloc_free_frame_from_frame (kpage);
            file_lock_release();
            return false; 
        }
        entry->has_loaded = true;
        entry->kpage = kpage;
        file_lock_release();
        return true;
    }
    file_lock_release();
    return false;
}
```
`make_page_binded`는 입력 받은 user virtual page `upage`에 frame을 매핑하고 해당 page가 가져야하는 정보를 가질 수 있게 해주는 함수이다. Swap In, Lazy Loading의 역할을 모두 수행하고 있으며 여기에서는 lazy loading에 초점을 맞추어 설명하도록 하겠다.
먼저 `find_s_page_table_entry_from_upage`를 통해 `upage`의 entry를 찾은 뒤 `entry->in_swap`이라면 현재 해당 페이지의 정보가 Swap Disk에 있는 것이므로 Swap In을 수행하여 메모리로 정보를 가지고 온다. 이에 해당되지 않고 `!entry->has_loaded`라면 Lazy Loading에 의해 아직 메모리에 로드되지 않았거나 이전 물리 메모리 확보를 위한 swap out 과정에서 dirty하지 않은 page의 frame을 할당 해제된(swap disk로 정보를 옮기지 않으므로 `in_swap`은 false임.) 경우이다. 이 둘은 완벽히 동일한 로직을 통해 해당 페이지가 정상적으로 가져야 할 정보를 메모리에 로드하게 된다. 먼저 `falloc_get_frame_w_upage`를 통해 `upage`에 매핑할 frame을 할당 받고 사전에 입력한 `s_page_table_entry`의 정보를 바탕으로 기존 `load_segment`의 로직을 수행한다. 우선 `ofs`로 `file`이 보는 위치를 옮긴 후, `page_read_bytes`만큼 파일에서 읽어온 뒤, `page_zero_bytes`만큼 0으로 채운다. 이후 `install_page`를 통해 page directory, page table에 upage ,kpage 매핑을 등록하고 `entry->has_loaded`를 true로, `entry->kpage`를 할당 받은 frame 주소로 두어 올바르게 load가 완료되었음을 표시한다. 
File Read를 하지 않는 로직들에 대해서도 앞서 `file_lock_acquire`을 하는 이유는 `file_lock`과 `frame_table_lock`의 접근 순서에 따라 dead lock이 발생할 수 있는 가능성이 존재하기에 이 접근 순서를 통일하기 위함이다.

### 4. Stack Growth
Stack growth는 핀토스 기존 구현의 1 page로 고정되어 있는 user stack을 동적으로 확장할 수 있는 기능이다. 이를 위해 lazy loading과 유사한 방식으로 구현하였다. 우선은 기존처럼 1 page의 user stack을 할당하여 사용한다. 만약 해당 범위를 넘어서 접근하려고 한다면 page fault를 발생시키게 된다. 이 때 우리는 해당 page fault가 올바른 stack에 대한 접근에 의해 발생한 page fault인지 검사하고 그렇다면 동적으로 stack을 신장한다. 
```c
static void
page_fault (struct intr_frame *f) 
{
   ...
   if(user && !check_ptr_in_user_space (fault_addr))
      sys_exit (-1);
   /* User caused page fault */
   void* upage = find_page_from_uaddr (fault_addr);
   if (!upage)
   {
      void* esp = f->esp;
      if (!user)
         esp = thread_current ()->last_esp;
         
      if (is_valid_stack_address_heuristic (fault_addr, esp))
      {
         if (make_more_binded_stack_space (fault_addr))
            return;
      }
      sys_exit (-1);
   }
   ...
}
```
앞서 설명하였던 것처럼 page fault 발생시 해당 page fault가 user의 kernel virtual address를 접근에 의한 것인지 검사한다. 그렇다면 이는 완전히 잘못된 접근이므로 `sys_exit(-1)`한다. 해당 경우가 아니라면 다음으로는 `find_page_from_uaddr (fault_addr);`을 통해 `s_page_table`에 존재하는 page 속 uaddr인지 확인한다. 이를 통해 lazy loading 대상의 page인지, swap out된 page인지 등, 사전에 할당한 page(frame을 할당 받지 않은 page일 수 있다.)인지 확인한다. 만약 이 또한 아니라면 이제 stack growth를 해야하는 상황인지를 검사하게 된다.
`user`에서 발생한 page fault라면 interrupt frame의 `esp`가 valid한 값을 가지게 된다. (user->kernel 이동시 esp 값 등록) 하지만 `kernel`에서 발생한 page fault라면  interrupt frame의 `esp`가 valid하다고 할 수 없다. kernel -> kernel 시에는 esp 값을 기록하지 않는다.
```c
struct thread
  {
  ...
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct hash s_page_table;           /* Supplemental Page Table */
    struct process* process_ptr;
    void* last_esp;
#endif
  ...
  }
```
이러한 상황(kernel -> kernel)에 valid한 esp를 얻기 위해 별도로 esp를 관리해주었다. 
`thread`에 `last_esp`를 추가하였다.
```c
static void
syscall_handler (struct intr_frame *f) 
{
  int arg[4];
  if (!check_ptr_in_user_space (f->esp))
    sys_exit (-1);
  thread_current ()->last_esp = f->esp;
```
page fault 발생한 것이 kernel인 경우 + user stack growth가 필요한 경우는 user process 가 system call을 호출하였을 때이다. 그러므로 syscall이 호출하여 `syscall_handler`가 호출되었을 때마다 현재 `esp` 값을 `t->last_esp`에 넣어 준다. 이로써 page fault handler에서 `kernel`에 의한 page fault 일 때도 esp를 얻을 수 있다. 이 값을 활용하여 `is_valid_stack_address_heuristic`를 통해 해당 page fault 주소가 stack growth를 필요로 하는 올바른 주소 접근인지 판별한다.
그렇다고 판단되면 `make_more_binded_stack_space (fault_addr)`를 통해 해당 주소에 대한 page의 frame을 할당해주어 stack growth를 수행한다.

```c
bool is_valid_stack_address_heuristic (void *fault_addr, void *esp)
{
    return ((uint32_t) fault_addr >= (uint32_t) PHYS_BASE - (uint32_t) 0x00800000
        && (uint32_t) fault_addr >= (uint32_t) esp - 32);
}
```
page fault가 발생한 주소가 stack growth를 해주어야 하는 주소인지 판별하는 휴리스틱한 함수이다. 우선 stack의 최대 크기인 8MB를 넘지 않는지 확인한다. 이후 `push`, `pusha`에 의해 최대로 접근 가능한 `esp-32`보다는 같거나 위에서 접근한 시도인지 확인한다. 만일 `esp`를 `sub` 등을 통해 조작한 경우에도 해당 조건을 반드시 만족하게 된다. 이를 통해 휴리스틱하게 올바른 stack에 대한 접근인지, stack growth를 해야 하는 상황인지 판별 후 그 결과를 bool로 반환한다.
디자인 계획에서는 `esp-32`의 조건만을 고려하여 잘못된 `esp` 에 대한 처리가 없었으나 구현에서 추가하였다. 또한 조건 판별을 위한 별도의 함수로 구분하였다.

```c
bool make_more_binded_stack_space (void *uaddr)
{
    void *upage = pg_round_down (uaddr);
    uint8_t *kpage;
    bool success = false;

    /* PALLOC -> FALLOC */
    kpage = falloc_get_frame_w_upage (FAL_USER | FAL_ZERO, upage);
    if (kpage != NULL) 
    {
        success = install_page (upage, kpage, true) && 
        s_page_table_binded_add (upage, kpage, true, FAL_USER | FAL_ZERO);
        if (!success)
        {
            /* PALLOC -> FALLOC */
            falloc_free_frame_from_frame (kpage);
        }
    }
    return success;
}
```
`stack`내 주소로 판별된 `uaddr`이 포함되는 page를 할당하고 이를 위한 frame 할당을 수행하는, 즉 stack growth를 실질적으로 수행하는 함수이다.
`uaddr`은 반드시 `is_valid_stack_address_heuristic`를 통해 검증되었음이 가정된다. 유저 스택 공간을 위한 `FAL_USER | FAL_ZERO`옵션을 이용한 `falloc_get_frame_w_upage`를 통해 frame을 할당 받고 `install_page`를 통해 page table에 등록, `s_page_table_binded_add`를 이용하여 `s_page_table`에 frame 매핑된 page의 entry로써 추가한다. 이렇게 `s_page_table`에 등록된 page는 다른 page들과 동일하게 상황에 따라 swap out/in의 대상이 될 수 있다.

위 함수들을 활용하여 Stack Growth를 구현함으로써 기존에 1 page로 고정되어 있던 User Stack은 올바른 stack 주소에 대한 page fault 발생시 동적으로 Stack 공간을 추가로 할당함으로써 Stack Growth를 수행한다.
### 5. File Memory Mapping

#### process.h / process.c
파일 맵핑은 각 프로세스마다 관리하므로, `process.h`에 다음과 같이 각 맵핑에 대한 정보를 나타내는 구조체와 맵핑 정보를 저장할 리스트를 추가했다.

```c
struct fmm_data
{
  mapid_t id;
  struct file *file;
  int file_size;
  int page_count;
  void *upage;
  struct list_elem fmm_data_list_elem;
};
```
```c
struct process
{
  ...
  struct list fmm_data_list;
  int mmap_count;
};
```

Design에서 설계했던 구조에서 약간의 변동사항이 있다. Design에서는 각 맵핑마다 맵핑된 페이지들을 관리하는 리스트인 `struct list page_list`를 추가하여 페이지들을 관리하고자 했는데, 한 맵핑 상에 있는 모든 페이지는 연속적으로 위치해야 하고 각 페이지를 일일이 할당해주는 데 `O(n)`의 시간이 필요하므로 리스트를 사용하는 데 이점이 없다고 판단하여 대신 파일 크기, 페이지 수 및 할당된 가상 공간 주소를 저장하도록 변경하였다.

```c
void
init_process (struct process *p)
{
  ...
  list_init (&p->fmm_data_list);
}
```
맵핑 정보를 담는 리스트인 `fmm_data_list`는 프로세스 시작 중 초기화한다.

#### syscall.h / syscall.c

`mmap`과 `munmap`은 시스템 콜의 일종이므로 Project 2에서 시스템 콜을 구현한 코드에 이어 구현하였다.

```c
static mapid_t
sys_mmap (int fd, void *addr)
{
  /* check file validity */
  int file_size = sys_filesize(fd);
  if(file_size <= 0)
  {
    /* zero or error */
    return MAP_FAILED;
  }

  /* check address align */
  if(addr == NULL || (int)addr % PGSIZE)
  {
    return MAP_FAILED;
  }

  /* check if page already exists in range */
  for(int i = 0; i < file_size; i += PGSIZE)
  {
    if(find_s_page_table_entry_from_upage(addr + i) != NULL)
    {
      return MAP_FAILED;
    }
  }
  ...
```
`fd`와 `addr` 관련 validity check을 우선 수행한다. 체크해야 할 사항은 다음과 같다.
- `fd`가 가리키는 파일이 정상적이고 파일의 크기가 0보다 클 것
- `addr`이 0이 아니고 page aligned 되어있을 것
- 파일이 맵핑되어야 할 가상 메모리 공간에 이미 할당되어있는 페이지가 없을 것

`fd`의 validity check는 `sys_filesize`를 호출할 때와 동일하게 진행하므로 중복 코드를 사용하지 않았다. 그리고 맵핑 범위 내 페이지 존재 체크는 `find_s_page_table_entry_from_upage`를 통해 범위 내 각 페이지들마다 supplemental page가 존재하는지를 체크해줬다.

```c
  ...
  file_lock_acquire();

  /* open same file again because original file can be 
     closed after mmap but mmap should stay */
  struct process *cur = thread_current()->process_ptr;
  struct file *f = cur->fd_table[fd].file;
  struct file *new_f = file_reopen(f);
  if(new_f == NULL)
  {
    file_lock_release();
    return MAP_FAILED;
  }

  /* allocate mapping data & setup */
  struct fmm_data *fmm = malloc(sizeof(struct fmm_data));
  if(fmm == NULL)
  {
    file_lock_release();
    return MAP_FAILED;
  }
  fmm->id = cur->mmap_count++;
  fmm->file = new_f;
  fmm->file_size = file_size;
  fmm->page_count = 0;
  fmm->upage = addr;
  ...
```
다음 과정으로 맵핑하고자 하는 파일을 다시 열어준 뒤, 맵핑 정보를 담을 구조체를 동적할당하여 정보를 담는다. 파일 시스템을 건드리므로 동시성 확보를 위해 Project 2에서 사용했던 filelock을 걸어줘야 한다.

```c
  ...
  /* set page table for every pages in range */
  for(int i = 0; i < file_size; i += PGSIZE)
  {
    int page_data_size = file_size - i >= PGSIZE ? PGSIZE : file_size - i;
    s_page_table_file_add(addr + i, true, new_f, i, 
    page_data_size, PGSIZE - page_data_size, FAL_USER);
    fmm->page_count++;
  }

  /* add mapping data to list */
  list_push_back(&(cur->fmm_data_list), &(fmm->fmm_data_list_elem));

  file_lock_release();
    
  return fmm->id;
}
```
실질적으로 맵핑을 수행하는 파트. 파일 크기만큼의 데이터가 모두 할당될 수 있을 만큼의 페이지를 `s_page_table_file_add` 함수를 통해 할당하고 마지막 페이지에 공간이 남을 시 `file_zero_bytes` 파라미터로 넘겨준다. 마지막으로 프로세스의 `fmm_data_list`에 새로 생성한 맵핑 정보를 넣고 filelock을 해제 후 새로 할당받은 맵핑 id를 반환한다.

```c
static void
sys_munmap (mapid_t mapping)
{
  struct process *cur = thread_current()->process_ptr;
  struct list *fmm_list = &(cur->fmm_data_list);

  struct fmm_data *found_entry = NULL;
  struct list_elem *e;

  file_lock_acquire();

  /* search for mapping with (id: mapping) in list */
  for(e = list_begin(fmm_list); e != list_end(fmm_list); e = list_next(e))
	{
		struct fmm_data *entry = list_entry(e, struct fmm_data, fmm_data_list_elem);
    if(entry->id == mapping)
    {
      found_entry = entry;
      break;
    }
  }

  /* mapping not found */
  if(found_entry == NULL)
  {
    file_lock_release();
    return;
  }
  ...
```

`sys_munmap`은 `mmap`에서 할당받았던 맵핑 id를 파라미터로 받아 해당하는 맵핑을 찾아 해제하는 함수이다. 우선 프로세스의 `fmm_data_list`에서 맵핑 id에 해당하는 맵핑이 존재하는지 검색하고, 존재할 시 `found_entry`에 저장한다.

```c
  ...
  /* free each s_page entry. modify original file if dirty_bit is on. */
  for(int i = 0; i < found_entry->file_size; i += PGSIZE)
  {
    struct s_page_table_entry *s_page 
      = find_s_page_table_entry_from_upage(found_entry->upage + i);
    if(pagedir_is_dirty(thread_current()->pagedir, found_entry->upage + i))
    {
      void *page = pagedir_get_page(thread_current()->pagedir, found_entry->upage + i);
      file_write_at(s_page->file, page, s_page->file_read_bytes, s_page->file_ofs);
    }
    s_page_table_delete_from_upage(found_entry->upage + i);
  }

  /* remove entry from mapping list */
  list_remove(e);
  free(found_entry);

  file_lock_release();
}
```

찾은 `found_entry`의 정보를 기반으로 mmap에서 할당한 페이지들을 동일한 방법으로 순회하며 할당 해제한다. 만약 `pagedir_is_dirty`에 따라 해당 페이지가 변경된 적이 있다고 판별될 시 `file_write_at`를 호출하여 해당 페이지의 변경 사항을 파일에 lazy하게 반영한다. 확인한 각 페이지는 `s_page_table_delete_from_upage`를 통해 해제한다.

마지막으로 `fmm_data_list`에서 찾은 맵핑 정보를 삭제한 후 동적할당까지 해제해준다.

```c
void
sys_exit (int status)
{
  ...
  /* naive free logic for mmap on exit */
  for(mapid_t i = 0; i < cur->process_ptr->mmap_count; i++){
    sys_munmap(i);
  }
  ...
}
```
프로세스가 종료될 때 맵핑된 파일들이 있다면 모두 할당 해제해줘야 하는데, 이는 `sys_exit`에서 수행한다. 현재까지 맵핑된 파일 수를 `mmap_count`에다 기록한 후 해제할 때 모든 맵핑 id를 확인하여 `munmap`해준다. 만약 이미 해제한 맵핑이라 하더라도 `sys_munmap`에서 아무 일도 일어나지 않도록 처리한다.

### 6. Swap Table
### 7. On Process Termination

프로세스가 종료될 때 동적할당한 자원들을 모두 해제해줘야 메모리 누수가 생기지 않는다. Project 2의 테스트 케이스로 Project 2 시점까지 메모리 누수가 생기지 않는다는 것을 확인했으므로 Project 3에서 새롭게 할당한 자원에 대해서 해제를 해주면 된다.

앞서 설명한 `mmap`에서의 파일 맵핑 정리와 더불어 frame table, supplementary page table을 할당 해제해줘야 한다.
#### process.c
```c
/* Free the current process's resources. */
void
process_exit (void)
{
  ...
  if (pd != NULL) 
    {
      free_frame_table_entry_about_current_thread ();
      free_s_page_table ();
      ...
    }
}
```
기존의 `process_exit`에서 프로세스를 종료하며 자원을 할당해제할 때 다음과 같은 함수들을 추가로 호출하여 Assn3에서 추가로 사용한 자원을 할당 해제한다.
#### frame-table.c
```c
void
free_frame_table_entry_about_current_thread ()
{
    lock_acquire (&frame_table_lock);
    struct list_elem *e;
    struct thread *t = thread_current ();
    
    for (e = list_begin (&frame_table); e != list_end (&frame_table);
         e = e)
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);   
        e = list_next (e);
        if(entry->thread == t)
        {
            clock_hand = NULL;
            falloc_free_frame_from_frame_wo_lock (entry->kpage);
        }
    }
    lock_release (&frame_table_lock);
}
```
추가한 Frame table의 entry를 할당 해제해주는 함수. 동시성을 위해 앞서 설명한 `frame_table_lock`을 걸어준 뒤, `frame_table`을 순회하며 현재 스레드가 할당한 frame의 entry에 대해`falloc_free_frame_from_frame_wo_lock`를 실행하여 frame의 할당 해제, entry에 대한 할당 해제를 수행한다. 본래 프레임에 대한 할당 해제는 `pagedir_destory`에서 이루어졌으나 `pagedir_destory`에서 frame 할당 해제하는 것을 제거하고 `free_frame_table_entry_about_current_thread`에서 수행되도록 하였다.
#### s-page-table.c
```c
void
free_s_page_table (void)
{
    struct thread *thread = thread_current ();
    hash_clear (&thread->s_page_table,s_page_table_hash_free_action_func);
}
```
현재 스레드의 Supplemental page table을 할당 해제해주는 함수이다. s-page table은 해시맵으로 구현되어 있으므로 `hash_clear` 내장함수를 통해 테이블의 모든 entry를 순회하며 각 entry에 대해`s_page_table_hash_free_action_func`를 수행한다.

```c
static void
s_page_table_hash_free_action_func (struct hash_elem *e, void *aux)
{
    struct s_page_table_entry *entry = hash_entry (e, struct s_page_table_entry, elem);
    if(entry->in_swap)
	{
		delete_swap_entry(entry->swap_idx);
	}
	
	hash_delete (&(thread_current())->s_page_table, &entry->elem);
	free (entry);
}
```
`hash_clear`함수를 통해 `s_page_table`의 모든 entry에 대해 수행되는 함수로 각 entry가 `in_swap` 즉 해당 페이지가 swap disk에 정보를 저장하고 있다면 swap disk에서 차지하고 있는 공간을 `delete_swap_entry`을 통해 자유롭게 한다. 이후 `hash_delete`를 통해 해당 entry를 `s_page_table`에서 제거하고 entry 자체에 할당된 공간을 해제한다.

```c
bool
delete_swap_entry (size_t swap_idx)
{
    block_sector_t page_start_sector_idx;
    lock_acquire (&swap_table.lock);
    if (!bitmap_test (swap_table.used_map, swap_idx))
    {
        lock_release (&swap_table.lock);
        return false;
    }

    bitmap_set_multiple (swap_table.used_map, swap_idx, 1, false);
    lock_release (&swap_table.lock);
    return true;
}
```
입력받은 `swap_idx`에 해당하는 Swap Disk Pool 내 비트를 free하다고 표시를 변경하는 함수이다.

## 발전한 점
이번 Project 3를 통해 가상 주소 공간 (Virtual Address)에 실제 메모리 (Physical Address)를 일정한 크기의 페이지 단위로 대응시키는 Virtual Memory 기능을 구현하여 물리적 공간과 독립적인 주소 공간에 프로그램이 실행될 수 있도록 하였다. 그리고 페이지를 디스크 공간에 저장하고 불러올 수 있는 Swap disk 기능을 함께 구현하여, 제한된 물리적 메모리 공간보다 더욱 많은 공간이 필요한 경우에도 유연하게 프로그램 실행이 가능하도록 하였다.

Page 관련 기능 구현을 통해 더욱 폭넓은 유저 프로그램 지원이 가능해졌다. 이전에는 스택 공간이 1페이지(4KB)를 넘을 수 없어 유저 스택이 많이 쌓이는 복잡한 유저 프로그램의 실행이 불가능하였다. 하지만 이제는 스택 공간이 1페이지 이상으로 자라나 page fault를 일으킬 시 자동으로 새 페이지를 할당하는 Stack Growth 기능을 구현하여 더욱 복잡한 유저 프로그램을 지원할 수 있게 되었다.

그리고 디스크에서 파일을 불러올 때도 물리 메모리 공간에 파일을 모두 읽어오는 대신 `mmap`이라는 기능을 이용하여 가상 주소 공간에 파일을 대응시켜 page fault를 통해 파일의 필요한 부분만 페이지 단위로 필요할 때 불러올 수 있게 File Memory Mapping 기능을 구현하였다. 페이지를 필요할 때만 불러오는 기능을 lazy loading이라고 하는데, 만약 파일이 변경되었을 경우 변경사항은 디스크 위의 파일에 바로 반영되지 않고 파일 맵핑이 해제될 시 변경이 일어난 페이지만 디스크에 반영되도록 쓰기 연산에도 lazy loading이 일어나도록 하였다.

마지막으로 프로세스가 종료될 때 메모리 누수가 일어나지 않도록 새로 할당한 자원들을 해제해주는 과정을 거친다.

## 한계
File Lock의 경우 락을 점유하고 있는 스레드가 다시 락을 점유하려고 하는 경우에 대해 락을 점유하거나 해제하지 않는 예외 처리를 하여 처리해주었다. 하지만 이보다는 user 모드에서의 lock과 kernel 모드에서의 lock이 분리되거나 이 둘의 로직이 동시에 실행될 수 없다는 사실을 이용하여 새로운 synchronization 를 구현하여 개선되면 좋을 것이다. 

현재 Frame Table은 linked list로 구현되어 원하는 frame을 찾는데 O(n)의 시간이 소모된다. 이를 evict policy를 위한 효율적인 iterator를 가지는 동시에 hash, index를 통해 효율적인 frame table entry 찾기가 가능한 자료구조로 변경하면 좋을 것이다. 

현재 구현상 기존의 page table, page directory와 `s_page_table`은 상당수의 정보(`upage`, `kpage`,`writable` 등)가 중복된다. 이는 메모리 관리에 대한 공간 overhead를 2배 가까이 늘리는 원인이 된다. 기존의 page directory, table과 `s_page_table`의 정보를 한 테이블로 관리할 수 있다면 메모리overhead를 감소시키고 entry 조회시간도 감소시킬 수 있을 것이다. 기존의 page table entry의 flag를 위한 공간에 남는 비트가 있어 두 테이블의 정보를 합치는 것이 가능할 것으로 보인다.

또한 아쉬운 점으로 프로세스 종료 시 파일 맵핑을 해제할 때 지금까지 맵핑했던 파일 수만큼 모든 맵핑 번호를 확인하며 `munmap`을 시도하도록 간략하게 구현했다는 점이 있는데, 특정 매핑 id가 존재하는지 빠르게 확인 가능 + 삽입 및 삭제가 편하도록 `fmm_data`를 해시맵 등으로 관리했으면 더욱 효율적이었을 것이라 예상한다.

마지막으로 아직까지 어느 자원이 thread와 process 중 어디에서 관리되어야 하는지 완전히 이해하지 못한 점이 있다. 현재 Pintos 구현에서는 `sys_exec` 등으로 포크를 할 때 스레드가 아니라 새로운 프로세스를 생성하므로 한 프로세스 당 하나의 스레드만을 가지고 있다. 따라서 스레드에서 관리해야 할 자원들을 프로세스에서 관리해도 동작상 큰 문제가 없고 반대도 마찬가지인데, 이러한 모호함 때문에 일부 자원들의 배치와 관리가 잘못된 위치에서 되고 있는 것 같다는 생각이 들었다.

## 배운 점
Synchronization의 중요성과 관리의 어려움을 깨달았다. Assn2까지는 다른 스레드/프로세스간의 락 Acquire, Release만을 고려하면 되었다. 하지만 Assn3에서는 한 스레드에서 같은 락을 여려 번 acquire하려는 시도들이 많았다. 이는 주로 File Lock을 Acquire하고 파일을 읽거나 system call 등을 하는 도중 Page Fault가 일어나 Lazy Loading하는 경우 이를 위해 File Lock을 Acquire하려고 하다 발생하는 경우들이다. 이럴 때는 같은 스레드의 락 점유에 대한 예외 처리 등이 필요하기에 까다로웠다. 또한 File Lock과 Frame Table Lock 등 여러 Lock을 동시에 사용하는 경우 이 Acquire, Release 순서에 따라 Dead Lock이 발생 가능한 상황들이 존재하여 이에 유의하며 프로그래밍해야만 하였다. OS에서 배운 Dead Lock 회피 방법 등을 다시 떠올리고 실습할 수 있는 기회였다.

그리고 이번 프로젝트를 진행하며 처음으로 실행 환경에 따라 테스트 결과가 달라지는 상황을 맞닥뜨렸는데, 정확히 어떤 변수에 의해 다른 결과가 발생하는 건지 디버깅을 오랜 기간동안 해봤는데도 알아내지 못해 아쉬움이 남는다.