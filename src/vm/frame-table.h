
void frame_table_init (void);
void* falloc_get_frame_w_upage (enum falloc_flags, void* upage);
void* falloc_free_frame (void *frame);
struct frame_table_entry* find_frame_table_entry_from_upage (void *upage);
