#include <stdbool.h>

void init_swap_table (void);

void* swap_out (void* frame);
bool swap_in (void* swap_idx, void* frame);
