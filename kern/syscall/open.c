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
sys_open(const_userptr_t filename, int flags)
{
    int result;
    size_t buflen;
    char *kfilename;
    struct vnode *vnode;
    struct filetable *filetable;
    struct file_entry *fentry;

    if (filename == NULL) {
        return EFAULT;
    }

    kfilename = kmalloc(FILENAME_MAXLEN * sizeof(char));
    if (kfilename == NULL) {
        return ENOSPC;
    }

    result = copyinstr(filename, kfilename, FILENAME_MAXLEN, &buflen);
    if (result) {
        kfree(kfilename);
        return result;
    }

    result = vfs_open(kfilename, flags, 0, &vnode);
    if (result) {
        kfree(kfilename);
        return result;
    }

    fentry = file_entry_create(kfilename, flags, vnode);
    if (fentry == NULL) {
        kfree(kfilename);
        vfs_close(vnode);
        return ENOSPC;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    result = filetable_add(filetable, fentry);

    kfree(kfilename);
    return result;
}
