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
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{
	int result;
	char *kbuffer;
	//struct vnode *vn;
	struct uio uio;
	struct iovec iov;


	if(buf == NULL)
	{
        *retval = EFAULT;
		return -1;
	}

	kbuffer = kmalloc(buflen * sizeof(char));		
    if (kbuffer == NULL) {
        *retval = ENOMEM;
        return -1;
    }

	uio_kinit(&iov, &uio, kbuffer, buflen, 0, UIO_READ);

	/*
	spinlock_acquire(&curproc->p_lock);
    vn = curproc->p_cwd;
    spinlock_release(&curproc->p_lock);
	*/
	result=vfs_getcwd(&uio);
	if (result)
	{
		kfree(kbuffer);
        *retval = result;
		return -1;
	}

	result = copyout(kbuffer, buf, buflen);
    if (result) 
	{
        kfree(kbuffer);
        *retval = result;
        return -1;
    }

    kfree(kbuffer);
	return buflen;
	
}
