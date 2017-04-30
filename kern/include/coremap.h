#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <vm.h>

struct cm_entry **coremap;
struct spinlock coremap_lock;

struct cm_entry {
    struct lpage *cme_page;
    unsigned cme_cpu_id:4;
    int cme_tlb_index:7;
    /* pid_t cme_pid; */
    bool cme_is_last_page:1;
    bool cme_is_allocated:1;
    bool cme_is_pinned:1;
    bool cme_is_refd:1;
};


bool cm_initted;
int cm_numpages;
int cm_start_page;
int cm_first_free_page;
unsigned int cm_used_bytes;
int cm_last_refd_page;

void coremap_init(void);
paddr_t coremap_alloc_npages(unsigned n);
paddr_t coremap_alloc_page(void);
void coremap_free_kpages(paddr_t paddr);
void coremap_set_lastrefd(paddr_t paddr);
void coremap_set_lpage(paddr_t paddr, struct lpage *lpage);
paddr_t coremap_choose_victim(void);

#endif  /* _COREMAP_H_ */
