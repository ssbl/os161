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

pid_t
waitpid (pid_t pid, int* status,int options)
{
	int result;
	
	if (options != 0)
	{
		return EINVAL;
	}	

	/*if (status == NULL)
	{
		return EFAULT;
	}*/

	


}
