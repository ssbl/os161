#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include <array.h>
#include <synch.h>

#ifndef FILETABLE_INLINE
#define FILETABLE_INLINE INLINE

struct file_entry {
    char *name;
    struct vnode f_node;
    struct lock f_lk;
    off_t f_offset;
    int f_mode;
};

DECLARRAY(file_entry, FILETABLE_INLINE);

struct file_entry *file_entry_create(const char *name);
void file_entry_destroy(struct file_entry *fentry);

#endif  /* _FILETABLE_H_ */
