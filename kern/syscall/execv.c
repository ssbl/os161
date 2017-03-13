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
#include <mips/trapframe.h>
#include <addrspace.h>


int
sys_execv(const_userptr_t program, char **args)
{
    int result;
    unsigned proglen, arglen;
    struct vnode *v;
    struct addrspace *as;
    vaddr_t entrypoint, stackptr, argptr;
    void *kprogram = NULL;
    char **kargs;
    char arg[1024];
    /* char *kp_ptr = kprogram, *ka_ptr = kargs; */

    int argc = 0, i;

    if (program == NULL || args == NULL) {
        return EFAULT;
    }

    kprogram = kmalloc(64);
    if (kprogram == NULL) {
        return ENOMEM;
    }
    result = copyinstr(program, kprogram, 64, &proglen);
    if (result) {
        kfree(kprogram);
        return result;
    }
    /* kprintf("got %s\n", (char *)kprogram); */

    kargs = kmalloc(128);
    if (kargs == NULL) {
        kfree(kprogram);
        return result;
    }

    for (i = 0; ; i++) {
        if (args[i] == NULL) {
            break;
        }

        result = copyinstr((const_userptr_t)(args[i]),
                           arg, sizeof(arg), &arglen);
        if (result) {
            kfree(kprogram);
            kfree(kargs);
            return result;
        }

        /* kprintf("got arg %d: %s\n", i, arg); */
        kargs[i] = kstrdup(arg);
        if (kargs[i] == NULL) {
            kfree(kprogram);
            kfree(kargs[i]);       /* leak */
            return ENOMEM;
        }
        argc++;
    }
            
    /* kprintf("argc = %d\n", argc - 1); */

    result = vfs_open(kprogram, O_RDONLY, 0, &v);
    if (result) {
        return result;
    }

    as = proc_getas();
    if (as != NULL) {
        as_deactivate();
        as_destroy(as);
    }

    as = as_create();
    if (as == NULL) {
        vfs_close(v);
        kfree(kprogram);
        kfree(kargs);
        return ENOMEM;
    }

    proc_setas(as);
    as_activate();

    result = load_elf(v, &entrypoint);
    if (result) {
        vfs_close(v);
        kfree(kprogram);
        kfree(kargs);
        return result;
    }

    vfs_close(v);

    result = as_define_stack(as, &stackptr);
    if (result) {
        kfree(kprogram);
        kfree(kargs);
        return result;
    }

    argptr = stackptr;
    for (i = 0; i < argc; i++) {
        int len = 0;
        bool stringleft = true;
        while (stringleft) {
            for (int j = 0; j < 4; j++, len++, argptr--) {
                if (kargs[i][len] == 0) {
                    stringleft = false;
                }
            }
        }
    }
    stackptr = argptr - 4;

    for (i = argc - 1; i >= 0; i--) {
        int len = 0;
        bool stringleft = true;
        stackptr -= 4;
        result = copyout(&argptr, (userptr_t)stackptr, sizeof argptr);
        if (result) {
            return result;
        }
        while (stringleft) {
            char strtocopy[4] = {0};
            for (int j = 0; j < 4; j++, len++) {
                if (kargs[i][len] == 0) {
                    stringleft = false;
                    break;
                } else {
                    strtocopy[j] = kargs[i][len];
                }
                /* kprintf("%c", strtocopy[j]); */
            }
            /* kprintf("\n"); */
            result = copyout(strtocopy, (userptr_t)argptr, sizeof(uint32_t));
            if (result) {
                return result;
            }
            argptr += 4;
        }
    }

    kfree(kprogram);
    kfree(kargs);
    enter_new_process(argc, (userptr_t)stackptr, NULL, stackptr, entrypoint);

    return EINVAL;
}
