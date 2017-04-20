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
        /* kprintf("new brk = %u, old brk = %u\n", brk, as->as_heapbrk); */
        spl = splhigh();
        /* unsigned freed = 0, checked = 0; */
        for (i = 0; i < HEAPPAGES; i++) {
            if (as->as_heap[i] != NULL && as->as_heap[i]->lp_freed == 0) {
                if (as->as_heap[i]->lp_startaddr >= brk &&
                    as->as_heap[i]->lp_startaddr < as->as_heapbrk) {
                    coremap_free_kpages(as->as_heap[i]->lp_paddr);
                    as->as_heap[i]->lp_freed = 1;
                    /* freed++; */
                    /* kprintf("freeing %d\n", i); */
                } /* else */
                    /* kprintf("heap entry %d: {%u,%u,%d}\n", i,
                     *         as->as_heap[i]->lp_startaddr,
                     *         as->as_heap[i]->lp_paddr,
                     *         as->as_heap[i]->lp_freed); */
            /* } else if (as->as_heap[i]) {
             *     kprintf("invalid entry %d: %d\n", i, as->as_heap[i]->lp_freed); */
            }
        }

        /* if (freed*PAGE_SIZE != as->as_heapbrk - brk) {
         *     panic("sbrk assertion failed, amount = %ld, freed = %u, checked = %u, start = %u, diff = %u",
         *           amount, freed, checked, as->as_heapstart,
         *           as->as_heapbrk - as->as_heapstart);
         * } */

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
