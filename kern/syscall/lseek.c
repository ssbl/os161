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
sys_lseek(int fd, off_t pos, int whence)
{
    int result;
    off_t cur_offset, new_offset;
    struct stat statbuf;
    struct file_entry *fentry;
    struct filetable *filetable;

    /* if (fd > 0) {
     *     kprintf("fd = %d, pos = %d, whence = %d\n", fd, (int)pos, whence);
     *     return pos;
     * } */

    if (fd < 0) {
        return EBADF;
    }
    if (pos < 0) {
        return EINVAL;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    fentry = filetable_get(filetable, fd);
    if (fentry == NULL) {
        return EBADF;
    }

    if (!VOP_ISSEEKABLE(fentry->f_node)) {
        return ESPIPE;
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
            lock_release(fentry->f_lk);
            return EBADF;
        }

        new_offset = statbuf.st_size + pos;
        break;
    default:
        lock_release(fentry->f_lk);
        return EINVAL;
    }
    fentry->f_offset = new_offset;
    lock_release(fentry->f_lk);

    return new_offset;
}
