#include <ramfs.h>
#include <heap.h>
#include <string.h>

typedef struct {
    char     name[32];
    char    *data;
    uint32_t size;
    bool     used;
} ramfs_file_t;

static ramfs_file_t files[RAMFS_MAX_FILES];

static ramfs_file_t *find(const char *name)
{
    int i;

    if (!name || !*name)
        return NULL;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0)
            return &files[i];
    }

    return NULL;
}

int ramfs_create(const char *name)
{
    int i;

    if (!name || strlen(name) >= sizeof(files[0].name))
        return -1;

    if (find(name))
        return 0;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            memset(&files[i], 0, sizeof(files[i]));
            strcpy(files[i].name, name);
            files[i].used = true;
            return 0;
        }
    }

    return -1;
}

int ramfs_write(const char *name, const char *data, size_t len, bool append)
{
    ramfs_file_t *file = find(name);
    uint32_t offset;
    uint32_t needed;

    if (!file)
        return -1;

    offset = append ? file->size : 0;
    needed = offset + (uint32_t)len;

    if (needed > RAMFS_MAX_SIZE)
        return -1;

    if (!file->data) {
        file->data = (char *)kmalloc(RAMFS_MAX_SIZE);
        if (!file->data)
            return -1;
    }

    memcpy(file->data + offset, data, len);
    file->size = needed;

    return (int)len;
}

int ramfs_read(const char *name, char *buffer, size_t cap)
{
    ramfs_file_t *file = find(name);
    uint32_t n;

    if (!file)
        return -1;

    n = file->size < cap ? file->size : (uint32_t)cap;
    if (n)
        memcpy(buffer, file->data, n);

    return (int)n;
}

int ramfs_unlink(const char *name)
{
    ramfs_file_t *file = find(name);

    if (!file)
        return -1;

    if (file->data)
        kfree(file->data);

    memset(file, 0, sizeof(*file));
    return 0;
}

int ramfs_count(void)
{
    int i;
    int n = 0;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used)
            n++;
    }

    return n;
}

const char *ramfs_name_at(int index)
{
    int i;
    int n = 0;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used)
            continue;
        if (n == index)
            return files[i].name;
        n++;
    }

    return NULL;
}

uint32_t ramfs_size_at(int index)
{
    int i;
    int n = 0;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used)
            continue;
        if (n == index)
            return files[i].size;
        n++;
    }

    return 0;
}

int ramfs_exists(const char *name)
{
    return find(name) != NULL;
}

static void seed(const char *name, const char *content)
{
    ramfs_create(name);
    ramfs_write(name, content, strlen(content), false);
}

void ramfs_init(void)
{
    memset(files, 0, sizeof(files));

    seed("README",
         "SaveiroOS 0.1\n"
         "=============\n"
         "\n"
         "Un sistema operativo x86 de 32 bits escrito desde cero:\n"
         "bootloader propio de dos etapas, kernel sin dependencias,\n"
         "drivers de VGA, teclado, PIT, PIC y CMOS, paginacion con\n"
         "paginas de 4 MiB, heap, RAMFS y planificador round-robin.\n"
         "\n"
         "Este archivo vive en un sistema de archivos en memoria\n"
         "(RAMFS). Puedes editarlo con:\n"
         "\n"
         "  write README hola mundo\n"
         "  cat README\n"
         "  rm README\n"
         "\n");

    seed("motd",
         "Bienvenido a SaveiroOS.\n"
         "Escribe 'help' para ver los comandos disponibles.\n"
         "Escribe 'test' para ejecutar la bateria de autopruebas.\n");

    seed("notas",
         "Ideas para la siguiente version:\n"
         "- modo usuario (anillo 3) con llamadas al sistema reales\n"
         "- paginacion de 4 KiB y memoria virtual por proceso\n"
         "- sistema de archivos persistente en disco\n"
         "- red via RTL8139\n");
}
