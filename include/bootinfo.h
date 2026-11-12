#ifndef SAVEIRO_BOOTINFO_H
#define SAVEIRO_BOOTINFO_H

#include <types.h>

#define BOOTINFO_ADDR 0x5000

/* Entrada del mapa de memoria E820 del BIOS (24 bytes) */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} __attribute__((packed)) e820_entry_t;

typedef struct {
    uint32_t count;
    e820_entry_t entries[48];
} __attribute__((packed)) boot_info_t;

#define E820_USABLE 1

#endif
