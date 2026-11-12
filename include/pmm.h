#ifndef SAVEIRO_PMM_H
#define SAVEIRO_PMM_H

#include <types.h>
#include <bootinfo.h>

#define PMM_MAX_MEMORY (128u * 1024u * 1024u)
#define PAGE_SIZE 4096u

void     pmm_init(boot_info_t *bi);
uint32_t pmm_alloc_page(void);
void     pmm_free_page(uint32_t addr);
void     pmm_reserve(uint32_t addr, uint32_t size);
uint32_t pmm_total_pages(void);
uint32_t pmm_used_pages(void);
uint32_t pmm_total_bytes(void);
uint32_t pmm_free_bytes(void);

#endif
