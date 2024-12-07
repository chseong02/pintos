#include <stdbool.h>
#include <stddef.h>

#define SWAP_ERROR BITMAP_ERROR


void swap_table_init (void);

size_t swap_out (void* frame);
bool swap_in (size_t swap_idx, void* frame);
bool delete_swap_entry (size_t swap_idx);
