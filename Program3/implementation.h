#ifndef IMPLEMENTATION_H
#define IMPLEMENTATION_H
#include <stdint.h>

#define SUPERBLOCKTYPE  1
#define INODETYPE       2
#define DIREXTENTSTYPE  3
#define FILEEXTENTSTYPE 4
#define FREETYPE        5

struct __attribute__((__packed__)) super_block_s {
    uint32_t type;
    uint8_t magic[4084];
    uint32_t root_inode;
    uint32_t free_list;
};
typedef struct super_block_s* SuperBlock;

struct __attribute__((__packed__)) free_block_s {
    uint32_t type;
    uint32_t next;
    uint8_t undefined[4088];
};
typedef struct free_block_s* FreeBlock;

struct __attribute__((__packed__)) inode_s {
    uint32_t type;
    uint16_t mode;
    uint16_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t rdev;
    uint32_t user_flags;
    uint32_t atim;
    uint32_t atimn;
    uint32_t mtim;
    uint32_t mtimn;
    uint32_t ctim;
    uint32_t ctimn;
    uint64_t size;
    uint64_t blocks;
    uint8_t contents[4028];
    uint32_t next;
};
typedef struct inode_s* Inode;

struct __attribute__((__packed__)) file_extents_s {
    uint32_t type;
    uint32_t file_inode;
    uint8_t contents[4084];
    uint32_t next;
};
typedef struct file_extents_s* FileExtents;

struct __attribute__((__packed__)) dir_extents_s {
    uint32_t type;
    uint8_t contents[4088];
    uint32_t next;
};
typedef struct dir_extents_s* DirExtents;

struct __attribute__((__packed__)) dir_entry_s {
    uint16_t len;
    uint32_t inode;
};
typedef struct dir_entry_s* DirEntry;

size_t DirEntry_name(DirEntry e, char *buf);
DirEntry DirEntry_next(DirEntry e);

#endif