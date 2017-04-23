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
    int f_refcount;
};
/* DECLARRAY(file_entry, FILETABLE_INLINE); */

/* DEFARRAY(file_entry, FILETABLE_INLINE); */

/*
 * A wrapper structure for file_entryarray.
 * Provides the guarantee: length(fdarray) >= maxfd + 1
 *
 * Should make the filesystem calls much simpler.
 */
struct filetable {
    /* struct file_entryarray *ft_fdarray; /\* array of file handles *\/ */
    struct file_entry **ft_fdarray;
    int ft_maxfd;               /* highest open (non-null) fd */
    int ft_openfds;             /* number of open fds */
    uint64_t *ft_bitset;
    /* struct lock *ft_lock; */
};

struct file_entry *file_entry_create(const char *name, int openflags,
                                     struct vnode *vnode);
void file_entry_destroy(struct file_entry *fentry);

struct vnode *getconsolevnode(void);
struct filetable *filetable_create(void);
struct filetable *filetable_copy(struct filetable *src);
struct file_entry *filetable_get(struct filetable *ft, int fd);
int filetable_set(struct filetable *ft, int fd, struct file_entry *fentry);
int filetable_remove(struct filetable *ft, int fd);
int filetable_add(struct filetable *ft, struct file_entry *fentry, int *retval);
int filetable_checkfd(struct filetable *ft, int fd);
void filetable_destroy(struct filetable *ft);

#endif  /* _FILETABLE_H_ */
