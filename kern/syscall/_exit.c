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
    struct proc *proc;/* , *parent; */

    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    cur = curthread;
    spinlock_release(&curproc->p_lock);

    /* parent = proc->p_parent;
     * if (parent == NULL) {
     *     parent = cur->t_proc;
     *     proc_remthread(cur);
     *     V(proc->p_sem);         /\* just in case *\/
     *     goto exit;
     * } */
    /* lock_acquire(proc->p_lk); */

    proc->p_exitstatus = code;
    proc->p_exitcode = exitcode;

    /* if (proc->p_parent != NULL) */
    V(proc->p_sem);
    proc_remthread(cur);
    /* exited, wait for signal from waitpid */
    /* P(parent->p_sem); */
/* exit: */
    /* lock_acquire(proctable->pt_lock);
     * proctable_remove(proctable, proc->p_pid);
     * lock_release(proctable->pt_lock); */
    /* cv_broadcast(proc->p_cv, proc->p_lk);
     * lock_release(proc->p_lk); */
    thread_exit();
}
