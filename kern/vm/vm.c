#include <types.h>
#include <lib.h>
#include <spl.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/stat.h>
#include <current.h>
#include <vfs.h>
#include <vnode.h>
#include <proc.h>
#include <vm.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <coremap.h>

unsigned swp_numslots;
struct vnode *swp_disk;
bool vm_swap_enabled = false;

void
vm_bootstrap(void)
{
    int result;
    struct stat statbuf;

    result = vfs_open((char *)SWAP_FILE, O_RDWR, 0, &swp_disk);
    if (result) {
        return;
    }

    result = VOP_STAT(swp_disk, &statbuf);
    if (result) {
        return;
    }

    swp_numslots = statbuf.st_size / PAGE_SIZE;
    vm_swap_enabled = true;
    kprintf("Swap capacity: %d pages\n", swp_numslots);
}

void
vm_swapin(void)
{

}

void
vm_swapout(void)
{
    /* choose a page to evict */
    /* write that page's data to swap file */
    /* update owner's PTE */
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
    struct lpage *lpage;

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
                    /* if (as->as_stack[i]->lp_slot == -1) { */
                    paddr = as->as_stack[i]->lp_paddr;
                    lpage = as->as_stack[i];
                    /*     goto skip_regions;
                     * } else {
                     *     paddr = vm_swapin_lpage(as->as_stack[i]); */
                    goto skip_regions;
                    /* } */
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

        KASSERT(paddr != 0);

        as->as_stack[nullpage] = as_create_lpage(paddr, faultaddress);
        if (as->as_stack[nullpage] == NULL) {
            return ENOMEM;
        }

        lpage = as->as_stack[nullpage];
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
                    lpage = as->as_heap[i];
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
            as->as_heap[nullpage] = as_create_lpage(paddr, faultaddress);
            if (as->as_heap[nullpage] == NULL) {
                return ENOMEM;
            }
        } else {
            as->as_heap[nullpage]->lp_freed = 0;
            as->as_heap[nullpage]->lp_paddr = paddr;
            as->as_heap[nullpage]->lp_startaddr = faultaddress;
        }

        lpage = as->as_heap[nullpage];
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
                spinlock_release(&coremap_lock);

                KASSERT(paddr != 0);

                region->r_pages[pageno] = as_create_lpage(paddr, faultaddress);
                if (region->r_pages[pageno] == NULL) {
                    return ENOMEM;
                }

                lpage = region->r_pages[pageno];
                bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
            } else {
                paddr = region->r_pages[pageno]->lp_paddr;
                lpage = region->r_pages[pageno];
            }
        }
    }

skip_regions:
    if (paddr == 0) {
        return EFAULT;
    }

    /* set last referenced page, update lpage in coremap entry */
    spinlock_acquire(&coremap_lock);
    coremap_set_lastrefd(paddr);
    coremap_set_lpage(paddr, lpage);
    spinlock_release(&coremap_lock);

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
