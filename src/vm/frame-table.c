#include "vm/frame-table.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include <list.h>
#include "userprog/pagedir.h"

static struct list frame_table;
static struct lock frame_table_lock;
static struct frame_table_entry *clock_hand;

struct frame_table_entry
{
    tid_t tid;
    struct thread* thread;
    void *upage;
    void *kpage;
    bool use_flag;
    struct list_elem elem;
};

void
frame_table_init (void)
{
    clock_hand = NULL;
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

void*
falloc_get_frame_w_upage (enum falloc_flags flags, void *upage)
{
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
        //TODO: evict policy
        printf("스왑한다!!!!\n");
        page_swap_out ();
        if (flags & FAL_ASSERT)
            _palloc_flags |= PAL_ASSERT;
        kpage = palloc_get_page (_palloc_flags);
        if (!kpage)
            return kpage;
    }

    entry = malloc (sizeof *entry);
    if (!entry)
    {
        palloc_free_page (kpage);
        if (flags & FAL_ASSERT)
            PANIC ("NO Memory for Frame Table Entry!");
        return NULL;
    }

    entry->tid = thread_current()->tid;
    entry->thread = thread_current ();
    entry->upage = upage;
    entry->kpage = kpage;
    entry->use_flag = false;

    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &entry->elem);
    lock_release (&frame_table_lock);
    return kpage;
}

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

static struct frame_table_entry*
find_frame_table_entry_from_frame (void *frame)
{
    lock_acquire (&frame_table_lock);

    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); 
        e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        if (entry->kpage == frame)
        {
            lock_release (&frame_table_lock);
            return entry;
        }
    }
    lock_release (&frame_table_lock);
    return NULL;
}

void
falloc_free_frame_from_upage (void *upage)
{
    struct frame_table_entry *entry = find_frame_table_entry_from_upage (upage);
    if (!entry)
        return;
    
    lock_acquire (&frame_table_lock);
    if (clock_hand == entry)
    {
        clock_hand = list_next (entry);
        if (clock_hand == list_end (&frame_table))
            clock_hand = NULL;
    }
    list_remove (&entry->elem);
    lock_release (&frame_table_lock);
    palloc_free_page (entry->kpage);
    free (entry);
}

void
falloc_free_frame_from_frame (void *frame)
{
    struct frame_table_entry *entry = find_frame_table_entry_from_frame (frame);
    if (!entry)
        return;
    lock_acquire (&frame_table_lock);
    if (clock_hand == entry)
    {
        clock_hand = list_next (entry);
        if (clock_hand == list_end (&frame_table))
            clock_hand = NULL;
    }
    list_remove (&entry->elem);
    lock_release (&frame_table_lock);
    palloc_free_page (entry->kpage);
    free (entry);  
}

bool
pick_thread_upage_to_swap (struct thread **t, void** upage)
{
    lock_acquire (&frame_table_lock);
    struct list_elem *e;

    if (clock_hand == NULL)
    {
        clock_hand = list_entry (list_begin (&frame_table), struct frame_table_entry, elem);
        if (&clock_hand->elem == list_end (&frame_table))
        {
            lock_release (&frame_table_lock);
            return false;
        }
    }
    printf("하하하하\n");
    //TODO: lock 관리 더 세밀하게.
    //TODO: 두바퀴를 돌아 circle queue처럼 보이게.
    //TODO: 시작 시점은 기록하고 그 시점부터 탐색 시작해야함.
    for (e = &clock_hand->elem; e != list_end (&frame_table);
         e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        
        uint32_t *pd = entry->thread->pagedir;
        bool is_accessed = pagedir_is_accessed (pd, entry->upage);
        if (!is_accessed)
        {
            *t = entry->thread;
            *upage = entry->upage;
            clock_hand = entry;
            lock_release (&frame_table_lock);
            printf("리턴은 햇냐?");
            return true;
        }
        pagedir_set_accessed (pd, entry->upage, false);
    }
    printf("첫번째\n");
    for (e = list_begin (&frame_table); e != list_end (&frame_table);
         e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        
        uint32_t *pd = entry->thread->pagedir;
        bool is_accessed = pagedir_is_accessed (pd, entry->upage);
        if (!is_accessed)
        {
            *t = entry->thread;
            *upage = entry->upage;
            clock_hand = entry;
            lock_release (&frame_table_lock);
            return true;
        }
        pagedir_set_accessed (pd, entry->upage, false);
    }
    printf("두번째\n");
    for (e = list_begin (&frame_table); e != &clock_hand->elem;
         e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        
        uint32_t *pd = entry->thread->pagedir;
        bool is_accessed = pagedir_is_accessed (pd, entry->upage);
        if (!is_accessed)
        {
            *t = entry->thread;
            *upage = entry->upage;
            clock_hand = entry;
            lock_release (&frame_table_lock);
            return true;
        }
        pagedir_set_accessed (pd, entry->upage, false);
    }
    printf("세번째\n");
    lock_release (&frame_table_lock);
    return false;
}
