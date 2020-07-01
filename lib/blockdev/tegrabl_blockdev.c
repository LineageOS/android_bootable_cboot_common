/*
 * Copyright (c) 2009 Travis Geiselbrecht
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

/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_BLOCK_DEV

#include "build_config.h"
#include <list.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_blockdev_local.h>
#include <tegrabl_malloc.h>
#include <tegrabl_debug.h>
#include <tegrabl_utils.h>
#include <tegrabl_error.h>
#include <tegrabl_compiler.h>

static struct tegrabl_bdev_struct *bdevs;

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static uint32_t	xfer_id;
#endif

static inline bool tegrabl_blockdev_buffer_aligned(tegrabl_bdev_t *dev, const void *buf)
{
	TEGRABL_ASSERT(dev != NULL);

	return MOD_POW2((uintptr_t)buf, dev->buf_align_size) == 0UL;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

static tegrabl_error_t tegrabl_blockdev_default_read(tegrabl_bdev_t *dev,
	void *buffer, off_t offset, off_t len)
{
	uint8_t *buf = (uint8_t *)buffer;
	size_t bytes_read = 0;
	bnum_t block;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	void *temp_buf = NULL;
	void *partial_block_buf = NULL;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (MOD_LOG2(offset, dev->block_size_log2) ||
		MOD_LOG2(len, dev->block_size_log2)) {
		partial_block_buf = tegrabl_alloc_align(TEGRABL_HEAP_DMA, TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE,
												TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
		if (partial_block_buf == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}
	}

	/* find the starting block */
	block = DIV_FLOOR_LOG2(offset, dev->block_size_log2);

	pr_trace("buf %p, offset %" PRIu64", block %u, len %"PRIu64"\n",
			 buf, offset, block, len);

	/* handle partial first block */
	if (MOD_LOG2(offset, dev->block_size_log2) != 0ULL) {
		/* read in the block */
		error = tegrabl_blockdev_read_block(dev, partial_block_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* copy what we need */
		size_t block_offset = MOD_LOG2(offset, dev->block_size_log2);
		size_t tocopy = MIN(TEGRABL_BLOCKDEV_BLOCK_SIZE(dev) -
							block_offset, len);
		memcpy(buf, (uint8_t *)partial_block_buf + block_offset, tocopy);

		/* increment our buffers */
		buf += tocopy;
		len -= tocopy;
		bytes_read += tocopy;
		block++;
	}

	pr_trace("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle middle blocks */
	if (len >= (1U << dev->block_size_log2)) {
		/* do the middle reads */
		size_t block_count = DIV_FLOOR_LOG2(len, dev->block_size_log2);
		size_t bytes = block_count << dev->block_size_log2;
		if (bytes > len) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			goto fail;
		}

		if (!tegrabl_blockdev_buffer_aligned(dev, buf)) {
			temp_buf = tegrabl_alloc_align(TEGRABL_HEAP_DMA, TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE,
										   block_count * TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
			if (temp_buf == NULL) {
				error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
				goto fail;
			}
			error = tegrabl_blockdev_read_block(dev, temp_buf, block, block_count);
			if (error != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(error);
				goto fail;
			}
			memcpy(buf, temp_buf, bytes);
		} else {
			error = tegrabl_blockdev_read_block(dev, buf, block, block_count);
			if (error != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(error);
				goto fail;
			}
		}

		/* increment our buffers */
		buf += bytes;
		len -= bytes;
		bytes_read += bytes;
		block += block_count;
	}

	pr_trace("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle partial last block */
	if (len > 0ULL) {
		/* read the block */
		error = tegrabl_blockdev_read_block(dev, partial_block_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* copy the partial block from our temp_buf buffer */
		memcpy(buf, partial_block_buf, len);
		bytes_read += len;
	}

fail:
	if (partial_block_buf != NULL) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, partial_block_buf);
	}
	if (temp_buf != NULL) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, temp_buf);
	}
	return error;
}

static tegrabl_error_t tegrabl_blockdev_default_write(tegrabl_bdev_t *dev,
	const void *buffer, off_t offset, off_t len)
{
	const uint8_t *buf = (const uint8_t *)buffer;
	size_t bytes_written = 0;
	bnum_t block;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	void *temp_buf = NULL;
	void *partial_block_buf = NULL;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	if ((MOD_LOG2(offset, dev->block_size_log2) != 0U) ||
		(MOD_LOG2(len, dev->block_size_log2) != 0U)) {
		partial_block_buf = tegrabl_alloc_align(TEGRABL_HEAP_DMA, TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE,
												TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
		if (partial_block_buf == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}
	}

	/* find the starting block */
	block = DIV_FLOOR_LOG2(offset, dev->block_size_log2);

	pr_trace("buf %p, offset %" PRIu64", block %u, len %"PRIu64"\n",
			 buf, offset, block, len);

	/* handle partial first block */
	if (MOD_LOG2(offset, dev->block_size_log2) != 0U) {
		/* read in the block */
		error = tegrabl_blockdev_read_block(dev, partial_block_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* copy what we need */
		size_t block_offset = MOD_LOG2(offset, dev->block_size_log2);
		size_t tocopy = MIN(TEGRABL_BLOCKDEV_BLOCK_SIZE(dev) -
							block_offset, len);
		memcpy((uint8_t *)partial_block_buf + block_offset, buf, tocopy);

		/* write it back out */
		tegrabl_blockdev_write_block(dev, partial_block_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* increment our buffers */
		buf += tocopy;
		len -= tocopy;
		bytes_written += tocopy;
		block++;
	}

	pr_trace("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle middle blocks */
	if (len >= (1U << dev->block_size_log2)) {
		/* do the middle writes */
		size_t block_count = DIV_FLOOR_LOG2(len, dev->block_size_log2);
		size_t bytes = block_count << dev->block_size_log2;
		if (bytes > len) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
			goto fail;
		}
		if (!tegrabl_blockdev_buffer_aligned(dev, buf)) {
			temp_buf = tegrabl_alloc_align(TEGRABL_HEAP_DMA, TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE,
										   block_count * TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
			if (temp_buf == NULL) {
				error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
				goto fail;
			}
			memcpy(temp_buf, buf, bytes);
			error = tegrabl_blockdev_write_block(dev, temp_buf, block, block_count);
		} else {
			error = tegrabl_blockdev_write_block(dev, buf, block, block_count);
		}

		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* increment our buffers */
		buf += bytes;
		len -= bytes;
		bytes_written += bytes;
		block += block_count;
	}

	pr_trace("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle partial last block */
	if (len > 0ULL) {
		/* read the block */
		error = tegrabl_blockdev_read_block(dev, partial_block_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		/* copy the partial block from our temp_buf buffer */
		memcpy(partial_block_buf, buf, len);

		/* write it back out */
		error = tegrabl_blockdev_write_block(dev, partial_block_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		bytes_written += len;
	}

fail:
	if (temp_buf != NULL) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, temp_buf);
	}
	if (partial_block_buf != NULL) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, partial_block_buf);
	}
	/* return error or bytes written */
	return error;
}

static tegrabl_error_t tegrabl_blockdev_default_erase(tegrabl_bdev_t *dev,
	bnum_t block, bnum_t count, bool is_secure)
{
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);
	TEGRABL_UNUSED(is_secure);

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

static tegrabl_error_t tegrabl_blockdev_default_read_block(tegrabl_bdev_t *dev,
	void *buf, bnum_t block, bnum_t count)
{
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(buf);
	pr_debug("default StartBlock= %d NumofBlock = %u\n", block, count);

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
}

static tegrabl_error_t tegrabl_blockdev_default_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count)
{
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(buf);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
}
#endif

static void bdev_inc_ref(tegrabl_bdev_t *dev)
{
	dev->ref += 1U;
}

static tegrabl_error_t bdev_dec_ref(tegrabl_bdev_t *dev)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t oldval = dev->ref;

	if (dev->ref > 0U) {
		dev->ref -= 1U;
	}

	pr_debug("%d: oldval = %d\n", dev->device_id, oldval);
	if (oldval == 1UL) {
		/* last ref, remove it */
		if (list_in_list(&dev->node)) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
			goto fail;
		}

		pr_debug("last ref, removing (%d)\n", dev->device_id);

		/* call the close hook if it exists */
		if (dev->close != NULL)
			dev->close(dev);

		tegrabl_free(dev);
	}
fail:
	return error;
}

tegrabl_bdev_t *tegrabl_blockdev_open(tegrabl_storage_type_t storage_type,
		uint32_t instance)
{
	tegrabl_bdev_t *bdev = NULL;
	tegrabl_bdev_t *entry;
	uint32_t device_id;

	if (storage_type >= TEGRABL_STORAGE_INVALID) {
		pr_error("Invalid storage type = %d, instance = %d\n", storage_type,
			instance);
		bdev = NULL;
		goto fail;
	}

	device_id = TEGRABL_BLOCK_DEVICE_ID(storage_type, instance);

	/* see if it's in our list */
	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		if (entry->ref <= 0U) {
			bdev = NULL;
			goto fail;
		}
		if (entry->device_id == device_id) {
			bdev = entry;
			bdev_inc_ref(bdev);
			break;
		}
	}

fail:
	if (bdev == NULL)
		pr_error("Blockdev open: exit error\n");

	return bdev;
}

tegrabl_bdev_t *tegrabl_blockdev_next_device(tegrabl_bdev_t *curr_bdev)
{
	tegrabl_bdev_t *next_bdev = NULL;
	struct list_node *next_node;

	/* If it is first registered device, get from the tail, otherwise
	 * move to head by getting the prev list.
	 */
	if (curr_bdev == NULL)
		next_node = list_peek_head(&bdevs->list);
	else
		next_node = list_next(&bdevs->list, &curr_bdev->node);

	if (next_node != NULL) {
		next_bdev = containerof(next_node, tegrabl_bdev_t, node);
	}

	return next_bdev;
}

tegrabl_error_t tegrabl_blockdev_close(tegrabl_bdev_t *dev)
{
	if (dev == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
	}

	pr_debug("Device id = %x\n", dev->device_id);

	bdev_dec_ref(dev);
	return TEGRABL_NO_ERROR;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

void tegrabl_blockdev_list_kpi(void)
{
	list_kpi(bdevs);
}

tegrabl_error_t tegrabl_blockdev_read(tegrabl_bdev_t *dev, void *buf,
	off_t offset, off_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (len == 0ULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
		goto fail;
	}

	if (dev->ref <= 0UL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}

	pr_trace("dev '%d', dev size %"PRIu64", buf %p, offset %"PRIu64", len %"PRIu64"\n",
			 dev->device_id, dev->size, buf, offset, len);

	if ((offset >= dev->size) || ((offset + len) > dev->size)) {
		pr_error("Either offset or requested data size from that offset is beyond device size\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	error = dev->read(dev, buf, offset, len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	 if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev read: exit error = %x\n", error);
	}
	return error;
}
#endif

tegrabl_error_t tegrabl_blockdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (count == 0U)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	profile_read_start(dev);
#endif

	if (!tegrabl_blockdev_buffer_aligned(dev, buf)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 25);
		goto fail;
	}

	pr_trace("dev '%d', buf %p, block %d, count %u\n", dev->device_id, buf, block, count);

	if (dev->ref <= 0U) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 9);
		goto fail;
	}

	/* range check */
	if ((block > dev->block_count) || ((block + count) > dev->block_count)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 1);
		goto fail;
	}

	error = dev->read_block(dev, buf, block, count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	profile_read_end(dev, count * (1 << dev->block_size_log2));
#endif

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev read block: exit error = %x\n", error);
	}
	return error;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
tegrabl_error_t tegrabl_blockdev_write(tegrabl_bdev_t *dev, const void *buf,
	off_t offset, off_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (len == 0ULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 10);
		goto fail;
	}

	pr_trace("dev '%d', buf %p, offset %"PRIu64", len %"PRIu64"\n",
			 dev->device_id, buf, offset, len);

	if (dev->ref <= 0UL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 11);
		goto fail;
	}

	if ((offset >= dev->size) || ((offset + len) > dev->size)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 2);
		goto fail;
	}

	error = dev->write(dev, buf, offset, len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev write: exit error = %x\n", error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((count == 0UL) || (dev == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	profile_write_start(dev);
#endif

	if (!tegrabl_blockdev_buffer_aligned(dev, buf)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 26);
		goto fail;
	}

	pr_trace("dev '%d', buf %p, block %d, count %u\n", dev->device_id, buf,
			 block, count);

	/* range check */
	if ((block > dev->block_count) || ((block + count) > dev->block_count)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 3);
		goto fail;
	}

	if (dev->ref <= 0UL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 13);
		goto fail;
	}

	error = dev->write_block(dev, buf, block, count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	profile_write_end(dev, count * (1 << dev->block_size_log2));
#endif

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev write block: exit error = %x\n", error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (count == 0UL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 14);
		goto fail;
	}

	pr_trace("dev '%d', block %u, count %u\n", dev->device_id, block, count);

	/* range check */
	if ((block > dev->block_count) || ((block + count) > dev->block_count)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 4);
		goto fail;
	}

	if (dev->ref <= 0UL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 15);
		goto fail;
	}

	error = dev->erase(dev, block, count, is_secure);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	 if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev erase: exit error = %x\n", error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_erase_all(tegrabl_bdev_t *dev, bool is_secure)
{
	if (dev == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 16);
	}

	return tegrabl_blockdev_erase(dev, 0, dev->block_count, is_secure);
}

tegrabl_error_t tegrabl_blockdev_xfer(struct tegrabl_blockdev_xfer_info *xfer)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_bdev *dev;

	if ((xfer == NULL) || (xfer->dev == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 22);
		goto fail;
	}

	dev = xfer->dev;

	pr_trace("dev '%d', buf %p, block %d, count %u\n", dev->device_id, xfer->buf, xfer->start_block,
			 xfer->block_count);

	/* range check */
	if ((xfer->start_block > dev->block_count) ||
			((xfer->start_block + xfer->block_count) > dev->block_count)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 5);
		goto fail;
	}

	if (dev->ref <= 0UL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 23);
		goto fail;
	}

	/* TODO - maintain xfer list and check if any previous xfer pending */

	error = dev->xfer(xfer);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	xfer->id = xfer_id++;

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev xfer: exit error = %x\n", error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout,
	uint8_t *status_flag)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_bdev *dev;

	if ((xfer == NULL) || (xfer->dev == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 24);
		goto fail;
	}

	dev = xfer->dev;

	error = dev->xfer_wait(xfer, timeout, status_flag);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	return error;
}
#endif

tegrabl_error_t tegrabl_blockdev_ioctl(tegrabl_bdev_t *dev, uint32_t ioctl,
	void *args)
{
	size_t *bsize;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (ioctl >= TEGRABL_IOCTL_INVALID) || (args == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 17);
		goto fail;
	}

	pr_trace("dev '%d', request %08x, argp %p\n", dev->device_id, ioctl, args);

	if (ioctl == TEGRABL_IOCTL_BLOCK_SIZE) {
		bsize = (size_t *)args;
		*bsize = TEGRABL_BLOCKDEV_BLOCK_SIZE(dev);
	} else {
		error = dev->ioctl(dev, ioctl, args);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev IOCTL: exit error = %x\n", error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_initialize_bdev(tegrabl_bdev_t *dev,
	uint32_t device_id, uint32_t block_size_log2, bnum_t block_count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 18);
		goto fail;
	}

	list_clear_node(&dev->node);
	dev->device_id = device_id;
	dev->block_size_log2 = block_size_log2;
	dev->block_count = block_count;
	dev->size = (off_t)block_count << block_size_log2;
	dev->ref = 0;

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	/* set up the default hooks, the sub driver should override the block
	 * operations at least
	 */
	dev->read = tegrabl_blockdev_default_read;
	dev->read_block = tegrabl_blockdev_default_read_block;
	dev->write = tegrabl_blockdev_default_write;
	dev->write_block = tegrabl_blockdev_default_write_block;
	dev->erase = tegrabl_blockdev_default_erase;
	dev->close = NULL;
#endif

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Blockdev initialize bdev: exit error = %x\n", error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_register_device(tegrabl_bdev_t *dev)
{
	tegrabl_bdev_t *entry;
	bool duplicate = false;

	if (dev == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 20);
	}

	pr_trace("Blockdev device id = %x\n", dev->device_id);

	bdev_inc_ref(dev);

	/* Check if duplicate */
	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		if (entry->device_id == dev->device_id) {
			pr_debug("Block device %x already registered\n", entry->device_id);
			duplicate = true;
			break;
		}
	}
	if (!duplicate)
		list_add_tail(&bdevs->list, &dev->node);

	return (duplicate) ? TEGRABL_ERROR(TEGRABL_ERR_ALREADY_EXISTS, 0) :
			TEGRABL_NO_ERROR;

}

tegrabl_error_t tegrabl_blockdev_unregister_device(tegrabl_bdev_t *dev)
{
	if (dev == NULL)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 21);

	pr_trace("Blockdev device id = %x\n", dev->device_id);

	/* remove it from the list  */
	list_delete(&dev->node);

	/* remove the ref the list used to have */
	bdev_dec_ref(dev);

	return TEGRABL_NO_ERROR;
}

void tegrabl_blockdev_dump_devices(void)
{
	pr_debug("Block devices:\n");
	tegrabl_bdev_t *entry;
	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		pr_debug("\t%d, size %"PRIu64", bsize %lu, ref %d\n", entry->device_id,
				 entry->size, TEGRABL_BLOCKDEV_BLOCK_SIZE(entry), entry->ref);
	}
}

const char *tegrabl_blockdev_get_name(tegrabl_storage_type_t type)
{
	const char * const storage_name[TEGRABL_STORAGE_MAX] = {
		[TEGRABL_STORAGE_SDMMC_BOOT] = "SDMMC_BOOT",
		[TEGRABL_STORAGE_SDMMC_USER] = "SDMMC_USER",
		[TEGRABL_STORAGE_SDMMC_RPMB] = "SDMMC_RPMB",
		[TEGRABL_STORAGE_QSPI_FLASH] = "QSPI_FLASH",
		[TEGRABL_STORAGE_SATA] = "SATA",
		[TEGRABL_STORAGE_SDCARD] = "SDCARD",
		[TEGRABL_STORAGE_USB_MS] = "USB_MS",
		[TEGRABL_STORAGE_UFS] = "UFS",
		[TEGRABL_STORAGE_UFS_USER] = "UFS_USER",
		[TEGRABL_STORAGE_UFS_RPMB] = "UFS_RPMB",
	};

	TEGRABL_COMPILE_ASSERT(ARRAY_SIZE(storage_name) == TEGRABL_STORAGE_MAX, "missing storage-type in array");
	if (type >= TEGRABL_STORAGE_MAX) {
		return NULL;
	}
	return storage_name[type];
}

tegrabl_error_t tegrabl_blockdev_init(void)
{
	if (bdevs != NULL) {
		pr_debug("Block dev already initialized:\n");
		return TEGRABL_NO_ERROR;
	} else {
		bdevs = tegrabl_malloc(sizeof(*bdevs));
		if (bdevs == NULL) {
			return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		}
	}

	list_initialize(&bdevs->list);

	return TEGRABL_NO_ERROR;
}
