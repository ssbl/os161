#include <types.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <synch.h>
#include <proc.h>
#include <proctable.h>
#include <limits.h>

int
proctable_checkpid(pid_t pid)
{
    if (pid < PID_MIN || pid >= PID_MAX) {
        return -1;
    }
    return 0;
}

struct proc *
proctable_get(struct proctable *pt, int pid)
{
    KASSERT(pt != NULL);

    if (proctable_checkpid(pid) != 0) {
        return NULL;
    }

    return procarray_get(pt->pt_procs, pid); 
}

int
proctable_set(struct proctable *pt, int pid, struct proc *proc)
{
    KASSERT(pt != NULL);
    KASSERT(proc != NULL);

    int ret;

    if (pid < PID_MIN || pid >= PID_MAX) {
        return ESRCH;
    }

    if (pid > pt->pt_maxpid) {
        ret = procarray_setsize(pt->pt_procs, pid + 1);
        if (ret) {
            return ret;
        }
        pt->pt_maxpid = pid;
    }

    procarray_set(pt->pt_procs, pid, proc);
    pt->pt_numprocs += 1;

    return 0;
}

int
proctable_remove(struct proctable *pt, int pid)
{
    KASSERT(pt != NULL);

    int i;
    struct proc *proc = NULL;

    if (proctable_checkpid(pid) != 0) {
        return ESRCH;
    }

    proc = procarray_get(pt->pt_procs, pid);
    if (proc == NULL) {
        return 0;               /* already removed, do nothing */
    }

    /* remove this entry */
    proc_destroy(proc);
    proc = NULL;

    /* update maxpid, numprocs */
    if (pid == pt->pt_maxpid) {
        for (i = pid - 1; i >= 0; i--) {
            proc = procarray_get(pt->pt_procs, i);
            if (proc != NULL) {
                pt->pt_maxpid = i;
                break;
            }
        }
    }
    pt->pt_numprocs -= 1;

    return 0;
}


pid_t
proctable_add(struct proctable *pt, struct proc *proc)
{
    KASSERT(pt != NULL);
    KASSERT(proc != NULL);

    pid_t i;
	int ret, arr_size;
    struct proc *p;

    for (i = 1; i < pt->pt_maxpid; i++) {
        p = procarray_get(pt->pt_procs, i);
        if (p == NULL) {
            procarray_set(pt->pt_procs, i, proc);
            break;
        }
    }

    if (i == pt->pt_maxpid) {
        if (i == PID_MAX - 1) {  /* proctable full */
            return ENPROC;
        }

        i += 1;                 /* otherwise, add it to the end */
        arr_size = pt->pt_maxpid + 1;
        ret = procarray_setsize(pt->pt_procs, arr_size + 1);
        if (ret) {
            return ENOMEM;
        }
        procarray_set(pt->pt_procs, i, proc);
    }

    pt->pt_maxpid = i > pt->pt_maxpid ? i : pt->pt_maxpid;
    pt->pt_numprocs += 1;
    proc->p_pid = i;            /* set the pid */

    return 0;
}

void
proctable_destroy(struct proctable *pt)
{
    KASSERT(pt != NULL);

    int i;
    struct proc *proc;

    for (i = 0; i < pt->pt_maxpid; i++) {
        proc = proctable_get(pt, i);
        if (proc != NULL) {
            proctable_remove(pt, i);
        }
    }

    kfree(pt);
    pt = NULL;
}