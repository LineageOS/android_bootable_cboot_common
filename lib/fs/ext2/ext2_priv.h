/*
 * Copyright (c) 2007-2015 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __EXT2_PRIV_H
#define __EXT2_PRIV_H

#include <bcache.h>
#include <fs.h>
#include <ext2fs.h>
#include <ext2_dinode.h>
#include <tegrabl_blockdev.h>

typedef uint64_t blocknum_t;
typedef uint32_t inodenum_t;
typedef uint32_t groupnum_t;

typedef struct {
    struct tegrabl_bdev *dev;
    bcache_t cache;

    struct ext2fs_super_block super_blk;
    int group_count;
    struct ext2_block_group_desc *grp_desc;
    struct ext2fs_dinode root_inode;

    uint64_t fs_offset;
} ext2_t;

struct cache_block {
    blocknum_t num;
    void *ptr;
};

/* open file handle */
typedef struct {
    ext2_t *ext2;

    struct cache_block ind_cache[3]; // cache of indirect blocks as they're scanned
    struct ext2fs_dinode inode;
} ext2_file_t;

/* internal routines */

/**
 * @brief Change superblock variables to correct endianess
 *
 * @param sb Superblock structure
 */
void ext2_endian_swap_superblock(struct ext2fs_super_block *super_blk);

/**
 * @brief Change group desc variables to correct endianess
 *
 * @param gd Group descriptor structure
 */
void ext2_endian_swap_group_desc(struct ext2_block_group_desc *grp_desc);
int ext2_load_inode(ext2_t *ext2, inodenum_t num, struct ext2fs_dinode *inode);
int ext2_lookup(ext2_t *ext2, const char *path, inodenum_t *inum); // path to inode

/* io */
int ext2_read_block(ext2_t *ext2, void *buf, blocknum_t bnum);
int ext2_get_block(ext2_t *ext2, void **ptr, blocknum_t bnum);
int ext2_put_block(ext2_t *ext2, blocknum_t bnum);

off_t ext2_file_len(ext2_t *ext2, struct ext2fs_dinode *inode);
ssize_t ext2_read_inode(ext2_t *ext2, struct ext2fs_dinode *inode, void *buf, off_t offset, size_t len);
int ext2_read_link(ext2_t *ext2, struct ext2fs_dinode *inode, char *str, size_t len);

/* fs api */
status_t ext2_mount(struct tegrabl_bdev *dev, uint64_t start_sector, fscookie **cookie);
status_t ext2_unmount(fscookie *cookie);
status_t ext2_open_file(fscookie *cookie, const char *path, filecookie **fcookie);
ssize_t ext2_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len);
status_t ext2_close_file(filecookie *fcookie);
status_t ext2_stat_file(filecookie *fcookie, struct file_stat *);

/* mode stuff */
#define S_IFMT      0170000
#define S_IFIFO     0010000
#define S_IFCHR     0020000
#define S_IFDIR     0040000
#define S_IFBLK     0060000
#define S_IFREG     0100000
#define S_IFLNK     0120000
#define S_IFSOCK    0140000

#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

/* Inode flags */
#define FLAG_EXTENTS                0x00080000
#define FLAG_HASHED_INDEX           0x00001000

#define IS_EXTENTS(flags)           (((flags) & FLAG_EXTENTS) == FLAG_EXTENTS)
#define IS_HASHED_INDEX(flags)      (((flags) & FLAG_HASHED_INDEX) == FLAG_HASHED_INDEX)

#endif
