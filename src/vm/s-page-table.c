#include "vm/s-page-table.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/swap-table.h"

static unsigned s_page_table_hash_func (const struct hash_elem *, void *);
static bool s_page_table_hash_less_func (const struct hash_elem *, 
    const struct hash_elem *, void *);
static void s_page_table_hash_free_action_func (struct hash_elem *e, void *aux);

void
init_s_page_table (void)
{
    struct thread *thread = thread_current ();
    hash_init (&thread->s_page_table, s_page_table_hash_func, 
        s_page_table_hash_less_func, NULL);
}

static unsigned
s_page_table_hash_func (const struct hash_elem *e, void *aux)
{
	struct s_page_table_entry *entry = hash_entry (e, struct s_page_table_entry, elem);
	return hash_bytes (&entry->upage, 4);
}

static bool
s_page_table_hash_less_func (const struct hash_elem *a, 
    const struct hash_elem *b, void *aux)
{
	struct s_page_table_entry *_a = hash_entry(a, struct s_page_table_entry, elem);
	struct s_page_table_entry *_b = hash_entry(b, struct s_page_table_entry, elem);
	return _a->upage < _b->upage;
}

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
    entry->present = true;
    entry->in_swap = false;
    entry->is_lazy = is_lazy;
    entry->has_loaded = !is_lazy;
	
	entry->writable = writable;
	entry->is_dirty = false;
	entry->is_accessed = false;
    
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

bool
s_page_table_binded_add (void *upage, void *kpage, bool writable, enum falloc_flags flags)
{
    return s_page_table_add (false, NULL, 0, writable, upage, kpage, 0, 0, flags);
}

bool
s_page_table_file_add (void *upage, bool writable, struct file *file, 
	off_t file_ofs, uint32_t file_read_bytes, uint32_t file_zero_bytes, 
	enum falloc_flags flags)
{
    return s_page_table_add (true, file, file_ofs, writable, upage, NULL, 
        file_read_bytes, file_zero_bytes, flags);
}

bool
s_page_table_lazy_add (void *upage, bool writable, enum falloc_flags flags)
{
    return s_page_table_add (true, NULL, 0, writable, upage, NULL, 0, 0, flags);
}

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

void
s_page_table_delete_from_upage (void *upage)
{
    struct s_page_table_entry *entry;
    struct thread *thread = thread_current ();

    entry = find_s_page_table_entry_from_upage (upage);
    if (!entry)
        return;

    entry->present = false;
	hash_delete (&thread->s_page_table, &entry->elem);
	free (entry);
}

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

static void
s_page_table_hash_free_action_func (struct hash_elem *e, void *aux)
{
    struct s_page_table_entry *entry = hash_entry (e, struct s_page_table_entry, elem);
    if(entry->in_swap)
	{
		delete_swap_entry(entry->swap_idx);
	}
	entry->present = false;
	
	hash_delete (&(thread_current())->s_page_table, &entry->elem);
	free (entry);
}

void
free_s_page_table (void)
{
    struct thread *thread = thread_current ();
    hash_clear (&thread->s_page_table,s_page_table_hash_free_action_func);
}