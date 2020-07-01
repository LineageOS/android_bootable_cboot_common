/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_PARTITION_MANAGER_H
#define TEGRABL_PARTITION_MANAGER_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_blockdev.h>

#define MAX_PARTITION_NAME 40
#define PART_GUID_STR_LEN  37

/**
 * @brief Stores the information about partition.
 *
 * TODO:Add id to name mapping.
 */
struct tegrabl_partition_info {
	char name[MAX_PARTITION_NAME];
	char guid[PART_GUID_STR_LEN];
	uint64_t start_sector;
	uint64_t num_sectors;
	uint64_t total_size;
};

/**
 * @brief Stores the information about partition and this
 * information will be used while all operations on partitions.
 */
struct tegrabl_partition {
	struct tegrabl_partition_info *partition_info;
	tegrabl_bdev_t *block_device;
	uint64_t offset;
};

#if defined(CONFIG_ENABLE_RECOVERY_VERIFY_WRITE)
/**
 * @brief Stores partition list that are to be verified.
 */
struct verify_list_info {
	uint64_t size;
	uint32_t crc32;
	struct list_node node;
	char name[MAX_PARTITION_NAME];
};
#endif
/**
 * @brief Defines the origin for seek operation.
 */
/* macro tegrabl partition seek */
typedef uint32_t tegrabl_partition_seek_t;
#define TEGRABL_PARTITION_SEEK_SET 0
#define TEGRABL_PARTITION_SEEK_CUR 1
#define TEGRABL_PARTITION_SEEK_END 2

/**
 * @brief Defines the operations that can be done
 * by ioctl function.
 */
/* macro tegrabl ioctl */
typedef uint32_t tegrabl_ioctl_t;
#define TEGRABL_IOCTL_SIZE 0
#define TEGRABL_IOCTL_SECTOR_SIZE 1
#define TEGRABL_IOCTL_ALGINMENT 2

/**
 * @brief Initializes the data structures used by partition
 * manager. Iterates over all storage devices and publish partitions.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_manager_init(void);

/**
 * @brief Looks up the partition name into published partitions
 * and updates the partition handle.
 *
 * @param partition_name Name of the partition.
 * @param partition Handle of the partition.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_open(const char *partition_name, struct tegrabl_partition *partition);

/**
 * @brief Looks up the partition name into published partitions of given block device
 * and updates the partition handle.
 *
 * @param partition_name Name of the partition.
 * @param partition Handle of the partition.
 * @param bdev block device to be looked into.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_lookup_bdev(const char *partition_name, struct tegrabl_partition *partition,
											  tegrabl_bdev_t *bdev);

/**
 * @brief Closes the partition. After this call same variable
 * cannon be used to access the partition.
 *
 * @param partition Handle of the partition.
 */
void tegrabl_partition_close(struct tegrabl_partition *partition);

/**
 * @brief Reads specified number of bytes of a  partition from the current
 * offset into buffer. Call to this function will be blocked till specified
 * number of bytes are read.
 *
 * @param partition Handle of the partition.
 * @param buf Destination buffer in which data to be read.
 * @param num_bytes Number of bytes to read.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_read(struct tegrabl_partition *partition,
		void *buf, size_t num_bytes);

/**
 * @brief Writes specified number of bytes to a partition at the current
 * offset from the buffer. Call to this function will be blocked till specified
 * number of bytes are wrote.
 *
 * @param partition Handle of the partition.
 * @param buf Source buffer containing data.
 * @param num_bytes Number of bytes to write.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_write(struct tegrabl_partition *partition,
		const void *buf, size_t num_bytes);

/**
 * @brief Returns the size of partition.
 *
 * @param partition Handle of the partition.
 *
 * @return Size in bytes.
 */
uint64_t tegrabl_partition_size(struct tegrabl_partition *partition);
/**
 * @brief Get start of partition.
 *
 * @param partition Handle of the partition.
 * @param start Start of partition in block, to be updated in function
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */


tegrabl_error_t tegrabl_partition_start_in_block(
		struct tegrabl_partition *partition,
		uint32_t *start);
/**
 * @brief Sets the current read/write position of partition to
 * new position.
 *
 * @param partition Handle of the partition.
 * @param offset Number of bytes to offset from origin.
 * @param origin Position used as reference for the offset.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_seek(struct tegrabl_partition *partition,
		int64_t offset, tegrabl_partition_seek_t origin);

/**
 * @brief Returns the current value of the position indicator of partition.
 *
 * @param partition Handle of the partition.
 *
 * @return Current value of position indicator is returned on success else -1.
 */
int64_t tegrabl_partition_tell(struct tegrabl_partition *partition);

/**
 * @brief Erases the entire partition. Call to this function will be blocked
 * till complete erase is done.
 *
 * @param partition Handle of the partition.
 * @oaram secure True if partition is erased securely, false otherwise.
 *
 * @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_partition_erase(struct tegrabl_partition *partition,
		bool secure);

/**
 * @brief Non blocking read operation on partition. Read of partial sector
 * cannot be done asynchronously. Function will immediately return with
 * asynchronous io handle. Call tegrabl_partition_getstatus() for status
 * of asynchronous io operation with returned handle.
 *
 * @param partition Handle of the partition.
 * @param buf Destination buffer in which data to read.
 * @param start_sector Relative sector number from which data to read.
 * @param num_sectors Number of sectors to be read.
 * @param p_xfer Address of the xfer info handle
 *
 * @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_partition_async_read(struct tegrabl_partition *partition, void *buf,
	uint64_t start_sector, uint64_t num_sectors, struct tegrabl_blockdev_xfer_info **p_xfer);

/**
 * @brief Non blocking write operation on partition. Write of partial sector
 * cannot be done asynchronously. Function will immediately return with
 * asynchronous io handle. Call tegrabl_partition_getstatus() for status
 * of asynchronous io operation with returned handle.
 *
 * @param partition Handle of the partition.
 * @param buf Source buffer containing the data.
 * @param start_sector Relative sector number from which data to write.
 * @param num_sectors Number of sectors to write.
 * @param p_xfer Address of the xfer info handle
 *
 * @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_partition_async_write(struct tegrabl_partition *partition, void *buf,
	uint64_t start_sector, uint64_t num_sectors, struct tegrabl_blockdev_xfer_info **p_xfer);

/**
 * @brief Returns the status of asynchronous io operation.
 *
 * @param partition Handle of the partition.
 * @param aio Handle of the asynchronous io operation.
 * @param xfer xfer info handle
 *
 * @return TEGRABL_NO_ERROR if successfully completed else appropriate error.
 */
tegrabl_error_t tegrabl_partition_getstatus(struct tegrabl_partition *partition,
		struct tegrabl_blockdev_xfer_info *xfer);

/**
 * @brief Does ioctl operation on a partition. Input buffer will contain
 * the data required to accomplish a specific ioctl. Out buffer will
 * be updated with result of operation.
 *
 * @param partition Handle of the partition.
 * @param ioctl IOCTL number.
 * @param in Input data buffer.
 * @param in_size Size of the input data buffer.
 * @param out Output data buffer. Should be large enough to hold
 * result of output.
 * @param out_size If value pointed by address is zero then size of the
 * ioctl result will be updated. Else value pointed by
 * address should be the size of output data buffer.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_ioctl(struct tegrabl_partition *partition,
		tegrabl_ioctl_t ioctl, void *in, size_t in_size, void *out,
		size_t *out_size);

/**
 * @brief Publishes all partition from specified device.
 *
 * @param dev Handle of the device.
 * @param offset Offset of partition table.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_publish(tegrabl_bdev_t *dev, off_t offset);

/**
 * @brief Removes all partitions registered for this device.
 *
 * @param dev Handle of the device.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partitions_unpublish(tegrabl_bdev_t *dev);

/**
 * @brief Flushes any partition manager caches and storage device
 * caches.
 */
void tegrabl_partition_manager_flush_cache(void);

/**
 * @brief Query whether system is booted with three level partition
 * table layout.
 *
 * @param info points to buffer which will be updated with the three
 * level PT information. It is set to true when syste is booted with
 * three level PT layout and false otherwise.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_partition_pt_has_3_levels(bool *info);

#endif	/* TEGRABL_PARTITION_MANAGER_H */


#if defined(CONFIG_ENABLE_RECOVERY_VERIFY_WRITE)
/**
 *	@brief Reads back & calculate crc32 for all the partitions
 *	present in verify list, and tally crc32 with the one sent
 *	from host.
 */
tegrabl_error_t tegrabl_partition_verify_write(void);

/**
 *  @brief Checks if partition is enabled for verification.
 */
bool tegrabl_partition_is_verify_enabled(
		struct tegrabl_partition *partition);

/**
 * @brief Mark partition for verification
 */
tegrabl_error_t tegrabl_partition_verify_enable(char *partition_name,
		uint32_t device_type, uint32_t instance);

/**
 * @brief Sets crc & size to verify partition node
 * @param partition_handle verify node to store crc value
 * @param size size of partition to be stored
 * @param is_sparse to skip sparse partition verification
 *
 */
tegrabl_error_t tegrabl_partition_set_crc(
		struct tegrabl_partition *partition, uint32_t verify_crc,
		uint32_t verify_size, bool is_sparse);
#endif
