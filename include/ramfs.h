#ifndef SAVEIRO_RAMFS_H
#define SAVEIRO_RAMFS_H

#include <types.h>

#define RAMFS_MAX_FILES 16
#define RAMFS_MAX_SIZE  8192

void ramfs_init(void);
int  ramfs_create(const char *name);
int  ramfs_write(const char *name, const char *data, size_t len, bool append);
int  ramfs_read(const char *name, char *buffer, size_t cap);
int  ramfs_unlink(const char *name);
int  ramfs_count(void);
const char *ramfs_name_at(int index);
uint32_t ramfs_size_at(int index);
int  ramfs_exists(const char *name);

#endif
