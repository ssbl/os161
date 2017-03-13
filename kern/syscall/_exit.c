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
    /* struct thread *cur = curthread; */
    struct proc *proc;/* , *parent; */

    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    spinlock_release(&curproc->p_lock);

    /* parent = proc->p_parent;
     * if (parent == NULL) {
     *     parent = cur->t_proc;
     *     proc_remthread(cur);
     *     V(proc->p_sem);         /\* just in case *\/
     *     goto exit;
     * } */

    proc->p_exitstatus = code;
    proc->p_exitcode = exitcode;

    /* proc_remthread(cur); */
    V(proc->p_sem);
    /* exited, wait for signal from waitpid */
    /* P(parent->p_sem); */
/* exit: */
    /* lock_acquire(proctable->pt_lock);
     * proctable_remove(proctable, proc->p_pid);
     * lock_release(proctable->pt_lock); */
    thread_exit();
}
