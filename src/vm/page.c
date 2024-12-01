#include "vm/page.h"
#include "vm/s-page-table.h"
#include "vm/frame-table.h"
#include "userprog/process.h"

void* find_page_from_uaddr (void *uaddr)
{
    void *upage = (uint32_t) uaddr & 0xFFFFF000;
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return NULL;
    if (entry->present)
        return entry->upage;
    return NULL;
}

bool make_page_binded (void *upage)
{
    struct s_page_table_entry *entry = find_s_page_table_entry_from_upage (upage);
    if (!entry || !entry->present)
        return false;
    if (entry->is_lazy && !entry->has_loaded)
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

        // We don't have to `file_open`, because It's not closed.
        file_seek (file, ofs);
        /* Load this page. */
        if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
            falloc_free_frame_from_frame (kpage);
            return false; 
        }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);
        /* Add the page to the process's address space. */
        if (!install_page (upage, kpage, writable)) 
        {
            falloc_free_frame_from_frame (kpage);
            return false; 
        }
        // TODO: change entry data about lazy loading, load status, etc.
        return true;
    }
    if (entry->in_swap)
    {
        //TODO: Impl Swap in
    }
    return false;
}