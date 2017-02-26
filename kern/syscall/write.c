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
sys_write(int fd, const_userptr_t user_buf, size_t buflen)
{
    int result, max_fds;
    char *kbuffer;
    struct uio uio;
    struct iovec iov;
    struct vnode *vnode;
    struct file_entryarray *filetable;
    struct file_entry *fentry;

    /* check file descriptor and buffer pointer */
    if (fd <= 0) {
        return EBADF;
    }
    if (user_buf == NULL) {
        return EFAULT;
    }

    /* initialize buffer */
    kbuffer = kmalloc(buflen * sizeof(char));
    if (kbuffer == NULL) {
        return ENOSPC;
    }

    /* load data into buffer */
    result = copyin(user_buf, kbuffer, buflen);
    if (result) {
        kfree(kbuffer);
        return result;
    }

    /* create a uio for vop_write */
    uio_kinit(&iov, &uio, kbuffer, buflen, 0, UIO_WRITE);

    /* check if fd has an entry in the filetable */
    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    max_fds = file_entryarray_num(filetable);
    if (fd >= max_fds) {
        kfree(kbuffer);
        return EBADF;
    }

    /* get fd's entry from filetable */
    fentry = file_entryarray_get(filetable, fd);
    if (fentry == NULL || fentry->f_mode == O_RDONLY) {
        kfree(kbuffer);
        return EBADF;
    }

    vnode = fentry->f_node;
    uio.uio_offset = fentry->f_offset;

    /* do the write */
write:
    result = VOP_WRITE(vnode, &uio);
    if (result) {
        kfree(kbuffer);
        return result;
    }

    /* make sure we wrote everything */
    if (buflen != (uio.uio_offset - fentry->f_offset)) {
        goto write;
    }

    /* update offset */
    fentry->f_offset += buflen;

    return buflen;
}
