#include <types.h>
#include <vm.h>
#include <coremap.h>

void
vm_bootstrap(void)
{
    /* numpages = (int)coremap_alloc_npages(10); */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void)faulttype;
    (void)faultaddress;
    return 0;
}

vaddr_t
alloc_kpages(unsigned npages)
{
    /* (void)npages; */
    /* return 1; */
    /* return PADDR_TO_KVADDR(ram_stealmem(npages)); */
    paddr_t paddr;
	spinlock_acquire(coremap_lock);
    if (npages > 1) {
        paddr = coremap_alloc_npages(npages);
    } else {
        paddr = coremap_alloc_page();
    }
	spinlock_release(coremap_lock);
    return PADDR_TO_KVADDR(paddr);
}

void
free_kpages(vaddr_t addr)
{
    paddr_t paddr = addr - MIPS_KSEG0;
     
    int start = paddr / PAGE_SIZE;
	spinlock_acquire(coremap_lock);
	for(int page_number = start ; ; page_number++) {
		coremap[page_number]->cme_is_allocated = 0;
		if (coremap[page_number]->cme_is_last_page == 1) {
			coremap[page_number]->cme_is_last_page = 0;
			break;
		}  
	}
	spinlock_release(coremap_lock);
}

unsigned int
coremap_used_bytes(void)
{
    return 1;
}

void
vm_tlbshootdown(const struct tlbshootdown *tlbshootdown)
{
    (void)tlbshootdown;
}
