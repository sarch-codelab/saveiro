#include <idt.h>
#include <string.h>

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_pointer;

extern uint32_t isr_stub_table[48];
extern void isr128(void);

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[num].base_low = (uint16_t)(handler & 0xFFFF);
    idt[num].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

static void idt_flush(void)
{
    __asm__ volatile("lidt (%0)" : : "r"(&idt_pointer));
}

void idt_init(void)
{
    int i;

    memset(idt, 0, sizeof(idt));

    idt_pointer.limit = (uint16_t)(sizeof(idt) - 1);
    idt_pointer.base = (uint32_t)&idt;

    /* Excepciones de la CPU y IRQs: puerta de interrupción, anillo 0 */
    for (i = 0; i < 48; i++)
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8E);

    /* int 0x80 accesible desde anillo 3 */
    idt_set_gate(0x80, (uint32_t)isr128, 0x08, 0xEE);

    idt_flush();
}
