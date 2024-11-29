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
