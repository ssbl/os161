#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/wait.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <syscall.h>
#include <proctable.h>

void
sys__exit(int exitcode)
{
    int code = _MKWAIT_EXIT(exitcode);
    struct thread *cur = curthread;
    struct proc *proc, *parent;

    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    spinlock_release(&curproc->p_lock);

    parent = proctable_get(proctable, proc->p_ppid);
    if (parent == NULL) {
        goto exit;
    }

    proc->p_exitstatus = code;
    proc->p_exitcode = exitcode;

    V(proc->p_sem);
    P(parent->p_sem);
    /* exited, wait for signal from waitpid */
exit:
    proc_remthread(cur);
    proctable_remove(proctable, proc->p_pid);
    thread_exit();
}
