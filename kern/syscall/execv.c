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
#include <limits.h>


/* char *arg, *argstart; */

int
sys_execv(const_userptr_t program, char **args, int *retval)
{
    int result;
    unsigned proglen, arglen;
    struct vnode *v;
    struct addrspace *as, *oldas;
    vaddr_t entrypoint, stackptr, argptr;
    void *kprogram = NULL;
    char **kargs, **kptr;
    char testbuf[1];
    char *arg, *argstart;
    /* char *kp_ptr = kprogram, *ka_ptr = kargs; */

    int argc = 0, i;

    if (program == NULL || args == NULL) {
        *retval = EFAULT;
		return -1;
    }

    arg = kmalloc(ARG_MAX * sizeof(char));
    if (arg == NULL) {
        *retval = ENOMEM;
        return -1;
    }
    argstart = arg;

    kprogram = kmalloc(20);
    if (kprogram == NULL) {
        kfree(arg);
        *retval = ENOMEM;
		return -1;
    }
    result = copyinstr(program, kprogram, 64, &proglen);
    if (result) {
        kfree(kprogram);
        kfree(arg);
        *retval = result;
        return -1;
    }
    /* kprintf("got %s\n", (char *)kprogram); */

    kargs = kmalloc(3850 * sizeof(char *));
    if (kargs == NULL) {
        kfree(arg);
        kfree(kprogram);
        *retval = result;
        return -1;
    }
    kptr = kargs;

    for (i = 0; ; i++) {
        result = copyin((const_userptr_t)((vaddr_t)args + i), testbuf, 1);
        if (result) {
            *retval = result;
            goto fail;
        }

        if (args[i] == NULL) {
            break;
        }

        result = copyinstr((const_userptr_t)(args[i]),
                           arg, ARG_MAX, &arglen);
        if (result) {
            *retval = result;
            goto fail;
        }
        /* kprintf("got arg %d: %s\n", i, arg); */
        /* if (kptr == NULL) {
         *     kptr = kmalloc(4);
         *     if (kptr == NULL) {
         *         *retval = ENOMEM;
         *         goto fail;
         *     }
         * } */

        kargs[i] = arg;
        if (kargs[i] == NULL) {
            *retval = ENOMEM;
            goto fail;
        }
        arg = arg + arglen;
        argc++;
        kptr += sizeof(char *);
    }
    arg = argstart;
    /* kprintf("argc = %d\n", argc - 1); */

    result = vfs_open(kprogram, O_RDONLY, 0, &v);
    if (result) {
        *retval = result;
        goto fail;
    }

    /* if (as != NULL) {
     *     as_deactivate();
     *     as_destroy(as);
     * } */

    as = as_create();
    if (as == NULL) {
        vfs_close(v);
        *retval = ENOMEM;
        goto fail;
    }

    oldas = proc_setas(as);
    as_activate();

    result = load_elf(v, &entrypoint);
    if (result) {
        vfs_close(v);
        proc_setas(oldas);
        *retval = result;
        goto fail_as;
    }

    vfs_close(v);

    result = as_define_stack(as, &stackptr);
    if (result) {
        proc_setas(oldas);
        *retval = result;
        goto fail_as;
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
            proc_setas(oldas);
            *retval = result;
            goto fail_as;
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
                proc_setas(oldas);
                *retval = result;
                goto fail_as;
            }
            argptr += 4;
        }
    }

    kfree(arg);
    kfree(kprogram);
    kfree(kargs);
    as_destroy(oldas);
    enter_new_process(argc, (userptr_t)stackptr, NULL, stackptr, entrypoint);

    *retval = EINVAL;
fail_as:
    as_destroy(as);
fail:
    kfree(arg);
    kfree(kprogram);
    kfree(kargs);
	return -1;
}
