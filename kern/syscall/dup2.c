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
    int result;
    struct file_entry *fentry_newfd = NULL;
    struct file_entry *fentry_oldfd = NULL;
    struct filetable *filetable = NULL;

    if (oldfd < 0 || newfd < 0) {
        return EBADF;
    }
    if (newfd == oldfd) {
        return newfd;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    fentry_oldfd = filetable_get(filetable, oldfd);
    if (fentry_oldfd == NULL) {
        return EBADF;
    }

    fentry_newfd = filetable_get(filetable, newfd);
    if (fentry_newfd != NULL) {
        filetable_remove(filetable, newfd);
    }

    result = filetable_set(filetable, newfd, fentry_oldfd);
    if (result) {
        return result;
    }

    return newfd;
}
