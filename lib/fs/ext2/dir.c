/*
 * Copyright (c) 2007 Travis Geiselbrecht
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
#include <trace.h>
#include <err.h>
#include <ext2_dinode.h>
#include <ext2_dir.h>
#include <ext2_priv.h>
#include <ext4_priv.h>

#define LOCAL_TRACE 0

/* read in the dir, look for the entry */
static int ext2_dir_lookup(ext2_t *ext2, struct ext2fs_dinode *dir_inode, const char *name, inodenum_t *inum)
{
    uint file_blocknum;
    int err;
    uint8_t *buf;
    size_t namelen = strlen(name);

    if (!S_ISDIR(dir_inode->e2di_mode)) {
        TRACEF("Not a directory\n");
        return ERR_NOT_DIR;
    }

    buf = malloc(E2FS_BLOCK_SIZE(ext2->super_blk));
    if (buf == NULL) {
        TRACEF("Failed to allocate mmeory for super block\n");
        return ERR_NO_MEMORY;
    }

    file_blocknum = 0;
    for (;;) {
        /* read in the offset */
        err = ext2_read_inode(ext2, dir_inode, buf, file_blocknum * E2FS_BLOCK_SIZE(ext2->super_blk),
                              E2FS_BLOCK_SIZE(ext2->super_blk));
        if (err <= 0) {
            free(buf);
            return -1;
        }

        /* walk through the directory entries, looking for the one that matches */
        struct ext2fs_dir_entry_2 *ent;
        uint pos = 0;
        while (pos < E2FS_BLOCK_SIZE(ext2->super_blk)) {
            ent = (struct ext2fs_dir_entry_2 *)&buf[pos];
            LTRACEF("ent %d:%d: inode 0x%x, reclen %d, namelen %d, name: %s\n",
                    file_blocknum, pos, LE32(ent->e2d_inode), LE16(ent->e2d_rec_len), ent->e2d_name_len,
                    ent->e2d_name);

            /* sanity check the record length */
            if (LE16(ent->e2d_rec_len) == 0) {
                LTRACEF("record len 0\n");
                break;
            }

            if (ent->e2d_name_len == namelen && memcmp(name, ent->e2d_name, ent->e2d_name_len) == 0) {
                // match
                *inum = LE32(ent->e2d_inode);
                LTRACEF("match: inode %d\n", *inum);
                free(buf);
                return 1;
            }

            pos += ROUNDUP(LE16(ent->e2d_rec_len), 4);
        }

        file_blocknum++;

        /* sanity check the directory. 4MB should be enough */
        if (file_blocknum > 1024) {
            TRACEF("Invalid file block num\n");
            free(buf);
            return -1;
        }
    }
}

/* note, trashes path */
static int ext2_walk(ext2_t *ext2, char *path, struct ext2fs_dinode *start_inode, inodenum_t *inum, int recurse)
{
    char *ptr;
    struct ext2fs_dinode inode;
    struct ext2fs_dinode dir_inode;
    int err;
    bool done;

    LTRACEF("path '%s', start_inode %p, inum %p, recurse %d\n", path, start_inode, inum, recurse);

    if (recurse > 4)
        return ERR_RECURSE_TOO_DEEP;

    /* chew up leading slashes */
    ptr = &path[0];
    while (*ptr == '/')
        ptr++;

    done = false;
    memcpy(&dir_inode, start_inode, sizeof(struct ext2fs_dinode));
    while (!done) {
        /* process the first component */
        char *next_sep = strchr(ptr, '/');
        if (next_sep) {
            /* terminate the next component, giving us a substring */
            *next_sep = 0;
        } else {
            /* this is the last component */
            done = true;
        }

        LTRACEF("component '%s', done %d\n", ptr, done);
        LTRACEF("inode flags: 0x%08x\n", dir_inode.e2di_flags);

        /* do the lookup on this component */
        if (IS_EXTENTS(dir_inode.e2di_flags)) {
            err = ext4_dir_lookup(ext2, &dir_inode, ptr, inum);
        } else {
            err = ext2_dir_lookup(ext2, &dir_inode, ptr, inum);
        }
        if (err < 0) {
            TRACEF("'%s' lookup failed\n", ptr);
            return err;
        }

nextcomponent:
        LTRACEF("inum %u\n", *inum);

        /* load the next inode */
        err = ext2_load_inode(ext2, *inum, &inode);
        if (err < 0)
            return err;

        /* is it a symlink? */
        if (S_ISLNK(inode.e2di_mode)) {
            char link[512];

            LTRACEF("hit symlink\n");

            err = ext2_read_link(ext2, &inode, link, sizeof(link));
            if (err < 0)
                return err;
            else {
                /* move to the next separator */
                ptr = next_sep + 1;

                /* consume multiple separators */
                while (*ptr == '/')
                    ptr++;
            }

            LTRACEF("symlink read returns %d '%s'\n", err, link);

            /* recurse, parsing the link */
            if (link[0] == '/') {
                /* link starts with '/', so start over again at the rootfs */
                err = ext2_walk(ext2, ptr, &ext2->root_inode, inum, recurse + 1);
            } else {
                err = ext2_walk(ext2, ptr, &dir_inode, inum, recurse + 1);
            }

            LTRACEF("recursive walk returns %d\n", err);

            if (err < 0)
                return err;
            else
                done = true;

            /* if we weren't done with our path parsing, start again with the result of this recurse */
            if (!done) {
                goto nextcomponent;
            }
        } else if (S_ISDIR(inode.e2di_mode)) {
            /* for the next cycle, point the dir inode at our new directory */
            memcpy(&dir_inode, &inode, sizeof(struct ext2fs_dinode));
        } else {
            if (!done) {
                /* we aren't done and this walked over a nondir, abort */
                LTRACEF("not finished and component is nondir\n");
                return ERR_NOT_FOUND;
            }
        }

        if (!done) {
            /* move to the next separator */
            ptr = next_sep + 1;

            /* consume multiple separators */
            while (*ptr == '/')
                ptr++;
        }
    }

    return 0;
}

/* do a path parse, looking up each component */
int ext2_lookup(ext2_t *ext2, const char *_path, inodenum_t *inum)
{
    LTRACEF("path '%s', inum %p\n", _path, inum);

    char path[512];
    strlcpy(path, _path, sizeof(path));

    return ext2_walk(ext2, path, &ext2->root_inode, inum, 1);
}

