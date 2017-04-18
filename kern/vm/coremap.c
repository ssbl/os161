#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <current.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>
#include <syscall.h>


struct cm_entry **coremap;
unsigned int cm_used_bytes = 0;
int cm_numpages = 0;
int cm_first_free_page = 0;
int cm_start_page = 0;

static int
countpages(int entries, size_t size_per_entry)
{
    int total_bytes = entries * size_per_entry;

    return (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}

void
coremap_init(void)
{
    int i;
    int kernel_pages, numpages;
    int entries, pages_needed, ptr_pages_needed, vpage_pages_needed;
    paddr_t ramsize, pageaddr, cmeaddr;
    struct cm_entry *cme = NULL;

    ramsize = ram_getsize();
    entries = ramsize / PAGE_SIZE;

    pages_needed = countpages(entries, sizeof(struct cm_entry));
    ptr_pages_needed = countpages(entries, sizeof(struct cm_entry *));
    vpage_pages_needed = countpages(entries, sizeof(struct vpage));

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
    cm_first_free_page = numpages;
    cm_start_page = numpages;
    cm_used_bytes = numpages * PAGE_SIZE;
	cm_numpages = i;
}

static int
coremap_nextfree(int current_index)
{
    KASSERT(coremap != NULL);
    KASSERT(current_index > 0);

    int i;
    bool looped = false;
find_page:
    for (i = current_index; i < cm_numpages; i++) {
        if (!coremap[i]->cme_is_allocated) {
            return i;
        }
    }
    current_index = cm_start_page;
    if (looped) {
        panic("infinite loop\n");
    }
    looped = true;
    goto find_page;
}

/* SLOW */
/* Does not evict pages! */
paddr_t
coremap_alloc_npages(unsigned n)
{
    int entries = cm_numpages;
    int start = cm_first_free_page;
    unsigned pages_found = 0;

    for (int i = start; i < entries; i++) {
        if (coremap[i]->cme_is_allocated) {
            pages_found = 1;
            start = coremap_nextfree(i);
            i = start;
        } else {
            pages_found++;
        }

        if (pages_found == n) {
            /* mark them all allocated */
            coremap[i]->cme_is_last_page = 1;
            for (int j = start; j <= i; j++) {
    			cm_used_bytes += PAGE_SIZE;
				coremap[j]->cme_is_allocated = 1;
            }
            break;
        }
    }

    KASSERT(pages_found == n);

    if (cm_first_free_page == start) {
        cm_first_free_page = coremap_nextfree(start);
    }

    return coremap[start]->cme_page->vp_paddr;
}

/* Does not evict pages! */
paddr_t
coremap_alloc_page(void)
{
    paddr_t paddr = 0;
    int i, entries = cm_numpages;

    for (i = cm_start_page; i < entries; i++) {
        if (!coremap[i]->cme_is_allocated) {
            coremap[i]->cme_is_allocated = 1;
            coremap[i]->cme_is_last_page = 1;
            paddr = coremap[i]->cme_page->vp_paddr;
            cm_used_bytes += PAGE_SIZE;
            /* cm_first_free_page = coremap_nextfree(i); */
            break;
        }
    }

    /* coremap[first_free_page]->cme_is_allocated = 1;
     * coremap[first_free_page]->cme_is_last_page = 1; */


    return paddr;
}

void
coremap_free_kpages(paddr_t paddr)
{
    int start = paddr / PAGE_SIZE;

    for (int page_number = start; page_number < cm_numpages; page_number++) {
        kprintf("f");
        cm_used_bytes -= PAGE_SIZE;
		coremap[page_number]->cme_is_allocated = 0;
		if (coremap[page_number]->cme_is_last_page == 1) {
			coremap[page_number]->cme_is_last_page = 0;
			break;
		}  
	}
}
