#ifndef SAVEIRO_PAGING_H
#define SAVEIRO_PAGING_H

#include <types.h>

void paging_init(void);
void paging_map_4mb(uint32_t phys);
uint32_t paging_directory_phys(void);

#endif
