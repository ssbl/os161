#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <proc.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <filetable.h>


off_t
sys_lseek(int fd, off_t pos, int whence, int *retval)
{
    int result;
    off_t cur_offset, new_offset;
    struct stat statbuf;
    struct file_entry *fentry;
    struct filetable *filetable;

    if (fd < 0) {
        *retval = EBADF;
        return -1;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    fentry = filetable_get(filetable, fd);
    if (fentry == NULL) {
        *retval = EBADF;
        return -1;
    }

    if (!VOP_ISSEEKABLE(fentry->f_node)) {
        *retval = ESPIPE;
        return -1;
    }

    lock_acquire(fentry->f_lk);
    cur_offset = fentry->f_offset;
    switch(whence) {
    case SEEK_SET:
        new_offset = pos;
        break;
    case SEEK_CUR:
        new_offset = cur_offset + pos;
        break;
    case SEEK_END:
        result = VOP_STAT(fentry->f_node, &statbuf);
        if (result) {
            *retval = EBADF;
            lock_release(fentry->f_lk);
            return -1;
        }

        new_offset = statbuf.st_size + pos;
        break;
    default:
        *retval = EINVAL;
        lock_release(fentry->f_lk);
        return -1;
    }
    if (new_offset < 0) {
        *retval = EINVAL;
        lock_release(fentry->f_lk);
        return -1;
    }
    fentry->f_offset = new_offset;
    lock_release(fentry->f_lk);

    return new_offset;
}
