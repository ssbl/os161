#include <types.h>
#include <copyinout.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <syscall.h>


pid_t
sys_getpid(void)
{
    pid_t ret;

    spinlock_acquire(&curproc->p_lock);
    ret = curproc->p_pid;
    spinlock_release(&curproc->p_lock);

    return ret;
}
