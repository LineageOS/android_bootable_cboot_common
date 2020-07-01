/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_BLOCKDEV_H
#define TEGRABL_BLOCKDEV_H

#include "build_config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <tegrabl_error.h>
#include <list.h>
#include <tegrabl_timer.h>


typedef uint32_t bnum_t ;
typedef uint64_t off_t;

/**
* @brief Block dev iocl list
*/
enum {
	TEGRABL_IOCTL_BLOCK_SIZE,
	TEGRABL_IOCTL_PROTECTED_BLOCK_KEY,
	TEGRABL_IOCTL_ERASE_SUPPORT,
	TEGRABL_IOCTL_SECURE_ERASE_SUPPORT,
	TEGRABL_IOCTL_DEVICE_CACHE_FLUSH,
	TEGRABL_IOCTL_GET_XFER_STATUS,
	TEGRABL_IOCTL_GET_RPMB_WRITE_COUNTER,
	TEGRABL_IOCTL_INVALID,
};

/**
* @brief Asynchronous io structure
*/
typedef struct
{
	uint32_t xfer_id;
	tegrabl_error_t status;
	void *buf;
	bnum_t block;
	size_t count;
	uint32_t flags;
} tegrabl_aio_t;

#define TEGRABL_BLOCK_DEVICE_ID(storage_type, instance) \
	((storage_type) << 16 | (instance))

/**
* @brief Storage devices list
*/
typedef enum
{
	TEGRABL_STORAGE_SDMMC_BOOT,
	TEGRABL_STORAGE_SDMMC_USER,
	TEGRABL_STORAGE_SDMMC_RPMB,
	TEGRABL_STORAGE_QSPI_FLASH,
	TEGRABL_STORAGE_SATA,
	TEGRABL_STORAGE_SDCARD,
	TEGRABL_STORAGE_USB_MS,
	TEGRABL_STORAGE_UFS,
	TEGRABL_STORAGE_INVALID,
} tegrabl_storage_type_t;

/**
* @brief Controller instances
*/
typedef enum
{
	INSTANCE_0 = 0,
	INSTANCE_1,
	INSTANCE_2,
	INSTANCE_3,
	INSTANCE_4,
	INSTANCE_INVALID,
} tegrabl_instance_t;

struct tegrabl_bdev_struct {
	struct list_node list;
};

/**
* @brief block device structure. It holds information about storage interface
*        block properties, kpi information and function pointers to read, write
*        and erase blocks. Each registered storage device will have its own
*        bdev structure.
*/
typedef struct tegrabl_bdev {
	struct list_node node;
	uint32_t ref;
	uint32_t device_id;
	uint64_t size;
	uint32_t block_size_log2;
	bnum_t block_count;
	bool published;

#if defined(CONFIG_ENABLE_BLOCKDEV_KPI)
	time_t last_read_start_time;
	time_t last_read_end_time;
	time_t last_write_start_time;
	time_t last_write_end_time;
	time_t total_read_time;
	time_t total_write_time;
	uint64_t total_read_size;
	uint64_t total_write_size;
#endif
	tegrabl_aio_t *xfer_queue;

	void *priv_data;

	tegrabl_error_t (*read)(struct tegrabl_bdev *, void *buf, off_t offset,
		off_t len);
	tegrabl_error_t (*write)(struct tegrabl_bdev *, const void *buf,
		off_t offset, off_t len);
	tegrabl_error_t (*read_block)(struct tegrabl_bdev *, void *buf,
		bnum_t block, bnum_t count);
	tegrabl_error_t (*write_block)(struct tegrabl_bdev *, const void *buf,
		bnum_t block, bnum_t count);
	tegrabl_aio_t *(*async_read_block)(struct tegrabl_bdev *, void *buf,
		bnum_t block, bnum_t count);
	tegrabl_aio_t *(*async_write_block)(struct tegrabl_bdev *, const void *buf,
		 bnum_t block, bnum_t count);
	tegrabl_error_t (*erase)(struct tegrabl_bdev *, bnum_t block, bnum_t count,
		bool is_secure);
	tegrabl_error_t (*erase_all)(struct tegrabl_bdev *, bool is_secure);
	tegrabl_error_t (*ioctl)(struct tegrabl_bdev *, uint32_t ioctl, void *args);
	tegrabl_error_t (*close)(struct tegrabl_bdev *);
} tegrabl_bdev_t;

#define TEGRABL_BLOCKDEV_BLOCK_SIZE(dev)	(1UL << (dev)->block_size_log2)

#define TEGRABL_BLOCKDEV_BLOCK_SIZE_LOG2(dev)		((dev)->block_size_log2)

/**
 * @brief Returns the type of the input storage device
 *
 * @param bdev Handle of the storage device
 *
 * @return Returns the storage type if bdev is not null
 * else TEGRABL_STORAGE_INVALID
 */
static inline uint16_t tegrabl_blockdev_get_storage_type(
		struct tegrabl_bdev *bdev)
{
	if (bdev != NULL) {
		return (uint16_t)
			MIN(((bdev->device_id >> 16) & 0xFF), TEGRABL_STORAGE_INVALID);
	}

	return TEGRABL_STORAGE_INVALID;
}

/**
 * @brief Returns the instance of the input storage device
 *
 * @param bdev Handle of the storage device
 *
 * @return Returns the instance if bdev is no null else 0xFF.
 */
static inline uint16_t tegrabl_blockdev_get_instance(
		struct tegrabl_bdev *bdev)
{
	if (bdev != NULL)
		return (uint16_t)(bdev->device_id & 0xFF);

	return (uint16_t)0xFF;
}

/**
 * @brief Initializes the block device framework
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_init(void);

/**
* @brief Returns the next registered block device
*
* @param curr_bdev Current block device. If is is NULL, it returns
*                  first registeted block device
*
* @return Returns handle of the next block device. If not present returns NULL.
*/
tegrabl_bdev_t *tegrabl_blockdev_next_device(tegrabl_bdev_t *curr_bdev);

/** @brief Checks and returns the handle of the given device.
 *
 *  @param name String which specifies the name of the
 *              block device
 *
 *  @return Returns handle of the given storage block device, NULL if fails.
 */
tegrabl_bdev_t* tegrabl_blockdev_open(uint16_t storage_type, uint16_t instance);

/** @brief Closes the given block device.
 *
 *  @param name String which specifies the name of the
 *              block device
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_close(tegrabl_bdev_t *dev);

/** @brief Reads the data of requested bytes from the given block device
 *         starting given offset.
 *
 *  @param dev Block device handle.
 *  @param buf Buffer to which read data has to be passed.
 *  @param offset Offset of the start byte for read.
 *  @param len Number of bytes to be read.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_read(tegrabl_bdev_t *dev, void *buf,
	off_t offset, off_t len);

/** @brief Writets the data to device starting from the given offset.
 *
 *  @param dev Block device handle.
 *  @param buf Buffer to which read data has to be passed.
 *  @param offset Offset of the start byte for read.
 *  @param len Number of bytes to be read.
 *
i *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_write(tegrabl_bdev_t *dev, const void *buf,
	off_t offset, off_t len);

/** @brief Reads the data of requested blocks from the given block device
 *         starting given start block.
 *
 *  @param dev Block device handle.
 *  @param buf Buffer to which read data has to be passed.
 *  @param block start sector for read.
 *  @param count Number of blocks to be read.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count);

/** @brief Writes the data from blocks starting from given start block.
 *
 *  @param dev Block device handle.
 *  @param buf Buffer from which data has to be written.
 *  @param block Start sector for write.
 *  @param count Number of blocks to be write.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count);

/** @brief Erases data in blocks starting from the given start block.
 *
 *  @param dev Block device handle.
 *  @param block Start block for erase.
 *  @param count Number of blocks to be erase.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure);

/** @brief Erases all the data in the block device
 *
 *  @param dev Block device handle.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_erase_all(tegrabl_bdev_t *dev, bool is_secure);

/** @brief Reads the data of requested blocks from the given block device
 *         starting given start block.
 *
 *  @param dev Block device handle.
 *  @param buf Buffer to which read data has to be passed.
 *  @param block start sector for read.
 *  @param count Number of blocks to be read.
 *
 *  @return Returns the pointer to the async io structure, NULL if fails.
 */
tegrabl_aio_t *tegrabl_blockdev_async_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count);

/** @brief Reads the data from blocks starting from given start block.
 *
 *  @param dev Block device handle.
 *  @param buf Buffer from which data has to be written.
 *  @param block Start sector for write.
 *  @param count Number of blocks to be write.
 *
 *  @return Returns the pointer to the async io structure, NULL if fails.
 */
tegrabl_aio_t *tegrabl_blockdev_async_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count);

/** @brief Executes given ioctl
 *
 *  @param dev Block device handle.
 *  @param ioctl Specifies the ioctl.
 *  @param in_argp Pointer to the Input Arguments structure
 *  @param out_argp Pointer to the Output Arguments structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_ioctl(tegrabl_bdev_t *dev, uint32_t ioctl,
	void *args);

/** @brief Registers the given block device with the blockdev framework
 *
 *  @param dev Block device handle.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_register_device(tegrabl_bdev_t *dev);

/** @brief Unregisters the given block device with the blockdev framework
 *
 *  @param dev Block device handle.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_unregister_device(tegrabl_bdev_t *dev);

/** @brief Initializes the given block device
 *
 *  @param dev Block device handle.
 *  @param name String which specifices the block device.
 *  @param block_size_log2 Log_2 of size of the block in the block device
 *  @param block_count  Total block count in the block device
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_initialize_bdev(tegrabl_bdev_t *dev,
	uint32_t device_id, uint32_t block_size_log2, bnum_t block_count);

/**
* @brief List the kpi like read time and write time
*/
void tegrabl_blockdev_list_kpi(void);

/**
* @brief Prints the block devices info
*/
void tegrabl_blockdev_dump_devices(void);
#endif
