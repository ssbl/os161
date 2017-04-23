#ifndef _PROCTABLE_H_
#define _PROCTABLE_H_

#include <array.h>
#include <proctable.h>
#include <spinlock.h>


/* DECLARRAY_BYTYPE(procarray, struct proc, ARRAYINLINE);
 * DEFARRAY_BYTYPE(procarray, struct proc, ARRAYINLINE); */

/*
 * process table
 */
struct proctable {
    /* struct procarray *pt_procs; */
    struct proc **pt_procs;
    struct lock *pt_lock;
    int pt_numprocs;
	int pt_maxpid;
};

/* Create process table */
void proctable_create(void);

struct proc *proctable_get(struct proctable *pt, pid_t pid);
int proctable_set(struct proctable *pt, pid_t pid, struct proc *proc);
int proctable_remove(struct proctable *pt, pid_t pid);
int proctable_add(struct proctable *pt, struct proc *proc);
int proctable_checkpid(pid_t pid);
void proctable_destroy(struct proctable *pt);

#endif  /* _FILETABLE_H_ */
