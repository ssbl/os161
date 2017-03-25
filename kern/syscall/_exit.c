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

void
sys__exit(int exitcode)
{
    int code = _MKWAIT_EXIT(exitcode);
    struct thread *cur;
    struct proc *proc;

    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    cur = curthread;
    spinlock_release(&curproc->p_lock);

    proc->p_exitstatus = code;
    proc->p_exitcode = exitcode;

    filetable_destroy(proc->p_filetable);

    proc_remthread(cur);
    V(proc->p_sem);
    /* exited, signal waiting parent process */
    thread_exit();
}
