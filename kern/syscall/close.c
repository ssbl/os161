#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <lib.h>
#include <proc.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <filetable.h>


int
sys_close(int fd)
{
    int result, max_fds;

    struct file_entryarray *filetable;
    struct file_entry *fentry;

    if (fd < 0) {
        return EBADF;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    max_fds = file_entryarray_num(filetable);
    if (fd >= max_fds) {
        return EBADF;
    }

    fentry = file_entryarray_get(filetable, fd);
    if (fentry == NULL) {       /* already closed */
        return 0;
    }

    if (fentry->f_node != NULL) {
        file_entry_destroy(fentry);
    } else {
        kprintf("close: found null file handle for entry %d\n", fd);
    }

    if (max_fds == fd + 1) {
        result = file_entryarray_setsize(filetable, fd);
        if (result) {
            return -1;
        }
    }

    fentry = NULL;
    return 0;
}
