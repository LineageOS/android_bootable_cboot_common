/*
 * Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_NVPT_H
#define INCLUDED_TEGRABL_NVPT_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_partition_manager.h>

#define TEGRABL_NVPT_PARTITION_NAME_LENGTH	21
#define TEGRABL_NVPT_MOUNTPATH_NAME_LENGTH (TEGRABL_NVPT_PARTITION_NAME_LENGTH)

/* Magic number to identify partition table on storage media.*/
#define TEGRABL_NVPT_MAGIC_WO_COMPLEMENT 0x70617274UL
#define TEGRABL_NVPT_MAGIC \
		(((uint64_t)(TEGRABL_NVPT_MAGIC_WO_COMPLEMENT) << 32) | \
		(~((uint64_t)(TEGRABL_NVPT_MAGIC_WO_COMPLEMENT))))

#define TEGRABL_NVPT_VERSION 0x0100

#define TEGRABL_NVPT_AES_HASH_BLOCK_LEN    16

/**
 * Defines the partition type.
 */
enum tegrabl_nvpt_partition_type {
	TEGRABL_NVPT_TYPE_BCT = 0X1,
	TEGRABL_NVPT_TYPE_BOOTLOADER,
	TEGRABL_NVPT_TYPE_MB1 = TEGRABL_NVPT_TYPE_BOOTLOADER,
	TEGRABL_NVPT_TYPE_PARTITION_TABLE,
	TEGRABL_NVPT_TYPE_OS,
	TEGRABL_NVPT_TYPE_OS_DTB,
	TEGRABL_NVPT_TYPE_NVDATA,
	TEGRABL_NVPT_TYPE_DATA,
	TEGRABL_NVPT_TYPE_MBR,
	TEGRABL_NVPT_TYPE_EBR,
	TEGRABL_NVPT_TYPE_GP1,
	TEGRABL_NVPT_TYPE_GPT,
	TEGRABL_NVPT_TYPE_BOOTLOADER_STAGE2,
	TEGRABL_NVPT_TYPE_DUSE_BYPASS,
	TEGRABL_NVPT_TYPE_CONFIG_TABLE,
	TEGRABL_NVPT_TYPE_WB0,
	TEGRABL_NVPT_TYPE_SECURE_OS,
	TEGRABL_NVPT_TYPE_MB1_BCT,
	TEGRABL_NVPT_TYPE_SPE_FW,
	TEGRABL_NVPT_TYPE_MTS_PRE,
	TEGRABL_NVPT_TYPE_MTS_BPK,
	TEGRABL_NVPT_TYPE_DRAM_ECC,
	TEGRABL_NVPT_TYPE_BLACKLIST_INFO,
	TEGRABL_NVPT_TYPE_EXTENDED_CAN_FW,
	TEGRABL_NVPT_TYPE_MB2,
	TEGRABL_NVPT_TYPE_APE_FW,
	TEGRABL_NVPT_TYPE_SCE_FW,
	TEGRABL_NVPT_TYPE_CPU_BL,
	TEGRABL_NVPT_TYPE_EKS,
	TEGRABL_NVPT_TYPE_BPMP_FW,
	TEGRABL_NVPT_TYPE_BPMP_FW_DTB,
	TEGRABL_NVPT_TYPE_GPH,
	TEGRABL_NVPT_TYPE_RAMDISK,
	TEGRABL_NVPT_TYPE_FORCE32 = 0X7FFFFFFF,
};

/**
 * Defines MBR-like information for a partition.
 */
struct tegrabl_nvpt_partition_info {
	/* Holds partition attributes.
	@warning not currently implemented or supported.*/
	uint32_t partition_attr;

	/* Specifies whether the partition is on virtual storage
	and using which IVC channel.*/
	uint32_t virtual_storage_ivc_channel;

	/* Holds logical start address for partition.
	It is the address of the device where partition is located.*/
	uint64_t start_logical_address;

	/* Holds the logical size of the partition.
	It is the size of the device where partition is located.*/
	uint64_t num_logical_bytes;

	/* Holds the start of physical address of the partition on the storage
	media.*/
	uint64_t start_physical_address;

	/* Holds the end of physical address of the partition on the storage
	media (inclusive). */
	uint64_t end_physical_address;

	/* Holds the partition type.*/
	enum tegrabl_nvpt_partition_type partition_type;

	/* Indicates if the partition is to be made write protected on bootup.*/
	uint32_t is_write_protected;
};

/**
 * Defines filesystem mount information.
 */
struct tegrabl_nvpt_fs_mount_info {
	/* Holds the type of device on which partition is located.*/
	uint32_t device_id;

	/* Holds the device instance on which partition is located.*/
	uint32_t device_instance;

	/* Holds the attribute passed to device driver when it is opened for
	accessing data in the partition.
	Device driver determines the meaning of this value.
	@warning This is a placeholder and is not currently implemented.*/
	uint32_t device_attr;

	/* Holds the path where partition will be mounted in the file system.
	@note string is in UTF-8 format.
	@warning Current implementation requires @c PartitionName and
	@c MountPath to be identical.*/
	char mount_path[TEGRABL_NVPT_MOUNTPATH_NAME_LENGTH];

	/* Holds the type of file system to mount on this partition.*/
	uint32_t file_system_type;

	/* Holds the attribute to be passed to file system driver when it is
	mounted on the partition.
	File system driver determines the meaning of this value.*/
	uint32_t file_system_attr;
};

/**
 * Defines partition table entry.
 */
struct tegrabl_nvpt_partition_entry {
	/* Holds the partition ID.*/
	uint32_t partition_id;
	/* Holds the partition name.*/
	char partition_name[TEGRABL_NVPT_PARTITION_NAME_LENGTH];
	/* Holds the filesystem mount information.*/
	struct tegrabl_nvpt_fs_mount_info mount_info;
	/* Holds the partition information.*/
	struct tegrabl_nvpt_partition_info part_info;
};

/**
 * Defines the insecure header.
 * The following data is neither signed nor encrypted.
 */
struct tegrabl_nvpt_header_insecure {
	/* Holds the magic number used to identify partition table on storage.*/
	uint64_t magic;

	/* Holds the partition table format version number.*/
	uint32_t insecure_version;

	/* Holds the length of partition data (in bytes) as stored on storage
	device. It includes the entire header, all partition table entries,
	and any padding. This value is never encrypted.*/
	uint32_t insecure_length;

	/* Holds the signature for secure header and all partition entries.*/
	uint8_t signature[TEGRABL_NVPT_AES_HASH_BLOCK_LEN];
};

/**
 * Defines the secure header.
 * @note The following data may be signed and/or encrypted, according to
 * IsEncrypted and IsSigned settings in the insecure header.
 *
 * Encryption and/or signing covers all data from this point forward to
 * the end of the Partition Table (@c PartitionTable) image as it appears
 * on the storage device.
 */
struct tegrabl_nvpt_header_secure {
	/* Holds random data used to randomize the first block of ciphertext
	that results from encrypting this structure.
	It serves a similar purpose as an initialization vector.*/
	uint8_t random_data[16];

	/* Holds the magic number used to identify partition table on storage.*/
	uint64_t secure_magic;

	/* Holds the partition table format version number.*/
	uint32_t secure_version;

	/* Holds the length of partition data (in bytes) as stored on storage.
	It includes entire header, all partition table entries & any padding.
	This value may be encrypted.*/
	uint32_t secure_length;

	/* Holds the number of partition table entries.*/
	uint32_t num_partitions;

	/* Holds dummy bytes to ensure that structure size is a multiple of
	sizeof(@c uint64_t).*/
	uint8_t padding1[4];
};

/**
 * Defines the partition table.
 *
 * @note Any padding required by the crypto algoritms cannot be included
 * here since the number of elements in the @c TableEntry[] array is unknown at
 * compile time.  Padding will have to be added at runtime, after the array
 * size is known.
 *
 * The total size of the partition table image (as it exists on the storage
 * device) is as follows:
 * <pre>
 * Total partition table image size =
 *   sizeof(NvPartitionTable) +
 *   sizeof(NvPartitionTableEntry[NvPartitionTable.SecureHeader.NumPartitions])
 *   + sizeof(crypto padding) </pre>
 */
struct tegrabl_nvpt_partition_table {
	/* Holds insecure header.*/
	struct tegrabl_nvpt_header_insecure insecure_header;
	/* Holds secure header.*/
	struct tegrabl_nvpt_header_secure secure_header;
	/* Holds pointer to variable-length array of table entries.*/
	struct tegrabl_nvpt_partition_entry table_entry[0];
};

/**
 * @brief Tries to read PT from the device and returns information
 * about all partitions in partition_list for that device. If there
 * are multiple storage devices, then very first call should be for
 * device containing PT. Same PT will be used to publish subsequent devices.
 *
 * @param dev Device to be published
 * @param offset Known offset of PT if not primary or secondary PT.
 * @param partition_list If PT is successfully found then this will
 * point to memory location containing list of partitions found.
 * @param num_partitions if PT  is successfully found then this will
 * be updated with number of partition found.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_nvpt_publish(tegrabl_bdev_t *dev,
		off_t offset, struct tegrabl_partition_info **partition_list,
		uint32_t *num_partitions);

/**
 * @brief Reads nvidia partition table from device and sets buffer to
 * point to memory containing nvpt.
 *
 * @param dev Device handle which contains nvpt
 * @param buffer Will be updated to memory containing
 * buffer. Caller must free.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_nvpt_read_table(tegrabl_bdev_t *dev,
		void **buffer);

#endif /* INCLUDED_TEGRABL_NVPT_H */
