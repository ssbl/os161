#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <vm.h>

struct cm_entry **coremap;
struct spinlock coremap_lock;

struct cm_entry {
    struct vpage *cme_page;
    unsigned cme_cpu_id:4;
    int cme_tlb_index:7;
    bool cme_is_last_page:1;
    bool cme_is_allocated:1;
    bool cme_is_pinned:1;
};


int cm_numpages;
int cm_first_free_page;
unsigned int cm_used_bytes;

void coremap_init(void);
paddr_t coremap_alloc_npages(unsigned n);
paddr_t coremap_alloc_page(void);
void coremap_free_kpages(paddr_t paddr);

#endif  /* _COREMAP_H_ */
