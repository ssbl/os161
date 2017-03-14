#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <lib.h>
#include <limits.h>
#include <proc.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <filetable.h>


int
sys_chdir(const_userptr_t pathname, int *retval)
{
	int result;
    unsigned len;
    void *kpath;
	struct vnode *vnode;

	if(pathname==NULL) {
        *retval = EFAULT;
		return -1;
	}

    kpath = kmalloc(NAME_MAX + 1);
    if (kpath == NULL) {
        *retval = ENOSPC;
        return -1;
    }

    result = copyinstr(pathname, kpath, NAME_MAX + 1, &len);
    if (result) {
        kfree(kpath);
        *retval = result;
        return -1;
    }

    result = vfs_open(kpath, O_RDONLY, 0, &vnode);
	if (result)
	{
        kfree(kpath);
        *retval = result;
		return -1;
	}

	result=vfs_setcurdir(vnode);
	if (result)
	{
        kfree(kpath);
        *retval = result;
		return -1;
	}

	spinlock_acquire(&curproc->p_lock);
    curproc->p_cwd=vnode;
    spinlock_release(&curproc->p_lock);

    kfree(kpath);
	return 0;
}
