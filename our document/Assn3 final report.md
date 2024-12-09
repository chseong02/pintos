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
Supplemental Page Table은 

### 2. Lazy Loading

### 4. Stack Growth
### 5. File Memory Mapping
### 6. Swap Table
### 7. On Process Termination

## 발전한 점

## 한계

## 배운 점
Synchrono