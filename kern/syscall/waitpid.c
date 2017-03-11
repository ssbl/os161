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
#include <proctable.h>

pid_t
sys_waitpid (pid_t pid, userptr_t status,int options)
{
	int result;
    bool nostatus = false;
    struct proc *child, *parent;

	if (options != 0)
	{
		return EINVAL;
	}	

	if (status == NULL)
	{
        nostatus = true;
	}

    spinlock_acquire(&curproc->p_lock);
    parent = curproc;
    spinlock_release(&curproc->p_lock);

    child = proctable_get(proctable, pid);
    if (child == NULL) {
        return ESRCH;
    } else if (child->p_ppid != parent->p_pid) {
        return ECHILD;
    }

    P(child->p_sem);
    if (child->p_exitcode >= 0) {
        if (!nostatus) {
            result = copyout(&child->p_exitstatus, status, sizeof(int));
            if (result) {
                return result;
            }
        }
    }
    V(parent->p_sem);
    
    return pid;
}
