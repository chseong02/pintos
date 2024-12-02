#include <stdbool.h>

void* find_page_from_uaddr (void* uaddr, bool write);
bool make_page_binded (void* upage);