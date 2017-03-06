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

    newproc = proc_create("<child>"); /* sets the pid */
    if (newproc == NULL) {
        kprintf("fork: couldn't create process\n");
        return ENOMEM;
    }

    tf->tf_v0 = newproc->p_pid;
    newproc->p_ppid = sys_getpid();

    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        kprintf("fork: couldn't create trapframe\n");
        proc_destroy(newproc);
        return ENOMEM;
    }

    *newtf = *tf;
    newtf->tf_v0 = 0;

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
