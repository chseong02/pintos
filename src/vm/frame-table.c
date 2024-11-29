#include "vm/frame-table.h"
#include "threads/thread.h"
#include <list.h>

static struct list frame_table;

struct frame_table_entry
{
    tid_t tid;
    void *upage;
    void *kpage;
    bool use_flag;
    struct list_elem elem;
};
