#include <types.h>
#include <lib.h>
#include <spl.h>
#include <kern/errno.h>
#include <current.h>
#include <proc.h>
#include <vm.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <coremap.h>

void
vm_bootstrap(void)
{
    /* numpages = (int)coremap_alloc_npages(10); */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    unsigned i = 0;
    struct addrspace *as;

    int spl, pageno;
    uint32_t ehi, elo;
    vaddr_t vtop, vbase;
    paddr_t paddr = 0;

    faultaddress &= PAGE_FRAME;

    if (curproc == NULL) {
        return EFAULT;
    }

    as = proc_getas();
    if (as == NULL) {
        return EFAULT;
    }

    KASSERT(as->as_regions != NULL);

    if (as->as_heapmax != 0 && faultaddress > as->as_heapbrk) {
        int nullpage = 0;
        for (i = 0; i < LPAGES; i++) {
            if (as->as_stack[i] != NULL) {
                vbase = as->as_stack[i]->lp_startaddr;

                if (faultaddress == vbase) {
                    paddr = as->as_stack[i]->lp_paddr;
                    goto skip_regions;
                }
            } else {
                nullpage = i;
            }
        }

        KASSERT(i == LPAGES);

        as->as_heapmax = faultaddress;

        spinlock_acquire(&coremap_lock);
        paddr = coremap_alloc_page();
        spinlock_release(&coremap_lock);

        as->as_stack[nullpage] = kmalloc(sizeof(struct lpage));
        if (as->as_stack[nullpage] == NULL) {
            return ENOMEM;
        }

        as->as_stack[nullpage]->lp_paddr = paddr;
        as->as_stack[nullpage]->lp_startaddr = faultaddress;
        bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
        goto skip_regions;
    } else if (faultaddress >= as->as_heapstart &&
               faultaddress < as->as_heapbrk) {
        int nullpage = 0;
        for (i = 0; i < HEAPPAGES; i++) {
            if (as->as_heap[i] != NULL && as->as_heap[i]->lp_freed == 0) {
                vbase = as->as_heap[i]->lp_startaddr;

                if (faultaddress == vbase) {
                    paddr = as->as_heap[i]->lp_paddr;
                    goto skip_regions;
                }
            } else {
                nullpage = i;
            }
        }

        KASSERT(i == HEAPPAGES);

        spinlock_acquire(&coremap_lock);
        paddr = coremap_alloc_page();
        spinlock_release(&coremap_lock);

        KASSERT(paddr != 0);

        if (as->as_heap[nullpage] == NULL) {
            as->as_heap[nullpage] = kmalloc(sizeof(struct lpage));
            if (as->as_heap[nullpage] == NULL) {
                return ENOMEM;
            }
        }

        as->as_heap[nullpage]->lp_freed = 0;
        as->as_heap[nullpage]->lp_paddr = paddr;
        as->as_heap[nullpage]->lp_startaddr = faultaddress;
        bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
        goto skip_regions;
    }

    for (i = 0; i < as->as_numregions; i++) {
        struct region *region = as->as_regions[i];

        if (region == NULL) {
            continue;
        }

        vbase = region->r_startaddr;
        vtop = vbase + PAGE_SIZE * region->r_numpages;

        if (faultaddress >= vbase && faultaddress < vtop) {
            pageno = (faultaddress - vbase) / PAGE_SIZE;
            if (region->r_pages[pageno] == NULL) {
                /* OOM page, need to allocate */

                spinlock_acquire(&coremap_lock);
                paddr = coremap_alloc_page();
                KASSERT(paddr != 0);
                region->r_pages[pageno] = coremap[paddr / PAGE_SIZE]->cme_page;
                spinlock_release(&coremap_lock);

                bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
            } else {
                paddr = region->r_pages[pageno]->vp_paddr;
            }
        }
    }

skip_regions:
    if (paddr == 0) {
        return EFAULT;
    }

    switch (faulttype) {
    case VM_FAULT_READONLY:
        if ((as->as_regions[i]->r_permissions & 2) == 0) {
            kprintf("RDONLY\n");
            return EFAULT;
        }
        break;
    case VM_FAULT_READ:
        /* if ((as->as_regions[i]->r_permissions & 4) == 0) {
         *     kprintf("RD\n");
         *     return EFAULT;
         * } */
        break;
    case VM_FAULT_WRITE:
        /* if ((as->as_regions[i]->r_permissions & 2) == 0) {
         *     kprintf("WR\n");
         *     return EFAULT;
         * } */
        break;
    default:
        return EINVAL;
    }

    /* make sure it's page-aligned */
    KASSERT((paddr & PAGE_FRAME) == paddr);

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();

/* add_to_tlb: */
    elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
    ehi = faultaddress;
    tlb_random(ehi, elo);
    /* for (i=0; i<NUM_TLB; i++) {
     *     tlb_read(&ehi, &elo, i);
     *     if (elo & TLBLO_VALID) {
     *         continue;
     *     }
     *     ehi = faultaddress;
     *     elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
     *     DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
     *     tlb_write(ehi, elo, i);
     *     splx(spl);
     *     return 0;
     * }
     *
     * if (i == NUM_TLB) {
     *     for (i = 0; i < NUM_TLB; i++) {
     *         tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
     *     }
     *     goto add_to_tlb;
     * } */

    splx(spl);
    return 0;
}

vaddr_t
alloc_kpages(unsigned npages)
{
    /* (void)npages; */
    /* return 1; */
    /* return PADDR_TO_KVADDR(ram_stealmem(npages)); */
    paddr_t paddr;

    spinlock_acquire(&coremap_lock);
    if (npages > 1) {
        paddr = coremap_alloc_npages(npages);
    } else {
        paddr = coremap_alloc_page();
    }
    spinlock_release(&coremap_lock);

    if (paddr == 0) {
        return 0;
    }

    return PADDR_TO_KVADDR(paddr);
}

void
free_kpages(vaddr_t addr)
{
    paddr_t paddr = addr - MIPS_KSEG0;

    spinlock_acquire(&coremap_lock);
    coremap_free_kpages(paddr);
    spinlock_release(&coremap_lock);
}

unsigned int
coremap_used_bytes(void)
{
    return cm_used_bytes;
}

void
vm_tlbshootdown(const struct tlbshootdown *tlbshootdown)
{
    (void)tlbshootdown;
    panic("tried tlbshootdown\n");
}

void
vm_cleartlb(void) {
    int i, spl = splhigh();

    for (i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}
