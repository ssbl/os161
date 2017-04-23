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
#include <thread.h>
#include <addrspace.h>
#include <coremap.h>

void
sys__exit(int exitcode)
{
    int code = _MKWAIT_EXIT(exitcode);
    struct thread *cur;
    struct proc *proc;
    struct addrspace *as;
    pid_t pid, ppid;

    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    cur = curthread;
    pid = curproc->p_pid;
    ppid = curproc->p_ppid;
    spinlock_release(&curproc->p_lock);

    proc->p_exitstatus = code;
    proc->p_exitcode = exitcode;

    as = proc_getas();
    if (as != NULL) {
        as_destroy(as);
    }

    /* filetable_destroy(proc->p_filetable);
     * kfree(proc->p_name); */

    proc_remthread(cur);

    if (ppid != 1) {
        V(proc->p_sem);
    } else {
        lock_acquire(proctable->pt_lock);
        proctable_remove(proctable, pid);
        lock_release(proctable->pt_lock);
    }

    /* exited, signal waiting parent process */
    thread_exit();
}
