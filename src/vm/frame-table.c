#include "vm/frame-table.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include <list.h>

static struct list frame_table;

struct frame_table_entry
{
    tid_t tid;
    void *upage;
    void *kpage;
    bool use_flag;
    struct list_elem elem;
};

void
frame_table_init()
{
    list_init(&frame_table);
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
        // evict_policy();
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
    entry->upage = upage;
    entry->kpage = kpage;
    entry->use_flag = false;
    list_push_back (&frame_table, &entry->elem);
}

static struct frame_table_entry*
find_frame_table_entry_from_upage (void *upage)
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

static struct frame_table_entry*
find_frame_table_entry_from_frame (void *frame)
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

void
falloc_free_frame_from_upage (void *upage)
{
    struct frame_table_entry *entry = find_frame_table_entry_from_upage (upage);
    if (!entry)
        return;
    
    list_remove (&entry->elem);
    palloc_free_page (entry->kpage);
    free (entry);
}

void
falloc_free_frame_from_frame (void *frame)
{
    struct frame_table_entry *entry = find_frame_table_entry_from_frame (frame);
    if (!entry)
        return;
    
    list_remove (&entry->elem);
    palloc_free_page (entry->kpage);
    free (entry);  
}