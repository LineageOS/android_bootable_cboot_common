/*
 * Copyright (c) 2019-2020, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <trace.h>
#include <lk/init.h>
#include <fs.h>
#include <ext2fs.h>
#include <ext2_dinode.h>
#include <ext2_dir.h>
#include <ext2_priv.h>
#include <ext4_priv.h>

#define LOCAL_TRACE    0

/**
 * @brief Extent tree header
 *        This describes how extents are arranged as a tree.
 */
struct ext4_extent_header {
    uint16_t magic;        /* Magic number */
    uint16_t entries;      /* Number of valid entries following the header */
    uint16_t max_entries;  /* Maximum number of entries that could follow the header */
    uint16_t depth;        /* Depth of this extent node in the extent tree */
    uint32_t generation;   /* Generation of the tree */
};

/**
 * @brief Extent index node
 *        This describes the internal nodes of the extent tree.
 */
struct ext4_extent_idx {
    uint32_t block;        /* This index node covers file blocks from 'block' onward */
    uint32_t leaf_lo;      /* Lower 32-bits of the block number of the extent node that is the next level */
                           /* lower in the tree */
    uint16_t leaf_hi;      /* Upper 16-bits of the previous field */
    uint16_t unused;
};

/**
 * @brief Extent info
 *        This describes the leaf nodes of the extent tree.
 */
struct ext4_extent {
    uint32_t block_no;     /* First file block number that this extent covers */
    uint16_t len;          /* Number of blocks covered by extent */
    uint16_t start_hi;     /* Upper 16-bits of the block number to which this extent points */
    uint32_t start_lo;     /* Lower 32-bits of the block number to which this extent points */
};

/**
 * @brief Hash entry
 */
struct dx_entry {
    uint32_t hash;         /* Hash code */
    uint32_t block;        /* Block num (within the dir file, not fs blocks) of the next node in the htree */
};

/**
 * @brief Root hash tree
 *        This describes meta data for hashed entries.
 */
struct dx_root {
    uint32_t dot_inode;         /* Inode num of this directory */
    uint16_t dot_rec_len;       /* Length of this record, 12 */
    uint8_t dot_name_len;       /* Length of the name, 1 */
    uint8_t dot_file_type;      /* File type of this entry, 0x2 (directory) */
    char dot_name[4];           /* ".\0\0\0" */
    uint32_t dot_dot_inode;     /* Inode num of parent directory */
    uint16_t dot_dot_rec_len;   /* block_size - 12. The record len is long enough to cover all htree data */
    uint8_t dot_dot_name_len;   /* Length of the name, 2 */
    uint8_t dot_dot_file_type;  /* File type of this entry, 0x2 (directory) */
    char dot_dot_name[4];       /* "..\0\0" */
    uint32_t reserved_zero;     /* Zero */
    uint8_t hash_version;       /* Hash version */
    uint8_t info_len;           /* Length of the tree information, 0x8 */
    uint8_t indirect_levels;    /* Depth of the tree */
    uint8_t unused_flags;
    uint16_t limit;             /* Max num of dx_entries that can follow this header, +1 for header itself */
    uint16_t count;             /* Actual num of dx_entries that follow this header, +1 for header itself */
    uint32_t block;             /* The block num (within the directory file) that goes with hash=0 */
};

static inline bool validate_extents_magic(struct ext4_extent_header *extent_header)
{
    return (extent_header->magic == E4FS_EXTENTS_MAGIC) ? true: false;
}

static int extents_blk_lookup(ext2_t *ext2, struct ext4_extent_header *extent_header, uint32_t num, void *buf)
{
    struct ext4_extent *extent = NULL;
    off_t blk_num = 0;
    off_t blk_addr;
    uint16_t i;
    uint32_t n;
    int err = 0;

    LTRACEF("Extent: depth: %u, entries: %u\n", extent_header->depth, extent_header->entries);
    extent = (struct ext4_extent *)((uintptr_t)extent_header + sizeof(struct ext4_extent_header));

    for (i = 0; i < extent_header->entries; i++) {
        n = extent->block_no + extent->len;
        if (n > num) {
            blk_num = extent->start_hi;
            blk_num = ((blk_num << 32U) | extent->start_lo) + (num - extent->block_no);
            break;
        }
        extent++;
    }

    blk_addr = blk_num * E2FS_BLOCK_SIZE(ext2->super_blk);
    blk_addr += ext2->fs_offset;
    err = tegrabl_blockdev_read(ext2->dev, buf, blk_addr, E2FS_BLOCK_SIZE(ext2->super_blk));
    if (err != TEGRABL_NO_ERROR) {
        TRACEF("blockdev read failed, err 0x%08x\n", err);
        err = ERR_GENERIC;
        goto fail;
    }

fail:
    return err;
}

static int get_extents_blk(ext2_t *ext2, struct ext2fs_dinode *inode, uint32_t num, void *buf)
{
    struct ext4_extent_header *extent_header = NULL;
    struct ext4_extent_idx *extent_idx = NULL;
    off_t blk_addr;
    int err = 0;
    void *buf2 = NULL;

    LTRACE_ENTRY;

    /* Extract extents info */
    extent_header = (struct ext4_extent_header *)inode->e2di_blocks;
    if (extent_header->depth > 0) {
        /* Read index node */
        extent_idx =
            (struct ext4_extent_idx *)((uintptr_t)extent_header + sizeof(struct ext4_extent_header));
        blk_addr = extent_idx->leaf_hi;
        blk_addr = ((blk_addr << 32U) | extent_idx->leaf_lo) * E2FS_BLOCK_SIZE(ext2->super_blk);
        blk_addr += ext2->fs_offset;

        buf2 = malloc(E2FS_BLOCK_SIZE(ext2->super_blk));
        if (buf2 == NULL) {
            err = ERR_NO_MEMORY;
            TRACEF("Failed to allocate memory for super block\n");
            goto fail;
        }
        err = tegrabl_blockdev_read(ext2->dev, buf2, blk_addr, E2FS_BLOCK_SIZE(ext2->super_blk));
        if (err != TEGRABL_NO_ERROR) {
            TRACEF("blockdev read failed\n");
            err = ERR_GENERIC;
            goto fail;
        }

        extents_blk_lookup(ext2, (struct ext4_extent_header *)buf2, num, buf);

    } else {
        extents_blk_lookup(ext2, extent_header, num, buf);
    }

fail:
    if (buf2) {
        free(buf2);
    }

    return err;
}

static int ext4_read_extent(ext2_t *ext2, struct ext4_extent_header *extent_header, void *buf,
                            off_t *total_bytes_read)
{
    struct ext4_extent *extent = NULL;
    uint8_t *buf_ptr = NULL;
    off_t blk_addr;
    off_t extent_len_bytes = 0;
    off_t bytes_read = 0;
    uint16_t i;
    off_t data_blk;
    int err = 0;

    LTRACE_ENTRY;

    LTRACEF("Extent: depth: %u, entries: %u\n", extent_header->depth, extent_header->entries);
    buf_ptr = (uint8_t *)buf;
    extent = (struct ext4_extent *)((uintptr_t)extent_header + sizeof(struct ext4_extent_header));

    for (i = 0; i < extent_header->entries; i++) {
        /* Calculate destination address */
        buf_ptr = (uint8_t *)(buf + (extent->block_no * E2FS_BLOCK_SIZE(ext2->super_blk)));
        /* Get block number to which this extent points */
        data_blk = extent->start_hi;
        data_blk = (data_blk << 32U) | extent->start_lo;
        /* Calculate block address */
        blk_addr = data_blk * E2FS_BLOCK_SIZE(ext2->super_blk);
        blk_addr += ext2->fs_offset;
        /* Calculate num of bytes to read */
        extent_len_bytes = extent->len * E2FS_BLOCK_SIZE(ext2->super_blk);
        LTRACEF("entry:%u: start data blk: %lu, addr: 0x%lx, len: %u\n", i, data_blk, blk_addr, extent->len);
        LTRACEF("num bytes to read: %lu, buf ptr: %p\n", extent_len_bytes, buf_ptr);
        err = tegrabl_blockdev_read(ext2->dev, buf_ptr, blk_addr, extent_len_bytes);
        if (err != TEGRABL_NO_ERROR) {
            TRACEF("blockdev read failed\n");
            err = ERR_GENERIC;
            goto fail;
        }
        bytes_read += extent_len_bytes;
        extent++;
    }

    if (total_bytes_read) {
        if (buf_ptr == buf) {
            *total_bytes_read = bytes_read;
        } else {
            *total_bytes_read = ((uintptr_t)buf_ptr + extent_len_bytes) - (uintptr_t)buf;
        }
    }
    LTRACEF("bytes_read %lu\n", *total_bytes_read);

fail:
    return err;
}

static ssize_t ext4_read_data_from_extent(ext2_t *ext2, struct ext2fs_dinode *inode, void *buf)
{
    struct ext4_extent_header *extent_header = NULL;
    struct ext4_extent_idx *extent_idx = NULL;
    off_t blk_addr;
    off_t bytes_read = 0;
    uint16_t i;
    void *buf2;
    uint8_t *buf_ptr = NULL;
    off_t total_bytes_read = 0;
    int err = 0;

    LTRACE_ENTRY;

    if (!validate_extents_magic((struct ext4_extent_header *)inode->e2di_blocks)) {
        TRACEF("Invalid extents magic\n");
        err = ERR_NOT_VALID;
        goto fail;
    }

    buf_ptr = (uint8_t *)buf;

    /* Extract extents info */
    extent_header = (struct ext4_extent_header *)inode->e2di_blocks;
    LTRACEF("Extent: depth: %u, entries: %u\n", extent_header->depth, extent_header->entries);

    if (extent_header->depth > 0) {
        for (i = 0; i < extent_header->entries; i++) {
            /* Read index node */
            extent_idx =
                (struct ext4_extent_idx *)((uintptr_t)extent_header + sizeof(struct ext4_extent_header));
            blk_addr = extent_idx->leaf_hi;
            blk_addr = ((blk_addr << 32U) | extent_idx->leaf_lo) * E2FS_BLOCK_SIZE(ext2->super_blk);
            blk_addr += ext2->fs_offset;

            buf2 = malloc(E2FS_BLOCK_SIZE(ext2->super_blk));
            if (buf2 == NULL) {
                err = ERR_NO_MEMORY;
                TRACEF("Failed to allocate memory for super block\n");
                goto fail;
            }

            err = tegrabl_blockdev_read(ext2->dev, buf2, blk_addr, E2FS_BLOCK_SIZE(ext2->super_blk));
            if (err != TEGRABL_NO_ERROR) {
                TRACEF("blockdev read failed\n");
                err = ERR_GENERIC;
                goto fail;
            }
            err = ext4_read_extent(ext2, (struct ext4_extent_header *)buf2, buf_ptr, &bytes_read);
            total_bytes_read += bytes_read;
            buf_ptr += bytes_read;
        }
    } else {
        /* Read leaf node */
        err = ext4_read_extent(ext2, extent_header, buf_ptr, &bytes_read);
        total_bytes_read += bytes_read;
    }

    LTRACEF("err %d, bytes_read %lu\n", err, total_bytes_read);

fail:
    return err < 0 ? err : (ssize_t)total_bytes_read;
}

/* Read in the dir, look for the entry */
static int lookup_hashed_dir(ext2_t *ext2, struct ext2fs_dinode *dir_inode, const char *name, uint8_t *buf,
                             inodenum_t *inum)
{
    struct ext2fs_dir_entry_2 *ent;
    uint32_t pos;
    uint32_t i;
    uint32_t hash_entries_count;
    uint32_t namelen = strlen(name);
    struct dx_root *root;
    struct dx_entry *entry = NULL;
    off_t entries_start_addr;
    bool file_entry_found = false;
    int err = 0;

    LTRACE_ENTRY;

    /* Get root of hash tree */
    err = get_extents_blk(ext2, dir_inode, 0, (void *)buf);
    if (err != NO_ERROR) {
        goto fail;
    }
    root = (struct dx_root *)buf;
    hash_entries_count = root->count;

    entries_start_addr = ((uintptr_t)buf + sizeof(struct dx_root));

    /* Get hash entries */
    entry = malloc(hash_entries_count * sizeof(struct dx_entry));
    if (entry == NULL) {
        TRACEF("Failed to allocate memory for hash entry\n");
        err = ERR_NO_MEMORY;
        goto fail;
    }
    entry[0].hash = 0;
    entry[0].block = root->block;
    memcpy(&entry[1], (void *)entries_start_addr, (hash_entries_count - 1) * sizeof(struct dx_entry));
    for (i = 0; i < root->count; i++) {
        LTRACEF("#%u: hash: %08x, block: %u\n", i, entry[i].hash, entry[i].block);
    }

    /* TODO: Improve following file lookup logic by using hash */
    /* Look for the file in hash entries block */
    for (i = 0; i < hash_entries_count; i++) {

        LTRACEF("#%i: hash: 0x%08x, blk: %u\n", i, entry[i].hash, entry[i].block);

        /* Get hash entry block */
        memset(buf, 0, E2FS_BLOCK_SIZE(ext2->super_blk));
        err = get_extents_blk(ext2, dir_inode, entry[i].block, (void *)buf);
        if (err != NO_ERROR) {
            goto fail;
        }

        pos = 0;

        while (pos < E2FS_BLOCK_SIZE(ext2->super_blk)) {

            ent = (struct ext2fs_dir_entry_2 *)&buf[pos];
            LTRACEF("inode 0x%x, reclen %d, namelen %d, name: %s\n",
                    LE32(ent->e2d_inode), LE16(ent->e2d_rec_len), ent->e2d_name_len, ent->e2d_name);

            /* Exit if no more file entries are present */
            if (LE16(ent->e2d_rec_len) == 0) {
                LTRACEF("record len 0\n");
                break;
            }

            /* match */
            if (ent->e2d_name_len == namelen && memcmp(name, ent->e2d_name, ent->e2d_name_len) == 0) {
                *inum = LE32(ent->e2d_inode);
                LTRACEF("match: inode %d\n", *inum);
                file_entry_found = true;
                break;
            }

            pos += ROUNDUP(LE16(ent->e2d_rec_len), 4);
        }

        if (file_entry_found) {
            break;
        }
    }

fail:
    if (entry) {
        free(entry);
    }
    return err;
}

static int lookup_linear_dir(ext2_t *ext2, struct ext2fs_dinode *dir_inode, const char *name, uint8_t *buf,
                             inodenum_t *inum)
{
    uint file_blocknum;
    size_t namelen = strlen(name);
    struct ext2fs_dir_entry_2 *ent;
    uint32_t pos;
    bool file_entry_found = false;
    int err;

    LTRACE_ENTRY;

    file_blocknum = 0;
    for (;;) {
        err = ext4_read_data_from_extent(ext2, dir_inode, buf);
        if (err <= 0) {
            return -1;
        }

        /* walk through the directory entries, looking for the one that matches */
        pos = 0;
        while (pos < E2FS_BLOCK_SIZE(ext2->super_blk)) {

            ent = (struct ext2fs_dir_entry_2 *)&buf[pos];
            LTRACEF("ent %d:%d: inode 0x%x, reclen %d, namelen %d, name: %s\n",
                    file_blocknum, pos, LE32(ent->e2d_inode), LE16(ent->e2d_rec_len), ent->e2d_name_len, ent->e2d_name);

            /* Exit if no more file entries are present */
            if (LE16(ent->e2d_rec_len) == 0) {
                LTRACEF("record len 0\n");
                break;
            }

            /* match */
            if (ent->e2d_name_len == namelen && memcmp(name, ent->e2d_name, ent->e2d_name_len) == 0) {
                *inum = LE32(ent->e2d_inode);
                LTRACEF("match: inode %d\n", *inum);
                file_entry_found = true;
                break;
            }

            pos += ROUNDUP(LE16(ent->e2d_rec_len), 4);
        }

        if (file_entry_found) {
            break;
        }

        file_blocknum++;

        /* sanity check the directory. 4MB should be enough */
        if (file_blocknum > 1024) {
            TRACEF("Invalid file block num\n");
            return -1;
        }
    }

    return err;
}

int ext4_dir_lookup(ext2_t *ext2, struct ext2fs_dinode *dir_inode, const char *name, inodenum_t *inum)
{
    uint8_t *buf;
    int err = 0;

    if (!S_ISDIR(dir_inode->e2di_mode)) {
        TRACEF("Not a directory\n");
        err = ERR_NOT_DIR;
        goto fail;
    }

    if (!validate_extents_magic((struct ext4_extent_header *)dir_inode->e2di_blocks)) {
        TRACEF("Invalid extents magic\n");
        err = ERR_NOT_VALID;
        goto fail;
    }

    buf = malloc(E2FS_BLOCK_SIZE(ext2->super_blk));
    if (buf == NULL) {
        TRACEF("Failed to allocate memory for super block\n");
        err = ERR_NO_MEMORY;
        goto fail;
    }

    /* Get root of hash tree */
    if (IS_HASHED_INDEX(dir_inode->e2di_flags)) {
        err = lookup_hashed_dir(ext2, dir_inode, name, buf, inum);
    } else {
        err = lookup_linear_dir(ext2, dir_inode, name, buf, inum);
    }

    free(buf);

fail:
    return err;
}

status_t ext4_mount(struct tegrabl_bdev *dev, uint64_t start_sector, fscookie **cookie)
{
    off_t fs_offset;
    uint32_t gd_size;
    struct ext2_block_group_desc *grp_desc;
    int i;
    int err = 0;
    tegrabl_error_t error;

    LTRACEF("dev %p\n", dev);

    if (!dev) {
        return ERR_NOT_FOUND;
    }

    ext2_t *ext2 = malloc(sizeof(ext2_t));
    if (ext2 == NULL) {
        TRACEF("Failed to allocate memory for ext2 priv data object\n");
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
    if (ext2->super_blk.e2fs_magic != E4FS_MAGIC) {
        err = -1;
        return err;
    }

    /* calculate group count, rounded up */
    ext2->group_count =
        (ext2->super_blk.e2fs_bcount + ext2->super_blk.e2fs_bpg - 1) / ext2->super_blk.e2fs_bpg;

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
    if (ext2->super_blk.e2fs_features_rocompat &
            ~(EXT2F_ROCOMPAT_SPARSESUPER     |
              EXT2F_ROCOMPAT_LARGEFILE       |
              EXT2F_ROCOMPAT_HUGE_FILE       |
              EXT2F_ROCOMPAT_GDT_CSUM        |
              EXT2F_ROCOMPAT_DIR_NLINK       |
              EXT2F_ROCOMPAT_EXTRA_ISIZE     |
              EXT2F_ROCOMPAT_METADATA_CKSUM)) {
        TRACEF("Unsupported ro features, 0x%08x\n", ext2->super_blk.e2fs_features_rocompat);
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
        TRACEF("Failed to allocate memory for group descriptor\n");
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
    for (i = 0; i < ext2->group_count; i++) {
        ext2_endian_swap_group_desc((struct ext2_block_group_desc *)grp_desc);
        LTRACEF("group %d:\n", i);
        if (ext2->super_blk.e3fs_desc_size > 32) {
            LTRACEF("\tblock bitmap lo %u, hi %u\n",
                    grp_desc->ext2bgd_b_bitmap, grp_desc->ext4bgd_b_bitmap_hi);
            LTRACEF("\tinode bitmap lo %u, hi %u\n",
                    grp_desc->ext2bgd_i_bitmap, grp_desc->ext4bgd_i_bitmap_hi);
            LTRACEF("\tinode table  lo %u, hi %u\n",
                    grp_desc->ext2bgd_i_tables, grp_desc->ext4bgd_i_tables_hi);
            LTRACEF("\tfree blocks  lo %u, hi %u\n",
                    grp_desc->ext2bgd_nbfree, grp_desc->ext4bgd_nbfree_hi);
            LTRACEF("\tfree inodes  lo %u, hi %u\n",
                    grp_desc->ext2bgd_nifree, grp_desc->ext4bgd_nifree_hi);
            LTRACEF("\tused dirs    lo %u, hi %u\n",
                    grp_desc->ext2bgd_ndirs, grp_desc->ext4bgd_ndirs_hi);
        } else {
            LTRACEF("\tblock bitmap %u\n", grp_desc->ext2bgd_b_bitmap);
            LTRACEF("\tinode bitmap %u\n", grp_desc->ext2bgd_i_bitmap);
            LTRACEF("\tinode table  %u\n", grp_desc->ext2bgd_i_tables);
            LTRACEF("\tfree blocks  %u\n", grp_desc->ext2bgd_nbfree);
            LTRACEF("\tfree inodes  %u\n", grp_desc->ext2bgd_nifree);
            LTRACEF("\tused dirs    %u\n", grp_desc->ext2bgd_ndirs);
        }
        grp_desc = (struct ext2_block_group_desc *)((uintptr_t)grp_desc + gd_size);
    }

    /* initialize the block cache */
    ext2->cache = bcache_create(ext2->dev, E2FS_BLOCK_SIZE(ext2->super_blk), 4, fs_offset);
	if (ext2->cache == NULL) {
		err = ERR_GENERIC;
		goto err;
	}

    /* load the first inode */
    err = ext2_load_inode(ext2, EXT2_ROOTINO, &ext2->root_inode);
    if (err < 0) {
        goto err;
    }

    ext2->fs_offset = fs_offset;
    *cookie = (fscookie *)ext2;

    return 0;

err:
    LTRACEF("exiting with err code %d\n", err);
    free(ext2);

    return err;
}

int ext4_open_file(fscookie *cookie, const char *path, filecookie **fcookie)
{
    ext2_t *ext2 = (ext2_t *)cookie;
    ext2_file_t *file;
    inodenum_t inum = 0;
    int err = 0;

   err = ext2_lookup(ext2, path, &inum);
    if (err < 0) {
        TRACEF("'%s' lookup failed\n", path);
        goto fail;
    }

    /* create the file object */
    file = malloc(sizeof(ext2_file_t));
    if (file == NULL) {
        TRACEF("Failed to allocate memory for file object\n");
        err = ERR_NO_MEMORY;
        goto fail;
    }
    memset(file, 0, sizeof(ext2_file_t));

    /* Read in the file inode */
    err = ext2_load_inode(ext2, inum, &file->inode);
    if (err < 0) {
        TRACEF("Failed to load inode\n");
        free(file);
        goto fail;
    }

    file->ext2 = ext2;
    *fcookie = (filecookie *)file;

fail:
    return err;
}

ssize_t ext4_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len)
{
    ext2_file_t *file = NULL;

    LTRACEF("\n");
    file = (ext2_file_t *)fcookie;

    /* Test that it's a file */
    if (!S_ISREG(file->inode.e2di_mode)) {
        TRACEF("not a file, mode: 0x%04x\n", file->inode.e2di_mode);
        return -1;
    }

    return ext4_read_data_from_extent(file->ext2, &file->inode, buf);
}

static const struct fs_api ext4_api = {
    .mount = ext4_mount,
    .unmount = ext2_unmount,
    .open = ext4_open_file,
    .stat = ext2_stat_file,
    .read = ext4_read_file,
    .close = ext2_close_file,
};

STATIC_FS_IMPL(ext4, &ext4_api);
