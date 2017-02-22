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
	int result;

	/*
	 * We need a vnode and a uio to call the underlying
	 * vop_write. We'll get the vnode from the filetable, but I'm not
	 * sure about the uio. In any case, we want to read from user_buf;
	 * there's a function copyin for this (in copyinout.h and
	 * vm/copyinout.c). This data can be loaded into a uio struct
	 * (uiomove in uio.h), which we can use.
	 */

	char *kbuffer;
	struct uio uio;
	struct iovec iov;
	struct vnode *vnode;
	struct file_entry *fentry;

	/* check file descriptor and buffer pointer */
	if (fd <= 0) {
		return EBADF;
	}
	if (user_buf == NULL) {
		return EFAULT;
	}

	/* initialize buffer */
	kbuffer = kmalloc(buflen * sizeof(char) + 1);
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

	/* get vnode from filetable */
	fentry = file_entryarray_get(curproc->p_filetable, fd);
	vnode = fentry->f_node;
	if (fentry->f_mode == O_RDONLY) {
		kfree(kbuffer);
		return EBADF;
	}

	/* do the write */
	result = VOP_WRITE(vnode, &uio);
	if (result) {
		kfree(kbuffer);
        kprintf("write error: %s\n", strerror(result));
		return result;
	}

	return buflen;
}
