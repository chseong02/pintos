#include "vm/frame-table.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include <list.h>
#include "userprog/pagedir.h"
#include "threads/interrupt.h"

struct frame_table_entry
{
    tid_t tid;
    struct thread* thread;
    void *upage;
    void *kpage;
    bool use_flag;
    struct list_elem elem;
};

static struct list frame_table;
static struct lock frame_table_lock;
static struct frame_table_entry *clock_hand;

static struct frame_table_entry*
find_frame_table_entry_from_upage (void *upage);
static struct frame_table_entry*
find_frame_table_entry_from_frame (void *frame);
static struct frame_table_entry*
find_frame_table_entry_from_frame_wo_lock (void *frame);
static struct frame_table_entry*
find_frame_table_entry_from_upage_wo_lock (void *upage);

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
        printf("혹시???");
        palloc_free_page (kpage);
        lock_release(&frame_table_lock);
        if (flags & FAL_ASSERT)
            PANIC ("NO Memory for Frame Table Entry!");
        return NULL;
    }

    entry->tid = thread_current()->tid;
    entry->thread = thread_current ();
    entry->upage = upage;
    entry->kpage = kpage;
    entry->use_flag = false;

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

void
falloc_free_frame_from_frame (void *frame)
{
    lock_acquire (&frame_table_lock);
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
    lock_release (&frame_table_lock);
}

bool
pick_thread_upage_to_swap (struct thread **t, void** upage)
{
    struct list_elem *e;

    if (clock_hand == NULL)
    {
        clock_hand = list_entry (list_begin (&frame_table), struct frame_table_entry, elem);
        if (&clock_hand->elem == list_end (&frame_table))
            return false;
    }
    for (e = &clock_hand->elem; e != list_end (&frame_table);
         e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        
        uint32_t *pd = entry->thread->pagedir;
        if(!pagedir_get_page(pd,entry->upage))
            continue;
        bool is_accessed = pagedir_is_accessed (pd, entry->upage);
        if (!is_accessed)
        {
            *t = entry->thread;
            *upage = entry->upage;
            clock_hand = entry;
            return true;
        }
        pagedir_set_accessed (pd, entry->upage, false);
    }
    for (e = list_begin (&frame_table); e != list_end (&frame_table);
         e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        
        uint32_t *pd = entry->thread->pagedir;
                if(!pagedir_get_page(pd,entry->upage))
            continue;
        bool is_accessed = pagedir_is_accessed (pd, entry->upage);
        if (!is_accessed)
        {
            *t = entry->thread;
            *upage = entry->upage;
            clock_hand = entry;
            return true;
        }
        pagedir_set_accessed (pd, entry->upage, false);
    }
    for (e = list_begin (&frame_table); e != &clock_hand->elem;
         e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        
        uint32_t *pd = entry->thread->pagedir;
                if(!pagedir_get_page(pd,entry->upage))
            continue;
        bool is_accessed = pagedir_is_accessed (pd, entry->upage);
        if (!is_accessed)
        {
            *t = entry->thread;
            *upage = entry->upage;
            clock_hand = entry;
            return true;
        }
        pagedir_set_accessed (pd, entry->upage, false);
    }
    return false;
}

void
free_frame_table_entry_about_current_thread ()
{
    lock_acquire(&frame_table_lock);
    struct list_elem *e;
    struct thread *t = thread_current ();
    
    for (e = list_begin (&frame_table); e != list_end (&frame_table);
         e = e)
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);   
        
        if(entry->thread == t)
        {
            e = list_remove (e);
            //falloc_free_frame_from_frame_wo_lock(entry->kpage);
            free (entry);
        }
        else{
e = list_next (e);
        }
    }
    lock_release(&frame_table_lock);
}

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

static struct frame_table_entry*
find_frame_table_entry_from_upage_wo_lock (void *upage)
{
    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); 
        e = list_next (e))
    {
        struct frame_table_entry *entry = 
            list_entry (e, struct frame_table_entry, elem);
        if (entry->upage == upage)
            return entry;
    }
    return NULL;
}