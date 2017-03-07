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
    /* vaddr_t stackptr; */

    newproc = proc_create("<child>"); /* sets the pid */
    if (newproc == NULL) {
        kprintf("fork: couldn't create process\n");
        return ENOMEM;
    }

    tf->tf_v0 = newproc->p_pid;
    tf->tf_a3 = 0;
    newproc->p_ppid = sys_getpid();

    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        kprintf("fork: couldn't create trapframe\n");
        proc_destroy(newproc);
        return ENOMEM;
    }

    bzero(newtf, sizeof(*newtf));
    *newtf = *tf;

    /* copy address space */
    curas = proc_getas();
    result = as_copy(curas, &newas);
    if (result) {
        kprintf("fork: couldn't copy addrspace\n");
        proc_destroy(newproc);
        kfree(newtf);
        return result;
    }
    newproc->p_addrspace = newas;

    /* define a user stack (ref. runprogram.c) */
    /* result = as_define_stack(newas, &stackptr);
     * if (result) {
     *     kprintf("fork: couldn't create user stack\n");
     *     proc_destroy(newproc);
     *     kfree(newtf);
     *     return result;
     * } */

    /* newtf->tf_sp = newas->as_stackpbase; */
    /* kprintf("%d\n", stackptr == newas->as_stackpbase); */

    kprintf("oldtf = %d\n", tf->tf_ra);
    /* copy file table */
    spinlock_acquire(&curproc->p_lock);
    curft = curproc->p_filetable;
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

    kprintf("created process with pid %d\n", (int)newproc->p_pid);

    return thread_fork(newproc->p_name, newproc,
                       enter_forked_process, newtf, 0);
}
