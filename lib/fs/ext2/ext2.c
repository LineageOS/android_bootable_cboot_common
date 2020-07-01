/*
 * Copyright (c) 2007-2015 Travis Geiselbrecht
 * Copyright (c) 2019, NVIDIA Corporation. All Rights Reserved.
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

#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <trace.h>
#include <lk/init.h>
#include <fs.h>
#include <ext2fs.h>
#include <ext2_dinode.h>
#include <ext2_priv.h>

#define LOCAL_TRACE 0

void ext2_endian_swap_superblock(struct ext2fs_super_block *super_blk)
{
    LE32SWAP(super_blk->e2fs_icount);
    LE32SWAP(super_blk->e2fs_bcount);
    LE32SWAP(super_blk->e2fs_rbcount);
    LE32SWAP(super_blk->e2fs_fbcount);
    LE32SWAP(super_blk->e2fs_ficount);
    LE32SWAP(super_blk->e2fs_first_dblock);
    LE32SWAP(super_blk->e2fs_log_bsize);
    LE32SWAP(super_blk->e2fs_log_fsize);
    LE32SWAP(super_blk->e2fs_bpg);
    LE32SWAP(super_blk->e2fs_fpg);
    LE32SWAP(super_blk->e2fs_ipg);
    LE32SWAP(super_blk->e2fs_mtime);
    LE32SWAP(super_blk->e2fs_wtime);
    LE16SWAP(super_blk->e2fs_mnt_count);
    LE16SWAP(super_blk->e2fs_max_mnt_count);
    LE16SWAP(super_blk->e2fs_magic);
    LE16SWAP(super_blk->e2fs_state);
    LE16SWAP(super_blk->e2fs_beh);
    LE16SWAP(super_blk->e2fs_minrev);
    LE32SWAP(super_blk->e2fs_lastfsck);
    LE32SWAP(super_blk->e2fs_fsckintv);
    LE32SWAP(super_blk->e2fs_creator);
    LE32SWAP(super_blk->e2fs_rev);
    LE16SWAP(super_blk->e2fs_ruid);
    LE16SWAP(super_blk->e2fs_rgid);
    LE32SWAP(super_blk->e2fs_first_ino);
    LE16SWAP(super_blk->e2fs_inode_size);
    LE16SWAP(super_blk->e2fs_block_group_nr);
    LE32SWAP(super_blk->e2fs_features_compat);
    LE32SWAP(super_blk->e2fs_features_incompat);
    LE32SWAP(super_blk->e2fs_features_rocompat);
    LE32SWAP(super_blk->e2fs_algo);

    /* ext3 journal stuff */
    LE32SWAP(super_blk->e3fs_journal_inum);
    LE32SWAP(super_blk->e3fs_journal_dev);
    LE32SWAP(super_blk->e3fs_last_orphan);
    LE32SWAP(super_blk->e3fs_default_mount_opts);
    LE32SWAP(super_blk->e3fs_first_meta_bg);
}

static void ext2_endian_swap_inode(struct ext2fs_dinode *inode)
{
    LE16SWAP(inode->e2di_mode);
    LE16SWAP(inode->e2di_uid);
    LE32SWAP(inode->e2di_size);
    LE32SWAP(inode->e2di_atime);
    LE32SWAP(inode->e2di_ctime);
    LE32SWAP(inode->e2di_mtime);
    LE32SWAP(inode->e2di_dtime);
    LE16SWAP(inode->e2di_gid);
    LE16SWAP(inode->e2di_nlink);
    LE32SWAP(inode->e2di_nblock);
    LE32SWAP(inode->e2di_flags);

    // leave block pointers/symlink data alone

    LE32SWAP(inode->e2di_gen);
    LE32SWAP(inode->e2di_facl);
    LE32SWAP(inode->e2di_size_high);
    LE32SWAP(inode->e2di_faddr);

    LE16SWAP(inode->e2di_uid_high);
    LE16SWAP(inode->e2di_gid_high);
}

void ext2_endian_swap_group_desc(struct ext2_block_group_desc *grp_desc)
{
    LE32SWAP(grp_desc->ext2bgd_b_bitmap);
    LE32SWAP(grp_desc->ext2bgd_i_bitmap);
    LE32SWAP(grp_desc->ext2bgd_i_tables);
    LE16SWAP(grp_desc->ext2bgd_nbfree);
    LE16SWAP(grp_desc->ext2bgd_nifree);
    LE16SWAP(grp_desc->ext2bgd_ndirs);
}

status_t ext2_mount(struct tegrabl_bdev *dev, uint64_t start_sector, fscookie **cookie)
{
    off_t fs_offset;
    uint32_t gd_size;
    struct ext2_block_group_desc *grp_desc;
    int i;
    int err = 0;
    tegrabl_error_t error;

    LTRACEF("dev %p\n", dev);

    if (!dev)
        return ERR_NOT_FOUND;

    ext2_t *ext2 = malloc(sizeof(ext2_t));
    if (ext2 == NULL) {
        TRACEF("Failed to allocate memory for ext2 structure\n");
        err = ERR_NO_MEMORY;
        goto err;
    }
    ext2->dev = dev;

    fs_offset = (start_sector * TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
    error = tegrabl_blockdev_read(dev, &ext2->super_blk, fs_offset + 1024, sizeof(struct ext2fs_super_block));
    if (error != TEGRABL_NO_ERROR) {
        TRACEF("Failed to read superblock\n");
        err = ERR_GENERIC;
        goto err;
    }

    ext2_endian_swap_superblock(&ext2->super_blk);

    /* see if the superblock is good */
    if (ext2->super_blk.e2fs_magic != E2FS_MAGIC) {
        err = -1;
        return err;
    }

    /* calculate group count, rounded up */
    ext2->group_count = (ext2->super_blk.e2fs_bcount + ext2->super_blk.e2fs_bpg - 1) / ext2->super_blk.e2fs_bpg;

    /* print some info */
    LTRACEF("rev level %d\n", ext2->super_blk.e2fs_rev);
    LTRACEF("compat features 0x%x\n", ext2->super_blk.e2fs_features_compat);
    LTRACEF("incompat features 0x%x\n", ext2->super_blk.e2fs_features_incompat);
    LTRACEF("ro compat features 0x%x\n", ext2->super_blk.e2fs_features_rocompat);
    LTRACEF("block size %d\n", E2FS_BLOCK_SIZE(ext2->super_blk));
    LTRACEF("inode size %d\n", E2FS_INODE_SIZE(ext2->super_blk));
    LTRACEF("block count %d\n", ext2->super_blk.e2fs_bcount);
    LTRACEF("blocks per group %d\n", ext2->super_blk.e2fs_bpg);
    LTRACEF("group count %d\n", ext2->group_count);
    LTRACEF("inodes per group %d\n", ext2->super_blk.e2fs_ipg);

    /* we only support dynamic revs */
    if (ext2->super_blk.e2fs_rev > E2FS_REV1) {
        TRACEF("Unsupported revision level\n");
        err = -2;
        return err;
    }

    /* make sure it doesn't have any ro features we don't support */
    if (ext2->super_blk.e2fs_features_rocompat & ~(EXT2F_ROCOMPAT_SPARSESUPER | EXT2F_ROCOMPAT_LARGEFILE)) {
        TRACEF("Unsupported ro features\n");
        err = -3;
        return err;
    }

    /* read in all the group descriptors */
    if (ext2->super_blk.e3fs_desc_size > 32) {
        gd_size = E2FS_64BIT_GD_SIZE;
    } else {
        gd_size = E2FS_GD_SIZE;
    }
    ext2->grp_desc = malloc(gd_size * ext2->group_count);
    if (ext2->grp_desc == NULL) {
        TRACEF("Failed to allocate memory for ext2 group descriptor structure\n");
        err = ERR_NO_MEMORY;
        goto err;
    }

    error = tegrabl_blockdev_read(ext2->dev,
                                  ext2->grp_desc,
                                  fs_offset + ((E2FS_BLOCK_SIZE(ext2->super_blk) == 4096) ? 4096 : 2048),
                                  gd_size * ext2->group_count);
    if (error != TEGRABL_NO_ERROR) {
        TRACEF("Failed to read group descriptors\n");
        err = ERR_GENERIC;
        goto err;
    }

    grp_desc = ext2->grp_desc;
    for (i=0; i < ext2->group_count; i++) {
        ext2_endian_swap_group_desc(&ext2->grp_desc[i]);
        LTRACEF("group %d:\n", i);
        LTRACEF("\tblock bitmap %u\n", grp_desc->ext2bgd_b_bitmap);
        LTRACEF("\tinode bitmap %u\n", grp_desc->ext2bgd_i_bitmap);
        LTRACEF("\tinode table  %u\n", grp_desc->ext2bgd_i_tables);
        LTRACEF("\tfree blocks  %u\n", grp_desc->ext2bgd_nbfree);
        LTRACEF("\tfree inodes  %u\n", grp_desc->ext2bgd_nifree);
        LTRACEF("\tused dirs    %u\n", grp_desc->ext2bgd_ndirs);
        grp_desc = (struct ext2_block_group_desc *)((uintptr_t)grp_desc + gd_size);
    }

    /* initialize the block cache */
    ext2->cache = bcache_create(ext2->dev, E2FS_BLOCK_SIZE(ext2->super_blk), 4, fs_offset);

    /* load the first inode */
    err = ext2_load_inode(ext2, EXT2_ROOTINO, &ext2->root_inode);
    if (err < 0)
        goto err;

//  TRACE("successfully mounted volume\n");

    ext2->fs_offset = fs_offset;
    *cookie = (fscookie *)ext2;

    return 0;

err:
    LTRACEF("exiting with err code %d\n", err);

    free(ext2);
    return err;
}

status_t ext2_unmount(fscookie *cookie)
{
    // free it up
    ext2_t *ext2 = (ext2_t *)cookie;

    bcache_destroy(ext2->cache);
    free(ext2->grp_desc);
    free(ext2);

    return 0;
}

static void get_inode_addr(ext2_t *ext2, inodenum_t num, blocknum_t *block, size_t *block_offset)
{
    num--;
    struct ext2_block_group_desc *grp_desc = ext2->grp_desc;
    uint32_t group = num / ext2->super_blk.e2fs_ipg;

    // calculate the start of the inode table for the group it's in
    if (ext2->super_blk.e3fs_desc_size > 32) {
        grp_desc = (struct ext2_block_group_desc *)((uintptr_t)grp_desc + (group * E2FS_64BIT_GD_SIZE));
        *block = (((uint64_t)grp_desc->ext4bgd_i_tables_hi) << 32) | grp_desc->ext2bgd_i_tables;
    } else {
        grp_desc = (struct ext2_block_group_desc *)((uintptr_t)grp_desc + (group * E2FS_GD_SIZE));
        *block = grp_desc->ext2bgd_i_tables;
    }

    // add the offset of the inode within the group
    size_t offset = (num % EXT2_INODES_PER_GROUP(ext2->super_blk)) * E2FS_INODE_SIZE(ext2->super_blk);
    *block_offset = offset % E2FS_BLOCK_SIZE(ext2->super_blk);
    *block += offset / E2FS_BLOCK_SIZE(ext2->super_blk);
}

int ext2_load_inode(ext2_t *ext2, inodenum_t num, struct ext2fs_dinode *inode)
{
    int err;

    LTRACEF("num %d, inode %p\n", num, inode);

    blocknum_t bnum;
    size_t block_offset;
    get_inode_addr(ext2, num, &bnum, &block_offset);

    LTRACEF("bnum %lu, offset %zd\n", bnum, block_offset);

    /* get a pointer to the cache block */
    void *cache_ptr;
    err = bcache_get_block(ext2->cache, &cache_ptr, bnum);
    if (err < 0) {
        TRACEF("Failed to get block\n");
        return err;
    }

    /* copy the inode out */
    memcpy(inode, (uint8_t *)cache_ptr + block_offset, sizeof(struct ext2fs_dinode));

    /* put the cache block */
    bcache_put_block(ext2->cache, bnum);

    /* endian swap it */
    ext2_endian_swap_inode(inode);

    LTRACEF("read inode: mode 0x%x, size %d\n", inode->e2di_mode, inode->e2di_size);

    return 0;
}

static const struct fs_api ext2_api = {
    .mount = ext2_mount,
    .unmount = ext2_unmount,
    .open = ext2_open_file,
    .stat = ext2_stat_file,
    .read = ext2_read_file,
    .close = ext2_close_file,
};

STATIC_FS_IMPL(ext2, &ext2_api);
