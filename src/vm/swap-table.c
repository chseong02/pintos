#include "swap-table.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "vm/s-page-table.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

struct swap_table
{
    struct block* swap_block;
    struct lock lock;
    struct bitmap *used_map;
};

static struct swap_table swap_table;

void
init_swap_table (void)
{
    swap_table.swap_block = block_get_role (BLOCK_SWAP);
    if (!swap_table.swap_block)
        PANIC ("Failed to get Swap Disk.");
    uint32_t sector_count = block_size (swap_table.swap_block);
    uint64_t page_count = ((uint64_t) sector_count) * BLOCK_SECTOR_SIZE / PGSIZE;
    lock_init (&(swap_table.lock));
    swap_table.used_map = bitmap_create (page_count);
    if (!swap_table.used_map)
        PANIC ("Failed to allocate swap disk bitmap.");
}


