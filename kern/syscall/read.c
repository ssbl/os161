#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <limits.h>
#include <lib.h>
#include <proc.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <filetable.h>


ssize_t
sys_read(int fd, userptr_t user_buf, size_t buflen, int *retval)
{
    int result;
    char *kbuffer;
    struct uio uio;
    struct iovec iov;
    struct vnode *vnode;
    struct filetable *filetable;
    struct file_entry *fentry;

    /* check file descriptor and buffer pointer */
    if (fd < 0 || fd >= OPEN_MAX) {
        *retval = EBADF;
        return -1;
    }
    if (user_buf == NULL) {
        *retval = EFAULT;
        return -1;
    }

    /* initialize buffer */
    kbuffer = kmalloc(buflen * sizeof(char));
    if (kbuffer == NULL) {
        *retval = ENOSPC;
        return -1;
    }

    /* create a uio for vop_read */
    uio_kinit(&iov, &uio, kbuffer, buflen, 0, UIO_READ);

    /* lock current process to get its filetable */
    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    /* get fd's entry from filetable */
    fentry = filetable_get(filetable, fd);
    if (fentry == NULL || fentry->f_mode == O_WRONLY) {
        kfree(kbuffer);
        *retval = EBADF;
        return -1;
    }

    lock_acquire(fentry->f_lk);
    fentry->f_refcount += 1;
    vnode = fentry->f_node;
    uio.uio_offset = fentry->f_offset;

    /* do the read */
    result = VOP_READ(vnode, &uio);
    if (result) {
        kfree(kbuffer);
        lock_release(fentry->f_lk);
        return result;
    }

    /* send read data into user's buffer */
    result = copyout(kbuffer, user_buf, buflen);
    if (result) {
        kfree(kbuffer);
        lock_release(fentry->f_lk);
        *retval = result;
        return -1;
    }

    /* store return value, update offset */
    result = uio.uio_offset - fentry->f_offset;
    fentry->f_offset += uio.uio_offset;
    fentry->f_refcount -= 1;
    lock_release(fentry->f_lk);

    return result;
}
