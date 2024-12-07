#include <stdbool.h>
#include <hash.h>
#include "filesys/file.h"
#include "vm/frame-table.h"
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


void init_s_page_table (void);
bool s_page_table_add (bool is_lazy, struct file *file, off_t file_ofs, 
	bool writable, void *upage, void *kpage, uint32_t file_read_bytes, 
	uint32_t file_zero_bytes, enum falloc_flags flags);
bool s_page_table_binded_add (void *upage, void *kpage, bool writable, 
	enum falloc_flags flags);
bool s_page_table_file_add (void *upage, bool writable, struct file *file, 
	off_t file_ofs, uint32_t file_read_bytes, uint32_t file_zero_bytes, 
	enum falloc_flags flags);
bool s_page_table_lazy_add (void *upage, bool writable, enum falloc_flags flags);
void s_page_table_delete_from_upage (void *upage);

struct s_page_table_entry* find_s_page_table_entry_from_upage (void *);
struct s_page_table_entry* find_s_page_table_entry_from_thread_upage 
	(struct thread *t, void* upage);

void free_s_page_table (void);