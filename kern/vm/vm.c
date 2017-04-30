#include <types.h>
#include <lib.h>
#include <spl.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/stat.h>
#include <current.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>
#include <proc.h>
#include <addrspace.h>
#include <vm.h>
#include <mips/tlb.h>
#include <coremap.h>
#include <bitmap.h>

unsigned swp_numslots;
struct vnode *swp_disk;
struct bitmap *swp_bitmap;
struct lock *swp_lock;
bool vm_swap_enabled = false;

struct lpage *
vm_create_lpage(paddr_t paddr, vaddr_t faultaddress)
{
    struct lpage *lpage = NULL;

    lpage = kmalloc(sizeof(struct lpage));
    if (lpage == NULL) {
        return NULL;
    }

    lpage->lp_lock = lock_create("lp_lock");
    if (lpage->lp_lock == NULL) {
        kfree(lpage);
        return NULL;
    }

    lpage->lp_paddr = paddr;
    lpage->lp_startaddr = faultaddress;
    lpage->lp_freed = 0;
    lpage->lp_slot = -1;

    /* spinlock_acquire(&coremap_lock);
     * coremap[paddr / PAGE_SIZE]->cme_page = lpage;
     * spinlock_release(&coremap_lock); */

    return lpage;
}

void
vm_destroy_lpage(struct lpage *lpage)
{
    KASSERT(lpage != NULL);
    KASSERT(lock_do_i_hold(lpage->lp_lock));

    lock_release(lpage->lp_lock);
    lock_destroy(lpage->lp_lock);
    kfree(lpage);
}

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
        panic("swap: could not stat swap file");
    }

    swp_numslots = statbuf.st_size / PAGE_SIZE;

    swp_bitmap = bitmap_create(swp_numslots);
    if (swp_bitmap == NULL) {
        panic("swap: could not create bitmap");
    }

    swp_lock = lock_create("swp_lock");
    if (swp_lock == NULL) {
        panic("swap: could not create bitmap lock");
    }

    vm_swap_enabled = true;
    kprintf("Swap capacity: %d pages\n", swp_numslots);
    kprintf("Kernel pages used: %d\n", cm_start_page);
}

void
vm_cleartlb(void) {
    int i, spl = splhigh();

    for (i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}

int
swp_get_slot(void)
{
    /* KASSERT(lock_do_i_hold(swp_lock)); */

    unsigned i;

    for (i = 0; i < swp_numslots; i++) {
        if (!bitmap_isset(swp_bitmap, i)) {
            return i;
        }
    }

    panic("Ran out of disk");
}

void
vm_swapin(struct lpage *lpage)
{
    KASSERT(lpage != NULL);
    KASSERT(lock_do_i_hold(lpage->lp_lock));
    KASSERT(bitmap_isset(swp_bitmap, lpage->lp_slot));

    int result;
    paddr_t paddr;
    struct uio uio;
    struct iovec iov;

    paddr = coremap_alloc_page(); /* locks */

    uio_kinit(&iov, &uio, (void *)PADDR_TO_KVADDR(paddr),
              PAGE_SIZE, 0, UIO_READ);
    uio.uio_offset = lpage->lp_slot * PAGE_SIZE;

    result = VOP_READ(swp_disk, &uio);
    if (result) {
        /* TODO: fail in some other way */
        panic("error reading from swap disk");
    }

    lock_acquire(swp_lock);
    bitmap_unmark(swp_bitmap, lpage->lp_slot);
    lock_release(swp_lock);

    lpage->lp_paddr = paddr;
    lpage->lp_slot = -1;

    vm_cleartlb();
}

paddr_t
vm_swapout(struct lpage *lpage)
{
    KASSERT(lpage != NULL);
    KASSERT(lock_do_i_hold(lpage->lp_lock));
    KASSERT(spinlock_do_i_hold(&coremap_lock));

    int slot, result;
    paddr_t paddr_victim;
    struct uio uio;
    struct iovec iov;

    /* get a free disk slot */
    lock_acquire(swp_lock);
    slot = swp_get_slot();
    bitmap_mark(swp_bitmap, slot);
    lock_release(swp_lock);

    /* update slot in PTE */
    paddr_victim = lpage->lp_paddr;
    lpage->lp_slot = slot;
    lpage->lp_paddr = 0;
    coremap[paddr_victim / PAGE_SIZE]->cme_page = NULL;

    spinlock_release(&coremap_lock);

    /* write that page's data to swap file */
    uio_kinit(&iov, &uio, (void *)PADDR_TO_KVADDR(paddr_victim),
              PAGE_SIZE, 0, UIO_WRITE);
    uio.uio_offset = slot*PAGE_SIZE;

    result = VOP_WRITE(swp_disk, &uio);
    if (result) {
        /* TODO: fail in some other way */
        panic("error writing to swap disk");
    }

    /* vm_cleartlb(); */
    lock_release(lpage->lp_lock);
    as_activate();
    return paddr_victim;
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
                lock_acquire(as->as_stack[i]->lp_lock);
                vbase = as->as_stack[i]->lp_startaddr;

                if (faultaddress == vbase) {
                    if (as->as_stack[i]->lp_paddr == 0) {
                        vm_swapin(as->as_stack[i]);
                    }

                    KASSERT(as->as_stack[i]->lp_paddr != 0);
                    KASSERT(as->as_stack[i]->lp_slot == -1);

                    lpage = as->as_stack[i];
                    paddr = lpage->lp_paddr;
                    lock_release(lpage->lp_lock);
                    goto skip_regions;
                }
                lock_release(as->as_stack[i]->lp_lock);
            } else {
                nullpage = i;
            }
        }

        KASSERT(i == LPAGES);

        as->as_heapmax = faultaddress;

        paddr = coremap_alloc_page();

        KASSERT(paddr != 0);

        as->as_stack[nullpage] = vm_create_lpage(paddr, faultaddress);
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

        paddr = coremap_alloc_page();

        KASSERT(paddr != 0);

        if (as->as_heap[nullpage] == NULL) {
            as->as_heap[nullpage] = vm_create_lpage(paddr, faultaddress);
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
                paddr = coremap_alloc_page();
                KASSERT(paddr != 0);

                region->r_pages[pageno] = vm_create_lpage(paddr, faultaddress);
                if (region->r_pages[pageno] == NULL) {
                    return ENOMEM;
                }

                lpage = region->r_pages[pageno];
                bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
            } else {
                lock_acquire(region->r_pages[pageno]->lp_lock);
                if (region->r_pages[pageno]->lp_paddr == 0) {
                    vm_swapin(region->r_pages[pageno]);
                }

                KASSERT(region->r_pages[pageno]->lp_paddr != 0);
                KASSERT(region->r_pages[pageno]->lp_slot == -1);

                lpage = region->r_pages[pageno];
                paddr = lpage->lp_paddr;
                lock_release(lpage->lp_lock);
            }
        }
    }

skip_regions:
    if (paddr == 0) {
        return EFAULT;
    }

    spinlock_acquire(&coremap_lock);
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
    paddr_t paddr;

    if (npages > 1) {
        spinlock_acquire(&coremap_lock);
        paddr = coremap_alloc_npages(npages);
        /* if (spinlock_do_i_hold(&coremap_lock)) { */
            spinlock_release(&coremap_lock);
        /* } */
    } else {
        paddr = coremap_alloc_page();
    }

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
    /* panic("vm_tlbshootdown"); */
    as_activate();
}
