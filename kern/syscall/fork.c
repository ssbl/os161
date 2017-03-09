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

    int result;
    struct proc *newproc;
    struct trapframe *newtf;
    struct addrspace *curas, *newas;
    struct filetable *curft;
    vaddr_t stackptr;

    newproc = proc_create("<child>"); /* sets the pid */
    if (newproc == NULL) {
        kprintf("fork: couldn't create process\n");
        return ENOMEM;
    }

    tf->tf_v0 = newproc->p_pid; /* return value */
    tf->tf_a3 = 0;              /* signal no error */
    newproc->p_ppid = sys_getpid(); /* set the parent pid */

    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        kprintf("fork: couldn't create trapframe\n");
        proc_destroy(newproc);
        return ENOMEM;
    }

    bzero(newtf, sizeof(*newtf)); /* zero out the trapframe */
    *newtf = *tf;                 /* copy using assignment */
    newtf->tf_v0 = 0;             /* return value of fork for child */

    /* copy address space */
    curas = proc_getas();
    result = as_copy(curas, &newas); /* do the copy */
    if (result) {
        kprintf("fork: couldn't copy addrspace\n");
        proc_destroy(newproc);
        kfree(newtf);
        return result;
    }
    newproc->p_addrspace = newas;

    /* switch to child's address space */
    proc_setas(newas);
    as_activate();
    /* define a user stack in child's address space (ref. runprogram.c) */
    result = as_define_stack(newas, &stackptr);
    if (result) {
        kprintf("fork: couldn't create user stack\n");
        proc_destroy(newproc);
        kfree(newtf);
        return result;
    }
    /* switch back to our address space */
    proc_setas(curas);
    as_activate();

    newtf->tf_sp = stackptr;    /* set the stack pointer */
    /* kprintf("%d\n", stackptr == newas->as_stackpbase); */

    /* copy file table */
    spinlock_acquire(&curproc->p_lock);
    curft = curproc->p_filetable; /* get the paren't filetable */
    spinlock_release(&curproc->p_lock);

    filetable_destroy(newproc->p_filetable);
    newproc->p_filetable = filetable_copy(curft);
    if (newproc->p_filetable == NULL) {
        kprintf("fork: couldn't copy filetable\n");
        proc_destroy(newproc);
        as_destroy(newas);
        kfree(newtf);
        return ENOMEM;
    }

    /* kprintf("epc = %d\n", tf->tf_epc);
     * kprintf("epc = %d\n", newtf->tf_epc);     */
    kprintf("created process with pid %d\n", (int)newproc->p_pid);
    kprintf("numprocs = %d\n", proctable->pt_numprocs);
    for (int i = 1; i <= proctable->pt_numprocs; i++) {
        struct proc *proc = proctable_get(proctable, i);
        if (proc != NULL) {
            kprintf("found %d (%s)\n", i, proc->p_name);
        }
    }

    result = thread_fork(newproc->p_name, newproc,
                         enter_forked_process, newtf, 0);
    kprintf("forked %d\n", newproc->p_pid);
    if (result) {
        return -1;
    }
    return newproc->p_pid;
}
