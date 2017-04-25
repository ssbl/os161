#include <types.h>
#include <lib.h>
#include <spl.h>
#include <mips/tlb.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <proc.h>
#include <syscall.h>
#include <addrspace.h>
#include <coremap.h>


int
sys_sbrk(intptr_t amount, int *retval)
{
    int i, spl;
    vaddr_t brk = 0;
    struct addrspace *as;

    if (amount % PAGE_SIZE != 0) {
        *retval = EINVAL;
        return -1;
    }

    as = proc_getas();
    if (as == NULL) {
        *retval = ENOMEM;
        return -1;
    }

    /* get current break value */
    brk = as->as_heapbrk;
    if (brk == 0) {             /* is it initialized? */
        *retval = ENOMEM;
        return -1;
    }

    /* test the change locally */
    brk += amount;
    if (brk < as->as_heapstart) {
        *retval = EINVAL;
        return -1;
    }
    if (brk >= as->as_heapmax) {
        *retval = amount < 0 ? EINVAL : ENOMEM;
        return -1;
    }

    /* free pages */
    if (brk < as->as_heapbrk) {
        for (i = 0; i < HEAPPAGES; i++) {
            if (as->as_heap[i] != NULL && as->as_heap[i]->lp_freed == 0) {
                if (as->as_heap[i]->lp_startaddr >= brk &&
                    as->as_heap[i]->lp_startaddr < as->as_heapbrk) {

                    spinlock_acquire(&coremap_lock);
                    coremap_free_kpages(as->as_heap[i]->lp_paddr);
                    spinlock_release(&coremap_lock);

                    as->as_heap[i]->lp_freed = 1;
                }
            }
        }

        spl = splhigh();
        for (i = 0; i < NUM_TLB; i++) {
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }
        splx(spl);
    }

    /* all OK, apply the change */
    *retval = as->as_heapbrk;
    as->as_heapbrk = brk;
    return 0;
}
