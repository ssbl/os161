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


int
sys_open(const_userptr_t filename, int flags, int *retval)
{
    int result;
    size_t buflen;
    char *kfilename;
    struct vnode *vnode;
    struct filetable *filetable;
    struct file_entry *fentry;

    if (filename == NULL) {
        *retval = EFAULT;
        return -1;
    }

    kfilename = kmalloc(FILENAME_MAXLEN * sizeof(char));
    if (kfilename == NULL) {
        *retval = ENOSPC;
        return -1;
    }

    result = copyinstr(filename, kfilename, FILENAME_MAXLEN, &buflen);
    if (result) {
        kfree(kfilename);
        *retval = result;
        return -1;
    }

    result = vfs_open(kfilename, flags, 0, &vnode);
    if (result) {
        kfree(kfilename);
        *retval = result;
        return -1;
    }

    fentry = file_entry_create(kfilename, flags, vnode);
    if (fentry == NULL) {
        kfree(kfilename);
        vfs_close(vnode);
        *retval = ENOSPC;
        return -1;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    result = filetable_add(filetable, fentry);

    kfree(kfilename);
    return result;
}
