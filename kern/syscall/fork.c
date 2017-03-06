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
	const char ch_proc[]="<child_process>";
    struct proc *newproc;
    struct trapframe *newtf;
    struct addrspace *curas, *newas;
    struct filetable *curft;
	//struct proctable *pt;	

    newproc = proc_create(ch_proc);
    if (newproc == NULL) {
        return ENOMEM;
    }

    /* newproc->p_pid = ?? */
    /* tf->tf_v0 = newproc->p_pid; */
    newproc->p_ppid = sys_getpid();

    /* copy trapframe */
    newtf = kmalloc(sizeof(*newtf));
    if (newtf == NULL) {
        proc_destroy(newproc);
        return ENOMEM;
    }

    *newtf = *tf;
    newtf->tf_v0 = 0;

    /* copy address space */
    curas = proc_getas();
    result = as_copy(curas, &newas);
    if (result) {
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
        proc_destroy(newproc);
        as_destroy(newas);
        kfree(newtf);
        return ENOMEM;
    }

	newproc->p_pid=proctable_add(proctable,newproc);
	if(newproc->p_pid==-1) {
        proc_destroy(newproc);
        as_destroy(newas);
        kfree(newtf);
        return -1;
    }

    return thread_fork(newproc->p_name, newproc,
                       enter_forked_process, newtf, 0);
}
