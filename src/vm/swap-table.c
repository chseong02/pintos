#include "swap-table.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "vm/s-page-table.h"

struct swap_table
{
    struct lock lock;
    struct bitmap *used_map;
};

static struct swap_table swap_table;


