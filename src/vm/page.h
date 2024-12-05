#include <stdbool.h>

void* find_page_from_uaddr (void *uaddr);
bool is_writable_page (void *upage);
bool make_page_binded (void *upage);
bool make_more_binded_stack_space (void *uaddr);
bool is_valid_stack_address_heuristic (void *fault_addr, void *esp);
bool page_swap_out (void);