#include <gdt.h>
#include <types.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdt_pointer;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran)
{
    gdt[num].base_low = (uint16_t)(base & 0xFFFF);
    gdt[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[num].base_high = (uint8_t)((base >> 24) & 0xFF);

    gdt[num].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[num].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[num].access = access;
}

static void gdt_flush(void)
{
    __asm__ volatile("lgdt (%0)" : : "r"(&gdt_pointer));

    __asm__ volatile(
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        : : : "eax");
}

void gdt_init(void)
{
    gdt_pointer.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_pointer.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    /* 0x08: código en anillo 0, 4 GiB */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
    /* 0x10: datos en anillo 0, 4 GiB */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    gdt_flush();
}
