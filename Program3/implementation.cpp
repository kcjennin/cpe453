#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "cpe453fs.h"
#include "implementation.h"

#define BLOCKSIZE 4096

struct Args
{
    int fd;
};

/**
 * @brief Sets the file descriptor for the image
 * 
 * @param args Args struct
 * @param fd of the image
 */
static void set_file_descriptor(void *args, int fd)
{
    struct Args *fs = (struct Args*)args;
    fs->fd = fd;
}

/**
 * @brief fills a stat struct for the file given
 * 
 * @param args Args struct
 * @param block_num of the file
 * @param stbuf stat structure
 * @return int 0 on success, negative otherwise
 */
static int mygetattr(void *args, uint32_t block_num, struct stat *stbuf)
{
    int err;
    inode_s in;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != INODETYPE)
        return -EBADF;

    // stbuf->st_dev = ;
    stbuf->st_ino = block_num;
    stbuf->st_mode = in.mode;
    stbuf->st_nlink = in.nlink;
    stbuf->st_uid = in.uid;
    stbuf->st_gid = in.gid;
    stbuf->st_rdev = in.rdev;
    stbuf->st_size = in.size;
    stbuf->st_blksize = BLOCKSIZE;
    stbuf->st_blocks = in.blocks;
    stbuf->st_atim.tv_sec = in.atim;
    stbuf->st_atim.tv_nsec = in.atimn;
    stbuf->st_mtim.tv_sec = in.mtim;
    stbuf->st_mtim.tv_nsec = in.mtimn;
    stbuf->st_ctim.tv_sec = in.ctim;
    stbuf->st_ctim.tv_nsec = in.ctimn;
    return 0;
}


/**
 * @brief calls the function cb on all the entries in the directory
 * 
 * @param args Args struct
 * @param block_num of the directory
 * @param buf for cb
 * @param cb function to call on the directory entries
 * @return int 0 on success, negative otherwise
 */
static int myreaddir(void *args, uint32_t block_num, void *buf, CPE453_readdir_callback_t cb)
{
    int err;
    size_t amount_read=0;
    char name_buf[256];
    union {
        inode_s inode;
        dir_extents_s extent;
    } block;
    DirEntry de;
    
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &block.inode, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (block.inode.type != INODETYPE)
        return -EBADF;
    if (!(block.inode.mode & S_IFDIR))
        return -ENOTDIR;

    for (;;)
    {
        // get the first entry
        if (block.inode.type == INODETYPE)
            de = (DirEntry) &block.inode.contents;
        else
            de = (DirEntry) &block.extent.contents;

        // return if there are no entries
        if (!de->len)
            return 0;

        // copy the name into a NULL-terminated buffer
        amount_read += DirEntry_name(de, name_buf);

        // call the function
        cb(buf, name_buf, de->inode);

        // do the same for the rest of the entries
        while((de = DirEntry_next(de)) && amount_read != block.inode.size)
        {
            amount_read += DirEntry_name(de, name_buf);

            cb(buf, name_buf, de->inode);
        }

        // exit if done
        if (!block.inode.next)
            return 0;

        block_num = block.inode.next;
        if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
            return -errno;

        err = read(fs->fd, &block.extent, BLOCKSIZE);
        if (err != BLOCKSIZE)
            return -errno;
        if (block.inode.type != DIREXTENTSTYPE)
            return -EBADF;
    }

    return 0;
}

static int myopen(void *args, uint32_t block_num)
{
    int err;
    inode_s in;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != INODETYPE)
        return -EBADF;
    if (!(in.mode & S_IFREG))
    {
        if (in.mode & S_IFDIR)
            return -EISDIR;
        else
            return -1;
    }

    return 0;
}

static int myread(void *args, uint32_t block_num, char *buf, size_t size, off_t offset)
{
    int err, size_of_block=4028;
    size_t local_size, size_temp, i_buf=0;
    union {
        inode_s inode;
        file_extents_s extent;
    } block;
    struct Args *fs = (struct Args*)args;

    // load the inode
    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &block, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (block.inode.type != INODETYPE)
        return -1;

    // correct for too much reading
    if (size + offset > block.inode.size)
        size -= (size + offset) - block.inode.size;

    size_temp = size;

    for (;;)
    {
        // check if we are in the right block
        if (offset < size_of_block)
        {
            local_size = size_of_block - offset;

            // if overflows
            if (size_temp > local_size)
            {
                // set the offset and size for the next block
                if (block.inode.type == INODETYPE)
                    memcpy(&buf[i_buf], &block.inode.contents[offset], local_size);
                else
                    memcpy(&buf[i_buf], &block.extent.contents[offset], local_size);
                i_buf += local_size;
                offset = 0;
                size_temp -= local_size;
            }
            else
            {
                if (block.inode.type == INODETYPE)
                    memcpy(&buf[i_buf], &block.inode.contents[offset], size_temp);
                else
                    memcpy(&buf[i_buf], &block.extent.contents[offset], size_temp);
                return size;
            }
        }
        else
            offset -= size_of_block;

        if (block.inode.type == INODETYPE)
            size_of_block = 4084;

        // get the next block
        block_num = block.inode.next;
        if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
            return -errno;

        err = read(fs->fd, &block, BLOCKSIZE);
        if (err != BLOCKSIZE)
            return -errno;
        if (block.inode.type != FILEEXTENTSTYPE)
            return -1;
    }
}

static int myreadlink(void *args, uint32_t block_num, char *buf, size_t size)
{
    int err;
    size_t lnk_size;
    inode_s in;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != INODETYPE)
        return -EBADF;
    if (!(in.mode & S_IFLNK))
        return -EINVAL;

    lnk_size = in.size;

    if (lnk_size < size - 1)
        lnk_size = size - 1;

    memcpy(buf, in.contents, lnk_size);
    buf[lnk_size] = 0;

    return 0;
}


static uint32_t root_node(void *args)
{
    int err;
    super_block_s sb;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, 0, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &sb, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;

    return sb.root_inode;
}

size_t DirEntry_name(DirEntry e, char *buf)
{
    char *name = (char *)(e + 1);

    if (!e->len)
        return 0;

    memset(buf, 0, 256);
    memcpy(buf, name, e->len - sizeof(dir_entry_s));

    return e->len;
}

DirEntry DirEntry_next(DirEntry e)
{
    DirEntry next_dir;

    next_dir = (DirEntry)(((uint8_t *)e) + e->len);
    if (next_dir->len)
        return next_dir;
    return NULL;
}

#ifdef  __cplusplus
extern "C" {
#endif

struct cpe453fs_ops *CPE453_get_operations(void)
{
    static struct cpe453fs_ops ops;
    static struct Args args;
    memset(&ops, 0, sizeof(ops));
    ops.arg = &args;

    ops.getattr = mygetattr;
    ops.readdir = myreaddir;
    ops.open = myopen;
    ops.read = myread;
    ops.readlink = myreadlink;
    ops.root_node = root_node;
    ops.set_file_descriptor = set_file_descriptor;

    return &ops;
}

#ifdef  __cplusplus
}
#endif

