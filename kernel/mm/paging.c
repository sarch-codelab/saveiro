#include <paging.h>
#include <pmm.h>
#include <types.h>

/* Directorio de páginas con páginas de 4 MiB (PSE). Identidad total:
 * la memoria virtual coincide con la física hasta 128 MiB. */
static uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));

#define PSE_BIT 0x10
#define CR0_PG  0x80000000u
#define CR0_WP  0x00010000u

void paging_init(void)
{
    uint32_t i;
    uint32_t cr0;
    uint32_t cr4;

    for (i = 0; i < 32; i++)
        page_directory[i] = (i * 0x400000u) | 0x83u;    /* presente | rw | 4 MiB */

    __asm__ volatile("movl %0, %%cr3" : : "r"(page_directory));

    __asm__ volatile("movl %%cr4, %0" : "=r"(cr4));
    cr4 |= PSE_BIT;
    __asm__ volatile("movl %0, %%cr4" : : "r"(cr4));

    __asm__ volatile("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PG | CR0_WP;
    __asm__ volatile("movl %0, %%cr0" : : "r"(cr0));
}

uint32_t paging_directory_phys(void)
{
    return (uint32_t)page_directory;
}
