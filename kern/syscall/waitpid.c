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
sys_waitpid (pid_t pid, userptr_t status, int options, int *retval)
{
	int result;
    bool nostatus = false;
    struct proc *child, *parent;

	if (options != 0)
	{
		*retval = EINVAL;
		return -1;
	}	

	if (status == NULL)
	{
        nostatus = true;
	}

    spinlock_acquire(&curproc->p_lock);
    parent = curproc;
    spinlock_release(&curproc->p_lock);

    lock_acquire(proctable->pt_lock);
    child = proctable_get(proctable, pid);
    if (child == NULL) {
        lock_release(proctable->pt_lock);
        *retval = ESRCH;
		return -1;
    } else if (child->p_ppid != parent->p_pid) {
        lock_release(proctable->pt_lock);
        *retval = ECHILD;
		return -1;
    }

    lock_release(proctable->pt_lock);
    P(child->p_sem);
    if (child->p_exitcode >= 0) {
        if (!nostatus) {
            result = copyout(&child->p_exitstatus, status, sizeof(int));
            if (result) {
                /* V(parent->p_sem); */
                *retval = result;
            }
        }
    }
    /* V(parent->p_sem); */
    lock_acquire(proctable->pt_lock);
    kprintf("child: %d\n", child->p_pid);
    proctable_remove(proctable, child->p_pid);
    lock_release(proctable->pt_lock);
    if (*retval) {
        return -1;
    }

    return pid;
}
