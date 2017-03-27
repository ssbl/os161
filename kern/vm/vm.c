#include <types.h>
#include <vm.h>

void
vm_bootstrap(void)
{

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
    return PADDR_TO_KVADDR(ram_stealmem(npages));
}

void
free_kpages(vaddr_t addr)
{
    (void)addr;
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
