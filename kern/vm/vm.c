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

    if (npages > 1) {
        paddr = coremap_alloc_npages(npages);
    } else {
        paddr = coremap_alloc_page();
    }

    return PADDR_TO_KVADDR(paddr);
}

void
free_kpages(vaddr_t addr)
{
    /* paddr_t paddr = addr - MIPS_KSEG0;
     * 
     * int page_number = paddr / PAGE_SIZE;
     * coremap[page_number]->cme_is_allocated = 0; */
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
