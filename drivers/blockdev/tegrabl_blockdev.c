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
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_blockdev_profiling.h>
#include <tegrabl_malloc.h>
#include <tegrabl_debug.h>
#include <tegrabl_utils.h>
#include <tegrabl_error.h>

static struct tegrabl_bdev_struct *bdevs;

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static void *temp_buf;
#endif

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

static tegrabl_error_t tegrabl_blockdev_default_read(tegrabl_bdev_t *dev,
	void *buffer, off_t offset, off_t len)
{
	uint8_t *buf = (uint8_t *)buffer;
	size_t bytes_read = 0;
	bnum_t block;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (MOD_LOG2(offset, dev->block_size_log2) ||
		MOD_LOG2(len, dev->block_size_log2)) {
		if (!temp_buf) {
			temp_buf = tegrabl_alloc(TEGRABL_HEAP_DMA,
									 TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
			if (!temp_buf) {
				error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				goto fail;
			}
		}
	}

	/* find the starting block */
	block = DIV_FLOOR_LOG2(offset, dev->block_size_log2);

	pr_debug("buf %p, offset %" PRIu64", block %u, len %"PRIu64"\n",
		buf, offset, block, len);

	/* handle partial first block */
	if (MOD_LOG2(offset, dev->block_size_log2) != 0) {
		/* read in the block */
		error = tegrabl_blockdev_read_block(dev, temp_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* copy what we need */
		size_t block_offset = MOD_LOG2(offset, dev->block_size_log2);
		size_t tocopy = MIN(TEGRABL_BLOCKDEV_BLOCK_SIZE(dev) -
							block_offset, len);
		memcpy(buf, (uint8_t *)temp_buf + block_offset, tocopy);

		/* increment our buffers */
		buf += tocopy;
		len -= tocopy;
		bytes_read += tocopy;
		block++;
	}

	pr_debug("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle middle blocks */
	if (len >= (1U << dev->block_size_log2)) {
		/* do the middle reads */
		size_t block_count = DIV_FLOOR_LOG2(len, dev->block_size_log2);

		error = tegrabl_blockdev_read_block(dev, buf, block, block_count);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* increment our buffers */
		size_t bytes = block_count << dev->block_size_log2;
		if (bytes > len) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			goto fail;
		}

		buf += bytes;
		len -= bytes;
		bytes_read += bytes;
		block += block_count;
	}

	pr_debug("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle partial last block */
	if (len > 0) {
		/* read the block */
		error = tegrabl_blockdev_read_block(dev, temp_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* copy the partial block from our temp_buf buffer */
		memcpy(buf, temp_buf, len);

		bytes_read += len;
	}

fail:
	return error;
}

static tegrabl_error_t tegrabl_blockdev_default_write(tegrabl_bdev_t *dev,
	const void *buffer, off_t offset, off_t len)
{
	const uint8_t *buf = (const uint8_t *)buffer;
	size_t bytes_written = 0;
	bnum_t block;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	if (MOD_LOG2(offset, dev->block_size_log2) ||
		MOD_LOG2(len, dev->block_size_log2)) {
		if (!temp_buf) {
			temp_buf = tegrabl_alloc(TEGRABL_HEAP_DMA,
									 TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
			if (!temp_buf) {
				error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				goto fail;
			}
		}
	}

	/* find the starting block */
	block = DIV_FLOOR_LOG2(offset, dev->block_size_log2);

	pr_debug("buf %p, offset %" PRIu64", block %u, len %"PRIu64"\n",
		buf, offset, block, len);

	/* handle partial first block */
	if (MOD_LOG2(offset, dev->block_size_log2) != 0) {
		/* read in the block */
		error = tegrabl_blockdev_read_block(dev, temp_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* copy what we need */
		size_t block_offset = MOD_LOG2(offset, dev->block_size_log2);
		size_t tocopy = MIN(TEGRABL_BLOCKDEV_BLOCK_SIZE(dev) -
							block_offset, len);
		memcpy((uint8_t *)temp_buf + block_offset, buf, tocopy);

		/* write it back out */
		tegrabl_blockdev_write_block(dev, temp_buf, block, 1);
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

	pr_debug("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle middle blocks */
	if (len >= (1U << dev->block_size_log2)) {
		/* do the middle writes */
		size_t block_count = DIV_FLOOR_LOG2(len, dev->block_size_log2);

		error = tegrabl_blockdev_write_block(dev, buf, block, block_count);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		/* increment our buffers */
		size_t bytes = block_count << dev->block_size_log2;
		if (bytes > len) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
			goto fail;
		}

		buf += bytes;
		len -= bytes;
		bytes_written += bytes;
		block += block_count;
	}

	pr_debug("buf %p, block %u, len %"PRIu64"\n", buf, block, len);

	/* handle partial last block */
	if (len > 0) {
		/* read the block */
		error = tegrabl_blockdev_read_block(dev, temp_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		/* copy the partial block from our temp_buf buffer */
		memcpy(temp_buf, buf, len);

		/* write it back out */
		error = tegrabl_blockdev_write_block(dev, temp_buf, block, 1);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		bytes_written += len;
	}

fail:
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
	dev->ref += 1;
}

static tegrabl_error_t bdev_dec_ref(tegrabl_bdev_t *dev)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t oldval = dev->ref;

	if (dev->ref > 0) {
		dev->ref -= 1;
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

tegrabl_bdev_t *tegrabl_blockdev_open(uint16_t storage_type, uint16_t instance)
{
	tegrabl_bdev_t *bdev = NULL;
	tegrabl_bdev_t *entry;
	uint32_t device_id;

	if ((storage_type >= TEGRABL_STORAGE_INVALID) ||
		(instance >= INSTANCE_INVALID)) {
		pr_error("Invalid storage type = %d, instance = %d\n", storage_type,
			instance);
		bdev = NULL;
		goto fail;
	}

	device_id = TEGRABL_BLOCK_DEVICE_ID(storage_type, instance);

	/* see if it's in our list */
	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		if (entry->ref <= 0) {
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
		pr_error("%s: exit error\n", __func__);

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

	pr_debug("device id = %x\n", dev->device_id);

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

	if ((dev == NULL) || (len == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
		goto fail;
	}

	if (dev->ref <= 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}

	pr_debug("dev '%d', buf %p, offset %"PRIu64", len %"PRIu64"\n",
		dev->device_id, buf, offset, len);

	if ((offset >= dev->size) || ((offset + len) > dev->size)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	error = dev->read(dev, buf, offset, len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	if (error) {
		pr_error("%s: exit error = %x\n", __func__, error);
	}
	return error;
}
#endif

tegrabl_error_t tegrabl_blockdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (count == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	profile_read_start(dev);
#endif

	pr_debug("dev '%d', buf %p, block %d, count %u\n", dev->device_id, buf,
		 block, count);

	if (dev->ref <= 0) {
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
		pr_error("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
tegrabl_error_t tegrabl_blockdev_write(tegrabl_bdev_t *dev, const void *buf,
	off_t offset, off_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (len == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 10);
		goto fail;
	}

	pr_debug("dev '%d', buf %p, offset %"PRIu64", len %"PRIu64"\n",
		dev->device_id, buf, offset, len);

	if (dev->ref <= 0) {
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
	if (error) {
		pr_error("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((count == 0) || (dev == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	profile_write_start(dev);
#endif

	pr_debug("dev '%d', buf %p, block %d, count %u\n", dev->device_id, buf,
		block, count);

	/* range check */
	if ((block > dev->block_count) || ((block + count) > dev->block_count)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 3);
		goto fail;
	}

	if (dev->ref <= 0) {
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
	if (error) {
		pr_error("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

tegrabl_error_t tegrabl_blockdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (count == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 14);
		goto fail;
	}

	pr_debug("dev '%d', block %u, count %u\n", dev->device_id, block, count);

	/* range check */
	if ((block > dev->block_count) || ((block + count) > dev->block_count)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 4);
		goto fail;
	}

	if (dev->ref <= 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 15);
		goto fail;
	}

	error = dev->erase(dev, block, count, is_secure);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	if (error) {
		pr_error("%s: exit error = %x\n", __func__, error);
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

tegrabl_aio_t *tegrabl_blockdev_async_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count)
{
	/* TODO */
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(buf);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);
	return NULL;
}

tegrabl_aio_t *tegrabl_blockdev_async_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count)
{
	/* TODO */
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(buf);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);
	return NULL;
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

	pr_debug("dev '%d', request %08x, argp %p\n", dev->device_id, ioctl, args);

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
		pr_error("%s: exit error = %x\n", __func__, error);
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
		pr_error("%s: exit error = %x\n", __func__, error);
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

	pr_debug("%s: device id = %x\n", __func__, dev->device_id);

	bdev_inc_ref(dev);

	/* Check if duplicate */
	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		pr_debug("%d\n", entry->device_id);
		if (entry->device_id == dev->device_id) {
			pr_debug("block-device %x already registered\n", entry->device_id);
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

	pr_debug("%s: device id = %x\n", __func__, dev->device_id);

	/* remove it from the list  */
	list_delete(&dev->node);

	/* remove the ref the list used to have */
	bdev_dec_ref(dev);

	return TEGRABL_NO_ERROR;
}

void tegrabl_blockdev_dump_devices(void)
{
	pr_debug("block devices:\n");
	tegrabl_bdev_t *entry;
	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		pr_debug("\t%d, size %"PRIu64", bsize %lu, ref %d\n", entry->device_id,
				 entry->size, TEGRABL_BLOCKDEV_BLOCK_SIZE(entry), entry->ref);
	}
}

tegrabl_error_t tegrabl_blockdev_init(void)
{
	if (bdevs != NULL) {
		pr_debug("block dev already initialized:\n");
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
