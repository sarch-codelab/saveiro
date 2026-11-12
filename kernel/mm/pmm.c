#include <pmm.h>
#include <heap.h>
#include <bootinfo.h>
#include <string.h>
#include <console.h>

#define MAX_PAGES    (PMM_MAX_MEMORY / PAGE_SIZE)
#define BITMAP_WORDS (MAX_PAGES / 32)

extern uint8_t _kernel_end;

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_pages;
static uint32_t used_pages;

static bool bit_is_used(uint32_t page)
{
    return (bitmap[page / 32] >> (page % 32)) & 1u;
}

static void bit_set_used(uint32_t page)
{
    if (!bit_is_used(page)) {
        bitmap[page / 32] |= 1u << (page % 32);
        used_pages++;
    }
}

static void bit_set_free(uint32_t page)
{
    if (bit_is_used(page)) {
        bitmap[page / 32] &= ~(1u << (page % 32));
        used_pages--;
    }
}

void pmm_reserve(uint32_t addr, uint32_t size)
{
    uint32_t first = addr / PAGE_SIZE;
    uint32_t last = (addr + size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (; first < last && first < total_pages; first++)
        bit_set_used(first);
}

void pmm_init(boot_info_t *bi)
{
    uint64_t limit = 0;
    uint32_t i;

    memset(bitmap, 0xFF, sizeof(bitmap));
    used_pages = MAX_PAGES;
    total_pages = 0;

    for (i = 0; i < bi->count && i < 48; i++) {
        uint64_t end;

        if (bi->entries[i].type != E820_USABLE)
            continue;
        if (bi->entries[i].base >= PMM_MAX_MEMORY)
            continue;

        end = bi->entries[i].base + bi->entries[i].length;
        if (end > PMM_MAX_MEMORY)
            end = PMM_MAX_MEMORY;
        if (end > limit)
            limit = end;
    }

    total_pages = (uint32_t)(limit / PAGE_SIZE);
    used_pages = MAX_PAGES;

    /* Liberar las regiones utilizables */
    for (i = 0; i < bi->count && i < 48; i++) {
        uint64_t start;
        uint64_t end;

        if (bi->entries[i].type != E820_USABLE)
            continue;

        start = bi->entries[i].base;
        end = bi->entries[i].base + bi->entries[i].length;

        if (start >= PMM_MAX_MEMORY)
            continue;
        if (end > PMM_MAX_MEMORY)
            end = PMM_MAX_MEMORY;

        start = (start + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

        for (; start + PAGE_SIZE <= end; start += PAGE_SIZE) {
            uint32_t page = (uint32_t)(start / PAGE_SIZE);

            if (page < total_pages)
                bit_set_free(page);
        }
    }

    /* Reservar zonas que no debe tocar el asignador */
    pmm_reserve(0, 0x100000);                            /* BIOS, VGA, EBDA */
    pmm_reserve(BOOTINFO_ADDR, PAGE_SIZE);
    pmm_reserve((uint32_t)&_kernel_end, PAGE_SIZE);      /* fin del kernel */
    pmm_reserve(0x10000, (uint32_t)&_kernel_end - 0x10000);
    pmm_reserve(HEAP_START, HEAP_SIZE);                  /* heap del kernel */

    /* Recontar sólo las páginas que existen físicamente */
    used_pages = 0;
    for (i = 0; i < total_pages; i++) {
        if (bit_is_used(i))
            used_pages++;
    }
}

uint32_t pmm_alloc_page(void)
{
    uint32_t page;

    for (page = 256; page < total_pages; page++) {
        if (!bit_is_used(page)) {
            bit_set_used(page);
            return page * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_page(uint32_t addr)
{
    uint32_t page = addr / PAGE_SIZE;

    if (addr & (PAGE_SIZE - 1))
        return;
    if (page < 256 || page >= total_pages)
        return;

    bit_set_free(page);
}

uint32_t pmm_total_pages(void)
{
    return total_pages;
}

uint32_t pmm_used_pages(void)
{
    return used_pages;
}

uint32_t pmm_total_bytes(void)
{
    return total_pages * PAGE_SIZE;
}

uint32_t pmm_free_bytes(void)
{
    return (total_pages - used_pages) * PAGE_SIZE;
}
