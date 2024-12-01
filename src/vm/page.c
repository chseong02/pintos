#include "vm/page.h"
#include "vm/s-page-table.h"

void* find_page_from_uvaddr (void *uaddr)
{
    // Not Implementated yet.
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
    // Not Implementated yet.
    return false;
}