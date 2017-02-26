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
    int i, result, max_fds;
    size_t buflen;
    unsigned index_ret;
    char *kfilename;
    struct vnode *vnode;
    struct file_entryarray *filetable;
    struct file_entry *fentry;

    if (filename == NULL) {
        kprintf("open: null userptr\n");
        return EFAULT;
    }

    kfilename = kmalloc(FILENAME_MAXLEN * sizeof(char));
    if (kfilename == NULL) {
        kprintf("open: kmalloc failed\n");
        return ENOSPC;
    }

    result = copyinstr(filename, kfilename, FILENAME_MAXLEN, &buflen);
    if (result) {
        kfree(kfilename);
        kprintf("open: copyinstr failed\n");
        return result;
    }
    
    result = vfs_open(kfilename, flags, 0664, &vnode);
    if (result) {
        kfree(kfilename);
        kprintf("open: error in vfs_open\n");
        return result;
    }

    fentry = file_entry_create(kfilename, flags, vnode);
    if (fentry == NULL) {
        kfree(kfilename);
        vfs_close(vnode);
        kprintf("open: couldn't create entry\n");
        return ENOSPC;
    }

    spinlock_acquire(&curproc->p_lock);
    filetable = curproc->p_filetable;
    spinlock_release(&curproc->p_lock);

    max_fds = file_entryarray_num(filetable);
    for (i = 0; i < max_fds; i++) {
        if (file_entryarray_get(filetable, i) == NULL) {
            break;
        }
    }
    if (i == max_fds) {
        result = file_entryarray_add(filetable, fentry, &index_ret);
        if (result) {
            kprintf("open: couldn't add entry to filetable\n");
            return EMFILE;
        }
    } else {
        file_entryarray_set(filetable, i, fentry);
    }

    kfree(kfilename);
    return i;
}
