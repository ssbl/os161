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
sys_dup2(int oldfd, int newfd)
{
    int result, max_fds;
    struct file_entry *fentry_newfd = NULL;
    struct file_entry *fentry_oldfd = NULL;
    struct file_entryarray *filetable = NULL;

    if (oldfd < 0 || newfd < 0) {
        return EBADF;
    }
    if (newfd == oldfd) {
        return newfd;
    }

    spinlock_acquire(&curproc->p_filetable);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_filetable);

    max_fds = file_entryarray_num(filetable);
    if (oldfd >= max_fds || newfd >= MAXFDS) {
        return EBADF;           /* also check if they exceed 64 */
    }

    fentry_oldfd = file_entryarray_get(filetable, oldfd);
    if (fentry == NULL) {
        return EBADF;
    }

    if (newfd >= max_fds) {
        result = file_entryarray_setsize(filetable, newfd);
        if (result) {
            return ENOSPC;
        }
    } else {
        fentry_newfd = file_entryarray_get(filetable, newfd);
        if (fentry_newfd != NULL) { /* fd points to an open file, close it */
            file_entry_destroy(fentry_newfd);
            fentry_newfd = NULL;
        }
    }

    file_entryarray_set(filetable, newfd, fentry_oldfd);

    return newfd;
}
