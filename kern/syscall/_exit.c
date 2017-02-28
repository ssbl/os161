#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <syscall.h>


void
sys__exit(int exitcode)
{
    int code = _MKWAIT_EXIT(exitcode);

    spinlock_acquire(&curproc->p_lock);
    curproc->exit_code = code;
    spinlock_release(&curproc->p_lock);

    thread_exit();
}
