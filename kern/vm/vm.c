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
    (void)faultaddress;

    unsigned i;
    struct addrspace *as;

    int spl;
    /* int index; */
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

    for (i = 0; i < as->as_numregions; i++) {
        struct region *region = as->as_regions[i];

        if (region == NULL) {
            continue;
        }

        vbase = region->r_startaddr;
        vtop = vbase + PAGE_SIZE * region->r_numpages;

        if (faultaddress >= vbase && faultaddress < vtop) {
            /* int tlb_index; */
            int pageno = (faultaddress - vbase) / PAGE_SIZE;
            if (region->r_pages[pageno] == NULL) {
                /* kprintf("oom page!\n"); */
                /* OOM page, need to allocate */

                paddr = coremap_alloc_page();
                region->r_pages[pageno] = coremap[paddr / PAGE_SIZE]->cme_page;
                /* Invalidate its TLB entry */
                /* spl = splhigh();
                 * vaddr_t page_vaddr = vbase + unrefd_page*PAGE_SIZE;
                 * if ( (tlb_index = tlb_probe(page_vaddr & PAGE_FRAME, 0)) != -1) {
                 *     kprintf("invalidating\n");
                 *     tlb_write(TLBHI_INVALID(tlb_index),
                 *               TLBLO_INVALID(), tlb_index);
                 * }
                 * if ( (tlb_index = tlb_probe(faultaddress, 0)) != -1) {
                 *     kprintf("invalidating\n");
                 *     tlb_write(TLBHI_INVALID(tlb_index),
                 *               TLBLO_INVALID(), tlb_index);
                 * }
                 * splx(spl); */
                bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
            }

            paddr = region->r_pages[pageno]->vp_paddr;
            coremap[paddr / PAGE_SIZE]->cme_is_referenced = 1;
            break;
        }
    }

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

add_to_tlb:
    for (i=0; i<NUM_TLB; i++) {
        tlb_read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }
        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
        tlb_write(ehi, elo, i);
        splx(spl);
        return 0;
    }

    if (i == NUM_TLB) {
        for (i = 0; i < NUM_TLB; i++) {
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }
        goto add_to_tlb;
    }

    splx(spl);
    return EFAULT;
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
}
