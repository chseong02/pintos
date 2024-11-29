#include "vm/s-page-table.h"

struct s_page_table_entry 
{
	bool present;
	bool in_swap;
	bool has_loaded;
	bool writable;
	bool is_dirty;
	bool is_accessed;
	bool is_lazy;
	struct file* file;
	off_t file_ofs;
	uint32_t file_read_bytes;
	uint32_t file_zero_bytes;
	size_t swap_idx;
	void *upage;
	void *kpage;
	enum falloc_flags flags;
	struct hash_elem elem;
};

void
init_s_page_table (struct hash* s_page_table)
{
    hash_init (s_page_table, s_page_table_hash_func, s_page_table_hash_less_func, NULL);
}

unsigned
s_page_table_hash_func (const struct hash_elem *e, void *aux)
{
	struct s_page_table_entry *entry = hash_entry (e, struct s_page_table_entry, elem);
	return hash_bytes (&entry->upage, 32);
}

bool
s_page_table_hash_less_func (const struct hash_elem *a, 
    const struct hash_elem *b, void *aux)
{
	struct s_page_table_entry *_a = hash_entry(a, struct s_page_table_entry, elem);
	struct s_page_table_entry *_b = hash_entry(b, struct s_page_table_entry, elem);
	return _a->upage < _b->upage;
}

