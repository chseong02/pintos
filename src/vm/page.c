#include "vm/page.h"
#include "vm/s-page-table.h"
#include "vm/frame-table.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/swap-table.h"
#include <bitmap.h>

void* find_page_from_uaddr (void *uaddr)
{
    void *upage = pg_round_down (uaddr);
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return NULL;
    if (!entry->present)
        return NULL;
    return entry->upage;
}

bool is_writable_page (void *upage)
{
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return false;
    return entry->writable;
}

bool is_valid_stack_address_heuristic (void *fault_addr, void *esp)
{
    return ((uint32_t) fault_addr >= (uint32_t) PHYS_BASE - (uint32_t) 0x00800000
        && (uint32_t) fault_addr >= (uint32_t) esp - 32);
}

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

bool make_page_binded (void *upage)
{
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry || !entry->present)
        return false;
    if (entry->in_swap)
    {
        void* frame = falloc_get_frame_w_upage (entry->flags, entry->upage);
        if (!frame)
            return false;
        swap_in (entry->swap_idx, frame);
        entry->kpage = frame;
        entry->in_swap = false;
        if (!install_page (upage, frame, entry->writable)) 
        {
            falloc_free_frame_from_frame (frame);
            return false; 
        }
        pagedir_set_dirty (thread_current()->pagedir,upage,true);
        return true;
    }
    if (!entry->has_loaded)
    {
        // NOT FOR FILE, just Lazy loading
        if (!entry->file)
        {
            //TODO: Maybe for stack?
            return false;
        }
        // File Lazy Loading
        uint8_t *kpage = falloc_get_frame_w_upage (entry->flags, entry->upage);
        if (!kpage)
            return false;
        struct file *file = entry->file; 
        off_t ofs = entry->file_ofs;
        off_t page_read_bytes = entry->file_read_bytes;
        off_t page_zero_bytes = entry->file_zero_bytes;
        off_t writable = entry->writable;
        file_lock_acquire();
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
    return false;
}

bool
page_swap_out (void)
{
    struct s_page_table_entry *entry;
    uint32_t *pd;
	struct thread *t;
	void* upage;
	if (!pick_thread_upage_to_swap (&t, &upage))
		return false;
	entry = find_s_page_table_entry_from_thread_upage (t, upage);
    if (!entry)
        return false;
    pd = t->pagedir;
    if (pagedir_is_dirty (pd, upage))
    {
        size_t swap_idx = swap_out (entry->kpage);
        if (swap_idx == SWAP_ERROR)
            return false;
        entry->swap_idx = swap_idx;
        entry->in_swap = true;
    }
    else
    {
        entry->has_loaded = false;
    }
        
    void *kpage = entry->kpage;
    entry->kpage = NULL;
    falloc_free_frame_from_frame_wo_lock (kpage);
    pagedir_clear_page (pd, entry->upage);
	return true;
}