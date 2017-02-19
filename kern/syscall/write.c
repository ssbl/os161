#include <types.h>
#include <syscall.h>

ssize_t
sys_write(int fd, const_userptr_t user_buf, size_t buflen)
{
    (void)fd;
    (void)user_buf;
    (void)buflen;

    /* We need a vnode and a uio to call the underlying
     * vop_write. We'll get the vnode from the filetable, but I'm not
     * sure about the uio. In any case, we want to read from user_buf;
     * there's a function copyin for this (in copyinout.h and
     * vm/copyinout.c). This data can be loaded into a uio struct
     * (uiomove in uio.h), which we can use.
     */

    return 0;
}
