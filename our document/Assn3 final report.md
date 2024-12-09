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


### 4. Stack Growth
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

## 발전한 점

## 한계

## 배운 점
Synchronization의 중요성과 관리의 어려움을 깨달았다. Assn2까지는 다른 스레드/프로세스간의 락 Acquire, Release만을 고려하면 되었다. 하지만 Assn3에서는 한 스레드에서 같은 락을 여려 번 acquire하려는 시도들이 많았다. 이는 주로 File Lock을 Acquire하고 파일을 읽거나 system call 등을 하는 도중 Page Fault가 일어나 Lazy Loading하는 경우 이를 위해 File Lock을 Acquire하려고 하다 발생하는 경우들이다. 이럴 때는 같은 스레드의 락 점유에 대한 예외 처리 등이 필요하기에 까다로웠다. 또한 File Lock과 Frame Table Lock 등 여러 Lock을 동시에 사용하는 경우 이 Acquire, Release 순서에 따라 Dead Lock이 발생 가능한 상황들이 존재하여 이에 유의하며 프로그래밍해야만 하였다. OS에서 배운 Dead Lock 회피 방법 등을 다시 떠올리고 실습할 수 있는 기회였다.