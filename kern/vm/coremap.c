#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <current.h>
#include <vm.h>
#include <coremap.h>


struct cm_entry **coremap;

void
coremap_init(void)
{
    int entries, pages_needed, ptr_pages_needed;
    paddr_t ramsize, start;/* , firstfree; */
    /* struct cm_entry *cme = NULL; */

    ramsize = ram_getsize();
    entries = ramsize / PAGE_SIZE;

    pages_needed = (entries * sizeof(struct cm_entry)) / PAGE_SIZE;
    ptr_pages_needed = (entries * sizeof(struct cm_entry *)) / PAGE_SIZE;

    /* Allocate an extra page just to be safe. */
    coremap = (struct cm_entry **)
        PADDR_TO_KVADDR(ram_stealmem(ptr_pages_needed + 1));
    /* firstfree = ram_getfirstfree();
     * (void)firstfree; */
    /* Check page alignment. If this fails, we can't boot. */
    /* KASSERT(firstfree % PAGE_SIZE == 0); */

    start = ram_stealmem(1);
    for (int i = 0; i < pages_needed; i++) {
        coremap[i] = (struct cm_entry *) PADDR_TO_KVADDR(start);
        coremap[i]->cme_is_pinned = 1;
        start += sizeof(struct cm_entry);
        /* coremap[i]->cme_cpu_id = curcpu->c_number; */
    }
}
