#include <heap.h>
#include <string.h>

#define HEAP_MAGIC_USED 0x5A5A1234u
#define HEAP_MAGIC_FREE 0xF4EE5678u
#define ALIGNMENT       16u

typedef struct block {
    uint32_t magic;
    uint32_t size;              /* bytes útiles */
    struct block *next;
    bool free;
} block_t;

static block_t *first_block;
static uint32_t heap_used;

static uint32_t align_up(uint32_t value)
{
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void heap_init(void)
{
    first_block = (block_t *)HEAP_START;
    first_block->magic = HEAP_MAGIC_FREE;
    first_block->size = HEAP_SIZE - (uint32_t)sizeof(block_t);
    first_block->next = NULL;
    first_block->free = true;
    heap_used = 0;
}

void *kmalloc(size_t size)
{
    block_t *block = first_block;
    uint32_t need;

    if (size == 0)
        return NULL;

    need = align_up((uint32_t)size);

    while (block) {
        if (block->free && block->size >= need) {
            /* ¿Se puede dividir? */
            if (block->size >= need + sizeof(block_t) + ALIGNMENT) {
                block_t *rest = (block_t *)((uint8_t *)block + sizeof(block_t) + need);

                rest->magic = HEAP_MAGIC_FREE;
                rest->size = block->size - need - (uint32_t)sizeof(block_t);
                rest->free = true;
                rest->next = block->next;

                block->next = rest;
                block->size = need;
            }

            block->free = false;
            block->magic = HEAP_MAGIC_USED;
            heap_used += block->size;

            return (uint8_t *)block + sizeof(block_t);
        }
        block = block->next;
    }

    return NULL;
}

void *kcalloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = kmalloc(total);

    if (ptr)
        memset(ptr, 0, total);

    return ptr;
}

void kfree(void *ptr)
{
    block_t *block;

    if (!ptr)
        return;

    block = (block_t *)((uint8_t *)ptr - sizeof(block_t));

    if (block->magic != HEAP_MAGIC_USED)
        return;

    block->magic = HEAP_MAGIC_FREE;
    block->free = true;
    heap_used -= block->size;

    /* Fusionar con el bloque siguiente si también está libre */
    if (block->next && block->next->free && block->next->magic == HEAP_MAGIC_FREE) {
        block->size += block->next->size + (uint32_t)sizeof(block_t);
        block->next = block->next->next;
    }
}

uint32_t heap_total_bytes(void)
{
    return HEAP_SIZE;
}

uint32_t heap_used_bytes(void)
{
    return heap_used;
}

uint32_t heap_free_bytes(void)
{
    return HEAP_SIZE - heap_used;
}
