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


ssize_t
sys_write(int fd, const_userptr_t user_buf, size_t buflen, int *retval)
{
    int result;
    char *kbuffer;
    struct uio uio;
    struct iovec iov;
    struct vnode *vnode;
    struct filetable *filetable;
    struct file_entry *fentry;

    /* check file descriptor and buffer pointer */
    if (fd < 0) {
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

    /* load data into buffer */
    result = copyin(user_buf, kbuffer, buflen);
    if (result) {
        kfree(kbuffer);
        *retval = result;
        return -1;
    }

    /* create a uio for vop_write */
    uio_kinit(&iov, &uio, kbuffer, buflen, 0, UIO_WRITE);

    /* check if fd has an entry in the filetable */
    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    /* get fd's entry from filetable */
    fentry = filetable_get(filetable, fd);
    if (fentry == NULL || fentry->f_mode == O_RDONLY) {
        kfree(kbuffer);
        *retval = EBADF;
        return -1;
    }

    lock_acquire(fentry->f_lk);
    vnode = fentry->f_node;
    uio.uio_offset = fentry->f_offset;

    /* do the write */
write:
    result = VOP_WRITE(vnode, &uio);
    if (result) {
        kfree(kbuffer);
        *retval = result;
        lock_release(fentry->f_lk);
        return -1;
    }

    /* make sure we wrote everything */
    if (buflen != (uio.uio_offset - fentry->f_offset)) {
        goto write;
    }

    /* update offset */
    fentry->f_offset += buflen;
    lock_release(fentry->f_lk);

    kfree(kbuffer);
    return buflen;
}
