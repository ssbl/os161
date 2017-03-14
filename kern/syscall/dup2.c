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
sys_dup2(int oldfd, int newfd, int *retval)
{
    int result;
    struct file_entry *fentry_newfd = NULL;
    struct file_entry *fentry_oldfd = NULL;
    struct filetable *filetable = NULL;

    if (oldfd < 0 || newfd < 0) {
        *retval = EBADF;
        return -1;
    }
    if (newfd == oldfd) {
        return newfd;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    KASSERT(filetable != NULL);

    fentry_oldfd = filetable_get(filetable, oldfd);
    if (fentry_oldfd == NULL) {
        *retval = EBADF;
        return -1;
    }

    lock_acquire(fentry_oldfd->f_lk);
    fentry_newfd = filetable_get(filetable, newfd);
    if (fentry_newfd != NULL) {
        filetable_remove(filetable, newfd);
    }

    result = filetable_set(filetable, newfd, fentry_oldfd);
    lock_release(fentry_oldfd->f_lk);
    if (result) {
        *retval = result;
        return -1;
    }

    return newfd;
}
