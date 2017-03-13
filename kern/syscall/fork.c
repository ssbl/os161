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
    struct vnode *cwd;
    struct proc *proc, *newproc;
    struct trapframe *newtf;
    struct addrspace *curas, *newas;
    struct filetable *curft;
    struct filetable *newft;

    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    VOP_INCREF(curproc->p_cwd);
    cwd = curproc->p_cwd;
    spinlock_release(&curproc->p_lock);

    newproc = proc_create("<child>"); /* sets the pid */
    if (newproc == NULL) {
        return ENOMEM;
    }

    tf->tf_v0 = newproc->p_pid;     /* return value */
    tf->tf_a3 = 0;                  /* signal no error */
    newproc->p_ppid = sys_getpid(); /* set the parent pid */
    newproc->p_cwd = cwd; 

    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        proc_destroy(newproc);
        return ENOMEM;
    }

    bzero(newtf, sizeof(*newtf)); /* zero out the trapframe */
    *newtf = *tf;                 /* copy using assignment */
    newtf->tf_v0 = 0;             /* return value of fork for child */

    /* copy address space */
    curas = proc->p_addrspace;
    result = as_copy(curas, &newas); /* do the copy */
    if (result) {
        proc_destroy(newproc);
        kfree(newtf);
        return result;
    }
    newproc->p_addrspace = newas;

    /* copy file table */
    curft = proc->p_filetable;
    newproc->p_parent = proc;

    newft = filetable_copy(curft);
    if (newft == NULL) {
        proc_destroy(newproc);
        kfree(newtf);
        return ENOMEM;
    }
    filetable_destroy(newproc->p_filetable);
    newproc->p_filetable = newft;

    lock_acquire(proctable->pt_lock);
    result = proctable_add(proctable, newproc);
    if (result) {
        kfree(newtf);
        proc_destroy(newproc);
        lock_release(proctable->pt_lock);
        return result;
    }
    lock_release(proctable->pt_lock);

    result = thread_fork(newproc->p_name, newproc,
                         enter_forked_process, newtf, 0);
    if (result) {
        proc_destroy(newproc);
        kfree(newtf);
        return result;
    }
    ret = newproc->p_pid;

    return ret;
}
