/*
 * Copyright (c) 2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __EXT4_PRIV_H
#define __EXT4_PRIV_H

#include <ext2_dinode.h>
#include <ext2_priv.h>

/**
 * @brief Mount ext4 filesystem
 *
 * @param dev Handle of the storage device
 * @param start_sector Start of the filesystem
 * @param cookie Filesystem cookie
 *
 * @return returns 0 for no error, otherwise appropriate error code
 */
status_t ext4_mount(struct tegrabl_bdev *dev, uint64_t start_sector, fscookie **cookie);

/**
 * @brief Open file present in ext4 filesystem
 *
 * @param cookie Filesystem cookie
 * @param path Filepath
 * @param fcookie File cookie
 *
 * @return returns 0 for no error, otherwise appropriate error code
 */
status_t ext4_open_file(fscookie *cookie, const char *path, filecookie **fcookie);

/**
 * @brief Read file present in ext4 filesystem
 *
 * @param fcookie File cookie
 * @param buf Buffer address to copy the file
 * @param offset Offset
 * @param len Length of the file
 *
 * @return returns error code in case of failure or number of bytes read on success
 */
ssize_t ext4_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len);

/**
 * @brief Recursively look for a file in the directory
 *
 * @param ext2/4 private structure
 * @param dir_inode Inode from which to start looking for the file
 * @param name Filename to look for
 * @param inum return file's inode
 *
 * @return returns 0 for no error, otherwise appropriate error code
 */
int ext4_dir_lookup(ext2_t *ext2, struct ext2fs_dinode *dir_inode, const char *name, inodenum_t *inum);

#endif
