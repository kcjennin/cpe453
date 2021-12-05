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

    // NOTE: make this work
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
    stbuf->st_atim.tv_sec = in.mtim;
    stbuf->st_atim.tv_nsec = in.mtimn;
    stbuf->st_atim.tv_sec = in.ctim;
    stbuf->st_atim.tv_nsec = in.ctimn;
    return 0;
}

/**
 * @brief performs the readdir operations on directory extents
 * 
 * @param args Args struct
 * @param block_num of the extent
 * @param buf for cb
 * @param cb function to call on all the entries
 * @return int 0 on success, negative otherwise
 */
static int readdir_helper(void *args, uint32_t block_num, void *buf, CPE453_readdir_callback_t cb, size_t remaining)
{
    int err;
    size_t amount_read=0;
    char name_buf[256];
    dir_extents_s in;
    DirEntry de;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != DIREXTENTSTYPE)
        return -ENOENT;

    // get the first entry
    de = (DirEntry) &in.contents;

    // return if there are no entries
    if (!de->len)
        return 0;

    // copy the name into a NULL-terminated buffer
    amount_read += DirEntry_name(de, name_buf);

    // call the function
    cb(buf, name_buf, de->inode);

    // do the same for the rest of the entries
    while((de = DirEntry_next(de)) && amount_read != remaining)
    {
        amount_read += DirEntry_name(de, name_buf);

        cb(buf, name_buf, de->inode);
    }

    // continue if we have extents
    if (in.next)
        return readdir_helper(args, in.next, buf, cb, remaining - amount_read);

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
    inode_s in;
    DirEntry de;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != INODETYPE)
        return -EBADF;
    if (!(in.mode & S_IFDIR))
        return -ENOTDIR;

    // get the first entry
    de = (DirEntry) &in.contents;

    // return if there are no entries
    if (!de->len)
        return 0;

    // copy the name into a NULL-terminated buffer
    amount_read += DirEntry_name(de, name_buf);

    // call the function
    cb(buf, name_buf, de->inode);

    // do the same for the rest of the entries
    while((de = DirEntry_next(de)) && amount_read != in.size)
    {
        amount_read += DirEntry_name(de, name_buf);

        cb(buf, name_buf, de->inode);
    }

    // continue if we have extents
    if (in.next)
        return readdir_helper(args, in.next, buf, cb, in.size - amount_read);

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

static int myread_helper(void *args, uint32_t block_num, char *buf, size_t size, off_t offset)
{
    int err;
    size_t local_size;
    file_extents_s in;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != FILEEXTENTSTYPE)
        return -1;

    // we start in this block
    if (offset < 4084)
    {
        local_size = 4084 - offset;

        // we continue
        if (size > local_size)
        {
            memcpy(buf, &in.contents[offset], local_size);
            err = myread_helper(args, in.next, &buf[local_size], size - local_size, 0);
            if (err < 0)
                return err;
        }
        // we end here
        else
            memcpy(buf, &in.contents[offset], size);
    }
    // we dont start in this block
    else
    {
        err = myread_helper(args, in.next, buf, size, offset - 4084);
        if (err < 0)
            return err;
    }

    return size;
}

static int myread(void *args, uint32_t block_num, char *buf, size_t size, off_t offset)
{
    int err;
    size_t local_size;
    inode_s in;
    struct Args *fs = (struct Args*)args;

    if(lseek(fs->fd, block_num * BLOCKSIZE, SEEK_SET) < 0)
        return -errno;

    err = read(fs->fd, &in, BLOCKSIZE);
    if (err != BLOCKSIZE)
        return -errno;
    if (in.type != INODETYPE)
        return -1;

    // correct for too much reading
    if (size + offset > in.size)
        size -= (size + offset) - in.size;

    // we start in this block
    if (offset < 4028)
    {

        local_size = 4028 - offset;

        // we continue
        if (size > local_size)
        {
            memcpy(buf, &in.contents[offset], local_size);
            err = myread_helper(args, in.next, &buf[local_size], size - local_size, 0);
            if (err < 0)
                return err;
        }
        // we end here
        else
            memcpy(buf, &in.contents[offset], size);
    }
    // we dont start in this block
    else
    {
        err = myread_helper(args, in.next, buf, size, offset - 4028);
        if (err < 0)
            return err;
    }

    return size;
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

