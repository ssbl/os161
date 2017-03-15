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
sys_fork(struct trapframe *tf, int *retval)
{
    KASSERT(tf != NULL);

    int result;
    pid_t ppid;
    struct vnode *cwd;
    struct proc *proc, *newproc;
    struct trapframe *newtf;
    struct addrspace *curas, *newas;
    struct filetable *curft;
    struct filetable *newft;

    lock_acquire(proctable->pt_lock);
    spinlock_acquire(&curproc->p_lock);
    proc = curproc;
    VOP_INCREF(curproc->p_cwd);
    cwd = curproc->p_cwd;
    curas = proc->p_addrspace;
    ppid = curproc->p_pid;
    spinlock_release(&curproc->p_lock);

    newproc = proc_create("<child>");
    if (newproc == NULL) {
        lock_release(proctable->pt_lock);
        *retval = ENOMEM;
        return -1;
    }

    newproc->p_ppid = ppid; /* set the parent pid */
    newproc->p_cwd = cwd; 
    /* kprintf("newproc's id = %d\n", newproc->p_pid); */
    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        proc_destroy(newproc);
        lock_release(proctable->pt_lock);
        *retval = ENOMEM;
        return -1;
    }

    bzero(newtf, sizeof(*newtf)); /* zero out the trapframe */
    *newtf = *tf;                 /* copy using assignment */
    newtf->tf_v0 = 0;             /* return value of fork for child */

    /* copy address space */
    result = as_copy(curas, &newas); /* do the copy */
    if (result) {
        proc_destroy(newproc);
        lock_release(proctable->pt_lock);
        kfree(newtf);
        *retval = result;
        return -1;
    }
    newproc->p_addrspace = newas;

    /* copy file table */
    curft = proc->p_filetable;
    newproc->p_parent = proc;

    newft = filetable_copy(curft);
    if (newft == NULL) {
        proc_destroy(newproc);
        lock_release(proctable->pt_lock);
        kfree(newtf);
        *retval = ENOMEM;
        return -1;
    }
    filetable_destroy(newproc->p_filetable);
    newproc->p_filetable = newft;

    /* for (int i = 0; i <= newft->ft_maxfd; i++) {
     *     struct file_entry *fentry = filetable_get(newft, i);
     *     if (fentry == NULL) {
     *         kprintf("found null at %d\n", i);
     *     } else {
     *         if (fentry->f_lk != NULL) {
     *             kprintf("found %s at %d\n", fentry->f_name, i);
     *         }
     *     }
     * } */

    result = thread_fork(newproc->p_name, newproc,
                         enter_forked_process, newtf, 0);
    if (result) {
        proc_destroy(newproc);
        lock_release(proctable->pt_lock);
        kfree(newtf);
        *retval = result;
        return -1;
    }

    result = proctable_add(proctable, newproc);
    if (result) {
        kfree(newtf);
        proc_destroy(newproc);
        *retval = result;
        lock_release(proctable->pt_lock);
        return -1;
    }
    lock_release(proctable->pt_lock);

    /* kprintf("newproc's pid = %d\n", newproc->p_ppid);
     */
    return newproc->p_pid;
}
