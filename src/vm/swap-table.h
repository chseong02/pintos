#include <stdbool.h>

#define SWAP_ERROR BITMAP_ERROR

void swap_table_init (void);

size_t swap_out (void* frame);
bool swap_in (size_t swap_idx, void* frame);
