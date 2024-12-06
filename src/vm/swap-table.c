#include "swap-table.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "vm/s-page-table.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#define PG_IN_SECTOR (PGSIZE / BLOCK_SECTOR_SIZE)

struct swap_table
{
    struct block* swap_block;
    struct lock lock;
    struct bitmap *used_map;
};

static struct swap_table swap_table;

void
swap_table_init (void)
{
    swap_table.swap_block = block_get_role (BLOCK_SWAP);
    if (!swap_table.swap_block)
        PANIC ("Failed to get Swap Disk.");
    uint32_t sector_count = block_size (swap_table.swap_block);
    uint64_t page_count = ((uint64_t) sector_count) / PG_IN_SECTOR;
    lock_init (&(swap_table.lock));
    swap_table.used_map = bitmap_create (page_count);
    if (!swap_table.used_map)
        PANIC ("Failed to allocate swap disk bitmap.");
}

size_t
swap_out (void* frame)
{
    size_t swap_disk_page_idx;
    block_sector_t page_start_sector_idx;
    lock_acquire (&swap_table.lock);
    swap_disk_page_idx = bitmap_scan_and_flip (swap_table.used_map, 0, 1, false);
    lock_release (&swap_table.lock);
    if (swap_disk_page_idx != BITMAP_ERROR)
        page_start_sector_idx = swap_disk_page_idx * PG_IN_SECTOR;
    else
        return SWAP_ERROR;
    for (uint32_t i = 0; i< PG_IN_SECTOR; i++)
    {
        block_sector_t sector_idx = page_start_sector_idx + i;
        void* start_save_ptr = (uint32_t) frame + (uint32_t) (i * BLOCK_SECTOR_SIZE);
        block_write (swap_table.swap_block, sector_idx, start_save_ptr);
    }
    return swap_disk_page_idx;
}

bool
swap_in (size_t swap_idx, void* frame)
{
    block_sector_t page_start_sector_idx;
    lock_acquire (&swap_table.lock);
    if (!bitmap_test (swap_table.used_map, swap_idx))
    {
        printf("이런일이 일어난다고?\n");
        lock_release (&swap_table.lock);
        return false;
    }
    lock_release (&swap_table.lock);
    
    page_start_sector_idx = swap_idx * PG_IN_SECTOR;
    for (uint32_t i = 0; i< PG_IN_SECTOR; i++)
    {
        block_sector_t sector_idx = page_start_sector_idx + i;
        void* start_load_ptr = (uint32_t) frame + (uint32_t) (i * BLOCK_SECTOR_SIZE);
        block_read (swap_table.swap_block, sector_idx, start_load_ptr);
    }

    lock_acquire (&swap_table.lock);

    bitmap_set_multiple (swap_table.used_map, swap_idx, 
        1, false);
    lock_release (&swap_table.lock);
    return true;
}
