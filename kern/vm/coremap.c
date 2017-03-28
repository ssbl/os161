#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <current.h>
#include <vm.h>
#include <coremap.h>


int numpages = 0;
struct cm_entry **coremap;

void
coremap_init(void)
{
    int i;
    int kernel_pages;
    int entries, pages_needed, ptr_pages_needed, vpage_pages_needed;
    paddr_t ramsize, pageaddr, cmeaddr;
    struct cm_entry *cme = NULL;

    ramsize = ram_getsize();
    entries = ramsize / PAGE_SIZE;

    pages_needed = (entries * sizeof(struct cm_entry)) / PAGE_SIZE;
    ptr_pages_needed = (entries * sizeof(struct cm_entry *)) / PAGE_SIZE;
    vpage_pages_needed = (entries * sizeof(struct vpage)) / PAGE_SIZE;

    numpages = ram_stealmem(vpage_pages_needed);
    kernel_pages = numpages / PAGE_SIZE;
    pageaddr = numpages;
    cmeaddr = ram_stealmem(pages_needed);
    coremap = (struct cm_entry **)
        PADDR_TO_KVADDR(ram_stealmem(ptr_pages_needed));
    numpages = kernel_pages + pages_needed
        + ptr_pages_needed + vpage_pages_needed;

    for (i = 0; i < entries; i++) {
        cme = (struct cm_entry *) PADDR_TO_KVADDR(cmeaddr);
        cme->cme_page = (struct vpage *) PADDR_TO_KVADDR(pageaddr);
        cme->cme_page->vp_paddr = i * PAGE_SIZE;
        coremap[i] = cme;
        if (i < numpages) {
            cme->cme_is_kern_page = 1;
            cme->cme_is_pinned = 1;
        }
        cmeaddr += sizeof(struct cm_entry);
        pageaddr += sizeof(struct vpage);
    }
    numpages = i;
}
