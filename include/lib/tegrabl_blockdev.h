/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
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

/* Block dev ioctl list */

#define TEGRABL_IOCTL_BLOCK_SIZE               0U
#define TEGRABL_IOCTL_PROTECTED_BLOCK_KEY      1U
#define TEGRABL_IOCTL_ERASE_SUPPORT            2U
#define TEGRABL_IOCTL_SECURE_ERASE_SUPPORT     3U
#define TEGRABL_IOCTL_DEVICE_CACHE_FLUSH       4U
#define TEGRABL_IOCTL_GET_XFER_STATUS          5U
#define TEGRABL_IOCTL_GET_RPMB_WRITE_COUNTER   6U
#define TEGRABL_IOCTL_BLOCK_DEV_SUSPEND	       7U
#define TEGRABL_IOCTL_SEND_STATUS		       8U
#define TEGRABL_IOCTL_INVALID                  9U

#define TEGRABL_BLOCKDEV_WRITE			1U
#define TEGRABL_BLOCKDEV_READ			2U
#define TEGRABL_BLOCKDEV_ERASE			2U

#define TEGRABL_BLOCKDEV_XFER_IN_PROGRESS	1U
#define TEGRABL_BLOCKDEV_XFER_COMPLETE		2U
#define TEGRABL_BLOCKDEV_XFER_FAILURE		3U

/*Align buffer to 8 bytes, in general storage driver's
 *needs buffer aligned. Aligning buffer to 8 bytes will satisfy
 *all storage media's.
 */
#define TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE		8U

/**
* @brief Blockdev Transfer Info structure
*/
struct tegrabl_blockdev_xfer_info {
	struct tegrabl_bdev *dev;
	uint8_t xfer_type; /* read, write, erase */
	void *buf;
	bnum_t start_block;
	bnum_t block_count;
	bool is_non_blocking;
	bool is_secure_erase;
	uint32_t id;
	uint8_t xfer_status;
};

#define TEGRABL_BLOCK_DEVICE_ID(storage_type, instance) \
	((storage_type) << 16 | (instance))

/**
* @brief Storage devices list
*/
#define TEGRABL_STORAGE_SDMMC_BOOT	0UL
#define TEGRABL_STORAGE_SDMMC_USER	1UL
#define TEGRABL_STORAGE_SDMMC_RPMB	2UL
#define TEGRABL_STORAGE_QSPI_FLASH	3UL
#define TEGRABL_STORAGE_SATA		4UL
#define TEGRABL_STORAGE_USB_MS		5UL
#define TEGRABL_STORAGE_SDCARD		6UL
#define TEGRABL_STORAGE_UFS		7UL
#define TEGRABL_STORAGE_UFS_USER	8UL
#define TEGRABL_STORAGE_UFS_RPMB	9UL
#define TEGRABL_STORAGE_NVME		10UL
#define TEGRABL_STORAGE_MAX		11UL
#define TEGRABL_STORAGE_INVALID		TEGRABL_STORAGE_MAX
typedef uint32_t tegrabl_storage_type_t;

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
	uint32_t buf_align_size;

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

	void *priv_data;

	tegrabl_error_t (*read)(struct tegrabl_bdev *dev, void *buf, off_t offset,
		off_t len);
	tegrabl_error_t (*write)(struct tegrabl_bdev *dev, const void *buf,
		off_t offset, off_t len);
	tegrabl_error_t (*read_block)(struct tegrabl_bdev *dev, void *buf,
		bnum_t block, bnum_t count);
	tegrabl_error_t (*write_block)(struct tegrabl_bdev *dev, const void *buf,
		bnum_t block, bnum_t count);
	tegrabl_error_t (*xfer)(struct tegrabl_blockdev_xfer_info *xfer);
	tegrabl_error_t (*xfer_wait)(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout, uint8_t *status_flag);
	tegrabl_error_t (*erase)(struct tegrabl_bdev *dev, bnum_t block, bnum_t count,
		bool is_secure);
	tegrabl_error_t (*erase_all)(struct tegrabl_bdev *dev, bool is_secure);
	tegrabl_error_t (*ioctl)(struct tegrabl_bdev *dev, uint32_t ioctl, void *args);
	tegrabl_error_t (*close)(struct tegrabl_bdev *dev);
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
			MIN(((bdev->device_id >> 16) & 0xFFU), TEGRABL_STORAGE_INVALID);
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
	if (bdev != NULL) {
		return (uint16_t)(bdev->device_id & 0xFFU);
	}
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
tegrabl_bdev_t *tegrabl_blockdev_open(tegrabl_storage_type_t storage_type,
		uint32_t instance);

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

/**
 * @brief Executes the either read/write/erase transaction based on the given transaction details.
 *
 * @param xfer transfer info
 *
 * @returin TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_blockdev_xfer(struct tegrabl_blockdev_xfer_info *xfer);

/**
* @brief Checks the transaction status and triggers the pending xfers and waits as per given timeout.
*
* @param xfer transfer info
* @param timeout  time to wait for the transfer in us
* @param xfer status_flag Address of the status flag. XFER_IN_PROGRESS, XFER_COMPLETE

* @returin TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_blockdev_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout,
		uint8_t *status_flag);

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

/**
* @brief return the storage name
*  @param type  storage-type
*
*  @return storage_name
*/
const char *tegrabl_blockdev_get_name(tegrabl_storage_type_t type);

#endif
