#ifndef SAVEIRO_HEAP_H
#define SAVEIRO_HEAP_H

#include <types.h>

#define HEAP_START 0x400000u
#define HEAP_SIZE  0x400000u   /* 4 MiB */

void  heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void  kfree(void *ptr);
uint32_t heap_total_bytes(void);
uint32_t heap_used_bytes(void);
uint32_t heap_free_bytes(void);

#endif
