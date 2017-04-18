/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <lib.h>
#include <spl.h>
#include <kern/errno.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <coremap.h>
#include <syscall.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
    struct addrspace *as;

    as = kmalloc(sizeof(struct addrspace));
    if (as == NULL) {
        return NULL;
    }

    as->as_regions = kmalloc(sizeof(struct region *));

    as->as_numregions = 0;
    as->as_heapmax = 0;
    as->as_regions[0] = NULL;

    return as;
}

void
as_destroy(struct addrspace *as)
{
    KASSERT(as != NULL);

    unsigned i;
    /* pid_t pid = sys_getpid(); */

    /* spinlock_acquire(&coremap_lock);
     * coremap_clear(sys_getpid());
     * spinlock_release(&coremap_lock); */
    for (i = 0; i < as->as_numregions; i++) {
        struct region *r = as->as_regions[i];
        for (unsigned j = 0; j < r->r_numpages; j++) {
            if (r->r_pages[j] != NULL) {
                /* if (coremap[r->r_pages[j]->vp_paddr/PAGE_SIZE]->cme_pid == pid) { */
                    spinlock_acquire(&coremap_lock);
                    /* kprintf("%d",
                     *         coremap[r->r_pages[j]->vp_paddr/PAGE_SIZE]->cme_pid); */
                    coremap_free_kpages(r->r_pages[j]->vp_paddr);
                    spinlock_release(&coremap_lock);
                /* } else
                 * kprintf("a"); */
            }
        }
        /* kprintf("\n"); */
    }

    for (i = 0; i < as->as_numregions; i++) {
        if (as->as_regions[i] != NULL) {
            kfree(as->as_regions[i]->r_pages);
        }
    }

    kfree(as->as_regions);
    kfree(as);
}

void
as_activate(void)
{
    int i, spl;
    struct addrspace *as;

    as = proc_getas();
    if (as == NULL) {
        /*
         * Kernel thread without an address space; leave the
         * prior address space in place.
         */
        return;
    }

    /* From dumbvm.c */
    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();

    for (i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}

void
as_deactivate(void)
{
    /*
     * Write this. For many designs it won't need to actually do
     * anything. See proc.c for an explanation of why it (might)
     * be needed.
     */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
         int readable, int writeable, int executable)
{
    KASSERT(as != NULL);

    unsigned npages, free_region;
    struct region **regionptr;

    for (free_region = 0; as->as_regions[free_region] != NULL; free_region++) {
        ;
    }

    /* If all slots are full, double the size of the region array. */
    if (free_region == as->as_numregions) {
        if (as->as_numregions == 0) { /* create two slots if it's empty */
            regionptr = kmalloc(2*sizeof(*as->as_regions));
        } else {
            regionptr = kmalloc((2*as->as_numregions)
                                * sizeof(*as->as_regions));
        }
        if (regionptr == NULL) {
            return ENOMEM;
        }

        memcpy(regionptr, as->as_regions,
               as->as_numregions * sizeof(*as->as_regions));
        kfree(as->as_regions);
        as->as_regions = regionptr;
    }

    as->as_regions[free_region] = kmalloc(sizeof(struct region));
    if (as->as_regions[free_region] == NULL) {
        return ENOMEM;
    }

    /* From dumbvm.c */
    /* Align the region. First, the base... */
    memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
    vaddr &= PAGE_FRAME;

    /* ...and now the length. */
    memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

    npages = memsize / PAGE_SIZE;

    as->as_regions[free_region]->r_startaddr = vaddr;
    as->as_regions[free_region]->r_numpages = npages;
    as->as_regions[free_region]->r_pages = NULL;
    as->as_regions[free_region]->r_permissions =
        readable | writeable | executable;
    as->as_numregions++;

    as->as_regions[free_region+1] = NULL;
    return 0;
}

int
as_prepare_load(struct addrspace *as)
{
    KASSERT(as != NULL);

    /* paddr_t pstart = 0; */
    unsigned numpages = 0;

    /*
     * r_numpages and r_startaddr have been marked by as_define_region.
     * Use these values to allocate physical pages and update region
     * information.
     */
    for (unsigned i = 0; i < as->as_numregions; i++) {
        numpages = as->as_regions[i]->r_numpages;
        as->as_regions[i]->r_pages = kmalloc(numpages * sizeof(struct vpage *));

        for (unsigned j = 0; j < numpages; j++) {
            as->as_regions[i]->r_pages[j] = NULL;
        }
    }

    return 0;
}

int
as_complete_load(struct addrspace *as)
{
    KASSERT(as != NULL);
    KASSERT(as->as_regions[as->as_numregions] == NULL);

    return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
    KASSERT(as != NULL);
    KASSERT(stackptr != NULL);
    KASSERT(as->as_regions != NULL);

    struct region *stack = NULL, *last_region = NULL;

    int result;
    vaddr_t vaddr = USERSTACK - STACKPAGES*PAGE_SIZE;
    size_t memsize = STACKPAGES * PAGE_SIZE;

    result = as_define_region(as, vaddr, memsize, 4, 2, 0);
    if (result) {
        return result;
    }

    stack = as->as_regions[as->as_numregions-1];
    stack->r_numpages = STACKPAGES;
    stack->r_pages = kmalloc(STACKPAGES * sizeof(struct vpage *));

    for (unsigned i = 0; i < stack->r_numpages; i++) {
        stack->r_pages[i] = NULL;
    }

    /* Stack region index = numregions-1 */
    /* Whatever is allocated after creating the stack is part of the heap */
    as->as_heapidx = as->as_numregions;
    /* Whatever is allocated before the stacktop is part of the heap */
    last_region = as->as_regions[as->as_numregions-2];
    as->as_heapbrk = last_region->r_startaddr
        + PAGE_SIZE*last_region->r_numpages;
    as->as_heapmax = as->as_heapbrk;

    /* Initial user-level stack pointer */
    *stackptr = USERSTACK;
    KASSERT(stack->r_startaddr == USERSTACK - STACKPAGES*PAGE_SIZE);
    KASSERT(stack->r_startaddr + stack->r_numpages*PAGE_SIZE == USERSTACK);

    return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
    KASSERT(old != NULL);
    KASSERT(ret != NULL);

    int result;
    unsigned i, j;
    paddr_t paddr;
    struct addrspace *newas;

    newas = as_create();
    if (newas==NULL) {
        return ENOMEM;
    }

    for (i = 0; i < old->as_numregions; i++) {
        struct region *rgn = old->as_regions[i];
        as_define_region(newas, rgn->r_startaddr,
                         rgn->r_numpages * PAGE_SIZE,
                         rgn->r_permissions & 4,
                         rgn->r_permissions & 2,
                         rgn->r_permissions & 1);
    }

    result = as_prepare_load(newas);
    if (result) {
        return result;
    }

    result = as_complete_load(newas);
    if (result) {
        return result;
    }

    newas->as_heapidx = old->as_heapidx;
    newas->as_heapbrk = old->as_heapbrk;
    newas->as_heapmax = old->as_heapmax;

    KASSERT(old->as_numregions == newas->as_numregions);

    for (i = 0; i < newas->as_numregions; i++) {
        struct region *region_old = old->as_regions[i];
        struct region *region_new = newas->as_regions[i];

        KASSERT(region_old->r_numpages == region_new->r_numpages);
        KASSERT(region_old->r_startaddr == region_new->r_startaddr);
        KASSERT(region_old->r_permissions == region_new->r_permissions);

        for (j = 0; j < region_old->r_numpages; j++) {
            if (region_old->r_pages[j] != NULL) {
                spinlock_acquire(&coremap_lock);
                paddr = coremap_alloc_page();
                region_new->r_pages[j] = coremap[paddr / PAGE_SIZE]->cme_page;
                spinlock_release(&coremap_lock);

                memmove((void *)PADDR_TO_KVADDR(paddr),
                        (const void *)
                        PADDR_TO_KVADDR(region_old->r_pages[j]->vp_paddr),
                        PAGE_SIZE);
            }
        }
    }

    KASSERT(old->as_heapbrk == newas->as_heapbrk);
    KASSERT(old->as_heapmax == newas->as_heapmax);
    KASSERT(old->as_heapidx == newas->as_heapidx);

    *ret = newas;
    return 0;
}
