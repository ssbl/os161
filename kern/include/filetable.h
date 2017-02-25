#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include <array.h>
#include <synch.h>
#include <vnode.h>

#define FILENAME_MAXLEN 64
#ifndef FILETABLE_INLINE
#define FILETABLE_INLINE INLINE
#endif

struct file_entry {
	char *f_name;
	struct vnode *f_node;
	struct lock *f_lk;
	off_t f_offset;
    mode_t f_mode;
    int f_flags;
};
DECLARRAY(file_entry, FILETABLE_INLINE);

DEFARRAY(file_entry, FILETABLE_INLINE);

struct file_entry *file_entry_create(const char *name, int openflags,
                                     struct vnode *vnode);
void file_entry_destroy(struct file_entry *fentry);

struct vnode *getconsolevnode(void);
struct file_entry *stdin_entry(struct vnode*);
struct file_entry *stdout_entry(struct vnode*);

#endif  /* _FILETABLE_H_ */
