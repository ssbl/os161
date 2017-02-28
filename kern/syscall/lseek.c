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
    off_t cur_offset, new_offset;
    int result, max_fds;
    struct stat statbuf;
    struct file_entry *fentry;
    struct file_entryarray *filetable;

    /* if (fd > 0) {
     *     kprintf("fd = %d, pos = %d, whence = %d\n", fd, (int)pos, whence);
     *     return pos;
     * } */

    if (fd < 0) {
        kprintf("lseek: negative fd (%d)\n", fd);
        return EBADF;
    }
    if (pos < 0) {
        kprintf("lseek: negative pos (%d)\n", (int)pos);
        return EINVAL;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    max_fds = file_entryarray_num(filetable);
    if (fd >= max_fds) {
        kprintf("lseek: fd %d >= highest fd\n", fd);
        return EBADF;
    }

    fentry = file_entryarray_get(filetable, fd);
    if (fentry == NULL) {
        kprintf("lseek: couldn't find a file handle for fd %d\n", fd);
        return EBADF;
    }

    if (!VOP_ISSEEKABLE(fentry->f_node)) {
        kprintf("lseek: file not seekable\n");
        return ESPIPE;
    }

    lock_acquire(fentry->f_lk); /* this seems useless */

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
    kprintf("new_offset = %d\n", (int)new_offset);
    return new_offset;
}