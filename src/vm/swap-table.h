#include <stdbool.h>

void init_swap_table (void);

size_t swap_out (void* frame);
bool swap_in (void* swap_idx, void* frame);
