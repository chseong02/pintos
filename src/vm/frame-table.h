#ifndef VM_FRAME_TABLE_H
#define VM_FRAME_TABLE_H

/* Flags for falloc_get_frame_w_upage */
enum falloc_flags
{
    FAL_ASSERT = 001,  /* Panic When fail to allocate frame */
    FAL_ZERO = 002,    /* Fill Frame with Zero */
    FAL_USER = 004,    /* Get Frame from User pool */
};

void frame_table_init (void);
void* falloc_get_frame_w_upage (enum falloc_flags flags, void *upage);
void falloc_free_frame_from_upage (void *upage);
void falloc_free_frame_from_frame (void *frame);
void* pick_upage_to_swap (void);

#endif /* vm/frame-table.h */