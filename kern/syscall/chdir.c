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
sys_chdir(const char *pathname)
{
	struct vnode *vnode;
	int result;

	if(pathname==NULL) {
		return EFAULT;
	}
	
	result=vfs_lookup((char*)pathname,&vnode);
	if (result)
	{
		return -1;
	}

	result=vfs_setcurdir(vnode);
	if (result)
	{
		return -1;
	}

	spinlock_acquire(&curproc->p_lock);
    curproc->p_cwd=vnode;
    spinlock_release(&curproc->p_lock);

	return 0;

}
