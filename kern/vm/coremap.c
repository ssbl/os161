#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <current.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>

int numpages = 0, first_free_page = 0;
struct cm_entry **coremap;
int start_page = 0;

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

    /* these values could be 0, set them to 1 if that's the case */
    vpage_pages_needed = vpage_pages_needed == 0 ? 1 : vpage_pages_needed;
    pages_needed = pages_needed == 0 ? 1: pages_needed;
    ptr_pages_needed = ptr_pages_needed == 0 ? 1 : ptr_pages_needed;

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
            cme->cme_is_pinned = 1;
            cme->cme_is_allocated = 1;
        }
        cmeaddr += sizeof(struct cm_entry);
        pageaddr += sizeof(struct vpage);
    }

    spinlock_init(&coremap_lock);
    first_free_page = numpages;
    start_page = numpages;
    numpages = i;
}

static int
coremap_nextfree(int current_index)
{
    KASSERT(coremap != NULL);
    KASSERT(current_index > 0);

    int i;
find_page:
    for (i = current_index; i < numpages; i++) {
        if (!coremap[i]->cme_is_allocated) {
            return i;
        }
    }
    current_index = start_page;
    goto find_page;
}

/* SLOW */
paddr_t
coremap_alloc_npages(unsigned n)
{
    int entries = numpages;
    int start = first_free_page;
    unsigned pages_found = 0;

    for (int i = start; i < entries; i++) {
        if (coremap[i]->cme_is_allocated) {
            pages_found = 0;
            start = coremap_nextfree(i);
            i = start;
        } else {
            pages_found++;
        }

        if (pages_found == n) {
            /* mark them all allocated */
            coremap[i]->cme_is_last_page = 1;
            for (int j = start; j <= i; j++) {
                coremap[j]->cme_is_allocated = 1;
            }
            break;
        }
    }

    if (first_free_page == start) {
        first_free_page = coremap_nextfree(start);
    }

    return coremap[start]->cme_page->vp_paddr;
}

paddr_t
coremap_alloc_page(void)
{
    paddr_t paddr;
    int i, entries = numpages;

    for (i = first_free_page; i < entries; i++) {
        if (!coremap[i]->cme_is_allocated) {
            coremap[i]->cme_is_allocated = 1;
            coremap[i]->cme_is_last_page = 1;
            break;
        }
    }

    /* coremap[first_free_page]->cme_is_allocated = 1;
     * coremap[first_free_page]->cme_is_last_page = 1; */
    paddr = coremap[i]->cme_page->vp_paddr;

    /* first_free_page = coremap_nextfree(first_free_page); */
    return paddr;
}
