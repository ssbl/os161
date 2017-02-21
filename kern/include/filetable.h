#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include <array.h>
#include <synch.h>
#include <vnode.h>

#ifndef FILETABLE_INLINE
#define FILETABLE_INLINE INLINE
#endif

struct file_entry {
	char *f_name;
	struct vnode *f_node;
	struct lock *f_lk;
	off_t f_offset;
	int f_mode;
};
DECLARRAY(file_entry, FILETABLE_INLINE);

DEFARRAY(file_entry, FILETABLE_INLINE);

struct file_entry *file_entry_create(const char *name, int openflags,
									 mode_t mode);
void file_entry_destroy(struct file_entry *fentry);

struct file_entry *stdin(void);
struct file_entry *stdout(void);

#endif  /* _FILETABLE_H_ */
