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


char *arg, *argstart;

int
sys_execv(const_userptr_t program, char **args, int *retval)
{
    int result;
    unsigned proglen, arglen;
    struct vnode *v;
    struct addrspace *as;
    vaddr_t entrypoint, stackptr, argptr;
    void *kprogram = NULL;
    char **kargs, **kptr;
    char testbuf[1];
    /* char *arg; */
    /* char *kp_ptr = kprogram, *ka_ptr = kargs; */

    int argc = 0, i;

    if (program == NULL || args == NULL) {
        *retval = EFAULT;
		return -1;
    }

    if (arg == NULL) {
        arg = kmalloc(ARG_MAX * sizeof(char));
        if (arg == NULL) {
            *retval = ENOMEM;
            return -1;
        }
        argstart = arg;
    }

    kprogram = kmalloc(64);
    if (kprogram == NULL) {
        *retval = ENOMEM;
		return -1;
    }
    result = copyinstr(program, kprogram, 64, &proglen);
    if (result) {
        kfree(kprogram);
        *retval = result;
        return -1;
    }
    /* kprintf("got %s\n", (char *)kprogram); */

    kargs = kmalloc(201);
    if (kargs == NULL) {
        kfree(kprogram);
        *retval = result;
        return -1;
    }
    kptr = kargs;

    for (i = 0; ; i++) {
        result = copyin((const_userptr_t)((vaddr_t)args + i), testbuf, 1);
        if (result) {
            kfree(kprogram);
            kfree(kargs);
            *retval = result;
            return -1;
        }

        if (args[i] == NULL) {
            break;
        }

        result = copyinstr((const_userptr_t)(args[i]),
                           arg, ARG_MAX, &arglen);
        if (result) {
            kfree(kprogram);
            kfree(kargs);
            *retval = result;
            return -1;
        }

        /* kprintf("got arg %d: %s\n", i, arg); */
        if (kptr == NULL) {
            kptr = kmalloc(50);
            if (kptr == NULL) {
                kfree(kprogram);
                *retval = ENOMEM;
                return -1;
            }
        }

        kargs[i] = arg;
        if (kargs[i] == NULL) {
            kfree(kprogram);
            kfree(kargs[i]);       /* leak */
            *retval = ENOMEM;
			return -1;
        }
        arg = arg + arglen + 1;
        argc++;
        kptr += sizeof(char *);
    }
    arg = argstart;
    /* kprintf("argc = %d\n", argc - 1); */

    result = vfs_open(kprogram, O_RDONLY, 0, &v);
    if (result) {
        kfree(kprogram);
        kfree(kargs);
		*retval = result;
		return -1;
    }

    /* as = proc_getas();
     * if (as != NULL) {
     *     as_deactivate();
     *     as_destroy(as);
     * } */

    as = as_create();
    if (as == NULL) {
        vfs_close(v);
        kfree(kprogram);
        kfree(kargs);
        *retval = ENOMEM;
		return -1;
    }

    proc_setas(as);
    as_activate();

    result = load_elf(v, &entrypoint);
    if (result) {
        vfs_close(v);
        kfree(kprogram);
        kfree(kargs);
        *retval = result;
        return -1;
    }

    vfs_close(v);

    result = as_define_stack(as, &stackptr);
    if (result) {
        kfree(kprogram);
        kfree(kargs);
		*retval = result;
        return -1;
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
			*retval = result;
            return -1;
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
                kfree(kprogram);
                kfree(kargs);
				*retval = result;
            	return -1; 
            }
            argptr += 4;
        }
    }

    kfree(kprogram);
    kfree(kargs);
    enter_new_process(argc, (userptr_t)stackptr, NULL, stackptr, entrypoint);

    *retval = EINVAL;
	return -1;
}
