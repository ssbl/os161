#include <types.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>

#define FILETABLE_INLINE
#include "filetable.h"

struct vnode *console_vnode = NULL;


struct file_entry *
file_entry_create(const char *name, int openflags, struct vnode *vnode)
{
	KASSERT(name != NULL);

	struct file_entry *fentry = kmalloc(sizeof(*fentry));
	if (fentry == NULL) {
		return NULL;
	}

	fentry->f_name = kstrdup(name);
	if (fentry->f_name == NULL) {
		kfree(fentry);
		return NULL;
	}

    fentry->f_node = vnode;

	fentry->f_lk = lock_create("file_entry lock");
	if (fentry->f_lk == NULL) {
		kfree(fentry->f_name);
		kfree(fentry);
		return NULL;
	}

	fentry->f_offset = 0;
	fentry->f_flags = openflags;
    fentry->f_mode = openflags & O_ACCMODE;

	return fentry;
}

void
file_entry_destroy(struct file_entry *fentry)
{
	KASSERT(fentry != NULL);

	kfree(fentry->f_name);
	vfs_close(fentry->f_node);
    lock_destroy(fentry->f_lk);
	kfree(fentry);
}

struct vnode * 
getconsolevnode()
{
	struct vnode* f_node;
	int result = vfs_open((char*)"con:", O_RDWR, 0664, &f_node);
    if (result) {
        return NULL;
    }
	return f_node;
}

struct filetable *
filetable_create()
{
    int ret;
    struct filetable *ft = NULL;
    struct file_entryarray *fdarray = NULL;

    ft = kmalloc(sizeof(*ft));
    if (ft == NULL) {
        return NULL;
    }

    fdarray = file_entryarray_create();
    if (fdarray == NULL) {
        kfree(ft);
        return NULL;
    }

    if (console_vnode == NULL) {
        console_vnode = getconsolevnode();
    }

    ret = file_entryarray_setsize(ft->ft_fdarray, 3);
    if (ret) {
        file_entryarray_destroy(ft->ft_fdarray);
        kfree(ft);
        return NULL;
    }

    stdin = file_entry_create("<stdin>", O_RDONLY, console_vnode);
    stdout = file_entry_create("<stdout>", O_WRONLY, console_vnode);
    stderr = file_entry_create("<stderr>", O_WRONLY, console_vnode);

    file_entryarray_set(ft->ft_fdarray, 0, stdin);
    file_entryarray_set(ft->ft_fdarray, 1, stdout);
    file_entryarray_set(ft->ft_fdarray, 2, stderr);

    ft->ft_maxfd = 2;
    ft->ft_openfds = 3;

    return ft;
}

struct file_entry *
filetable_get(struct filetable *ft, int fd)
{
    KASSERT(ft != NULL);
    KASSERT(fd >= 0 && fd < MAXFDS);
    KASSERT(fd <= ft->ft_maxfd);

    return file_entryarray_get(ft->ft_fdarray, fd);
}

int
filetable_set(struct filetable *ft, int fd, struct file_entry *fentry)
{
    KASSERT(ft != NULL);
    KASSERT(fd >= 0 && fd < MAXFDS);
    KASSERT(fentry != NULL);

    int ret;

    if (fd > ft->ft_maxfd) {
        ret = file_entryarray_setsize(ft->ft_fdarray, fd + 1);
        if (ret) {
            return ret;
        }
        ft->ft_maxfd = fd;
    }

    file_entryarray_set(ft->ft_fdarray, fd, fentry);
    ft->ft_openfds += 1;

    return 0;
}

int
filetable_remove(struct filetable *ft, int fd)
{
    KASSERT(ft != NULL);
    KASSERT(fd >= 0 && fd < MAXFDS);
    KASSERT(fd <= ft->ft_maxfd);

    int i;
    struct file_entry *fentry = NULL;

    fentry = file_entryarray_get(ft->ft_fdarray, fd);
    if (fentry == NULL) {
        return 0;               /* already removed, do nothing */
    }

    /* remove this entry */
    file_entry_destroy(fentry);
    fentry = NULL;

    /* update maxfd, openfds */
    if (fd == ft->ft_maxfd) {
        for (i = fd - 1; i >= 0; i--) {
            fentry = file_entryarray_get(ft->ft_fdarray, i);
            if (fentry != NULL) {
                ft->ft_maxfd = i;
                break;
            }
        }
    }
    ft->ft_openfds -= 1;

    return 0;
}


/*
 * Add an entry to the first available spot in the table.
 * Return the index of this new entry.
 */
int
filetable_add(struct filetable *ft, struct file_entry *fentry)
{
    KASSERT(ft != NULL);
    KASSERT(fentry != NULL);

    int i, ret, arr_size;
    struct file_entry *f;

    for (i = 0; i < ft->ft_maxfd; i++) {
        f = file_entryarray_get(ft->ft_fdarray, i);
        if (f == NULL) {
            file_entryarray_set(ft->ft_fdarray, i);
            break;
        }
    }

    if (i == ft->ft_maxfd) {
        i += 1;
        arr_size = ft->ft_maxfd + 1;
        ret = file_entryarray_setsize(ft->ft_fdarray, arr_size + 1);
        if (ret) {
            return -1;
        }
        file_entryarray_set(ft->ft_fdarray, i, fentry);
    }

    ft->ft_maxfd = i > ft->ft->maxfd ? i : ft->ft->ft_maxfd;
    ft->ft_openfds += 1;
    return i;
}
