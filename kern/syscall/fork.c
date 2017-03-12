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
#include <mips/trapframe.h>
#include <addrspace.h>
#include <proctable.h>

pid_t
sys_fork(struct trapframe *tf)
{
    KASSERT(tf != NULL);

    int result, ret;
    struct proc *newproc;
    struct trapframe *newtf;
    struct addrspace *curas, *newas;
    struct filetable *curft;
    struct filetable *newft;

    lock_acquire(proctable->pt_lock);
    newproc = proc_create("<child>"); /* sets the pid */
    if (newproc == NULL) {
        lock_release(proctable->pt_lock);
        return ENOMEM;
    }

    tf->tf_v0 = newproc->p_pid; /* return value */
    tf->tf_a3 = 0;              /* signal no error */
    newproc->p_ppid = sys_getpid(); /* set the parent pid */

    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        proctable_remove(proctable, newproc->p_pid);
        lock_release(proctable->pt_lock);
        return ENOMEM;
    }

    bzero(newtf, sizeof(*newtf)); /* zero out the trapframe */
    *newtf = *tf;                 /* copy using assignment */
    newtf->tf_v0 = 0;             /* return value of fork for child */

    spinlock_acquire(&curproc->p_lock);
    curas = curproc->p_addrspace;
    spinlock_release(&curproc->p_lock);

    /* copy address space */
    result = as_copy(curas, &newas); /* do the copy */
    if (result) {
        proctable_remove(proctable, newproc->p_pid);
        kfree(newtf);
        lock_release(proctable->pt_lock);
        return result;
    }
    newproc->p_addrspace = newas;

    /* copy file table */
    spinlock_acquire(&curproc->p_lock);
    curft = curproc->p_filetable;
    newproc->p_parent = curproc;
    spinlock_release(&curproc->p_lock);

    newft = filetable_copy(curft);
    if (newft == NULL) {
        proctable_remove(proctable, newproc->p_pid);
        kfree(newtf);
        lock_release(proctable->pt_lock);
        return ENOMEM;
    }
    filetable_destroy(newproc->p_filetable);
    newproc->p_filetable = newft;

    result = thread_fork(newproc->p_name, newproc,
                         enter_forked_process, newtf, 0);
    if (result) {
        lock_release(proctable->pt_lock);
        return result;
    }
    ret = newproc->p_pid;
    lock_release(proctable->pt_lock);

    return ret;
}
