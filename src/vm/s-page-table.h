#include <stdbool.h>
#include <hash.h>
#include "filesys/file.h"
#include "vm/frame-table.h"


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

void init_s_page_table (struct hash *s_page_table);
bool s_page_table_add (bool is_lazy, struct file *file, off_t file_ofs, 
	bool writable, void *upage, void *kpage, uint32_t file_read_bytes, 
	uint32_t file_zero_bytes, enum falloc_flags flags);
void s_page_table_delete_from_upage (void *upage);
