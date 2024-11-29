#include "vm/s-page-table.h"
#include "threads/malloc.h"
#include "threads/thread.h"

struct s_page_table_entry 
{
	bool present;
	bool in_swap;
    bool is_lazy;
	bool has_loaded;

	bool writable;
	bool is_dirty;
	bool is_accessed;
	
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

void
init_s_page_table (struct hash* s_page_table)
{
    hash_init (s_page_table, s_page_table_hash_func, s_page_table_hash_less_func, NULL);
}

static unsigned
s_page_table_hash_func (const struct hash_elem *e, void *aux)
{
	struct s_page_table_entry *entry = hash_entry (e, struct s_page_table_entry, elem);
	return hash_bytes (&entry->upage, 32);
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

    // TODO: Check Is Exist upage 
    // and return result
    
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