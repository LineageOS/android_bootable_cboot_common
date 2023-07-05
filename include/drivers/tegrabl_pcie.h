/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file tegrabl_pcie.h
 */

#ifndef TEGRABL_PCIE_H
#define TEGRABL_PCIE_H

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_error.h>

/**
 * @brief Maximum number of BARs a PCIe device can have
 */
#define MAX_BAR_COUNT 6

/**
 * @brief Types of IDs used while checking the presence of a PCIe device
 */
enum pcie_id_type {
	/**
	  * Combination of Vendor-ID and Device-ID.
	  * It is formed as (VID | (DID << 16))
	  */
	PCIE_ID_TYPE_VID_DID = 0,
	/** Class ID */
	PCIE_ID_TYPE_CLASS
};

/**
 * @brief Structure to represent a PCIe device
 */
struct tegrabl_pcie_dev;

/**
 * @brief Structure to represent resources of a PCIe device.
 */
struct tegrabl_pcie_resource {
	/** Start address of the resource */
	uint32_t start;
	/** Size of the resource */
	uint32_t size;
};

/**
 * @brief Structure to represent a PCIe bus
 */
struct tegrabl_pcie_bus {
	/** Start of IO address that this PCIe bus caters to */
	uint32_t io;
	/** Size of IO address */
	uint32_t io_size;
	/** Start of memory address that this PCIe bus caters to */
	uint32_t mem;
	/** Size of memory address */
	uint32_t mem_size;
	/** Hook to read configuration space of devices on this PCIe bus */
	void (*read)(struct tegrabl_pcie_dev *pdev, int offset, uint32_t *val);
	/** Hook to write configuration space of devices on this PCIe bus */
	void (*write)(struct tegrabl_pcie_dev *pdev, int offset, uint32_t val);
};

/**
 * @brief Structure to represent a PCIe device
 */
struct tegrabl_pcie_dev {
	/** Vendor-ID of the PCIe device */
	uint16_t vendor;
	/** Device-ID of the PCIe device */
	uint16_t device;
	/** Device type of the PCIe device */
	uint32_t dev_type;
	/** Device and Function numbers of the PCIe device */
	uint32_t devfn;
	/** Bus on which this PCIe device is present */
	uint32_t busnr;
	/** class code of this PCIe device */
	uint32_t class_code;
	/** Resouces information of the PCIe device */
	struct tegrabl_pcie_resource bar[MAX_BAR_COUNT];
	/** Bus information of this PCIe device */
	struct tegrabl_pcie_bus *bus;
	/** Host controller's private data structure */
	void *pdata;
	/** SMMU private data structure */
	void *smmu_cookie;
};

/**
 * @brief Helper API to return starting address of a particular resource of a PCIe device
 *
 * This API fulfills the following requirements:
 * - [Jama 10699522]
 *
 * @param[in] pdev Pointer to a PCIe device
 * @param[in] index Index of the resource for which start address is required
 *
 * @return Returns starting address of the Indexed resource
 */
static inline uint32_t tegrabl_pci_resource_start(struct tegrabl_pcie_dev *pdev,
						  uint32_t index)
{
	return ((pdev)->bar[(index)].start);
}

/**
 * @brief Performs initialization of the Host PCIE controller.
 *
 * Below are the steps performed by this API:
 * - Initialize Host controller
 * - Check for PCIe link up
 * - Enumerate connected device that includes
 * - Mapping of endpoint device's resouces into host system memory
 *
 * This API fulfills the following requirements:
 * - [Jama 10699303]
 *
 * @param[in] ctrl_num Controller number which needs to be initialized
 * @param[in] flags Specifies configuration information like link speed Etc.
 *                  Bits[2:0] : Specify the link speed
 *
 * @return TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_init(uint8_t ctrl_num, uint32_t flags);

/**
 * @brief Looks up for a device of specific ID
 *
 * This API returns pointer to PCIe device structure when a match of the device
 * is found. Matching is based on type of ID and this API supports matching
 * based on combination of Vendor-ID & Device-ID or Class code.
 *
 * @param[in] id ID with which comparison shall happen
 * @param[in] type Type of ID (combination of VID & DID OR Class code)
 *
 * @return Pointer to PCIe device structure
 */
struct tegrabl_pcie_dev *tegrabl_pcie_get_dev(uint32_t id,
					      enum pcie_id_type type);

/**
 * @brief API to read configuration space of a PCIe device
 *
 * This API fulfills the following requirements:
 * - [Jama 10699387]
 *
 * @param[in] pdev Pointer to PCIe device
 * @param[in] offset Offset in the config space to read
 * @param[in] val pointer to store the read value
 *
 * @return TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_conf_read(struct tegrabl_pcie_dev *pdev,
				       int offset, uint32_t *val);

/**
 * @brief API to write configuration space of a PCIe device
 *
 * This API fulfills the following requirements:
 * - [Jama 10699387]
 *
 * @param[in] pdev Pointer to PCIe device
 * @param[in] offset Offset in the config space to write
 * @param[in] val Value to be written
 *
 * @return TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_conf_write(struct tegrabl_pcie_dev *pdev,
					int offset, uint32_t val);

/**
 * @brief API to disable PCIe link with the endpoint
 *
 * This API fulfills the following requirements:
 * - [Jama 10709163]
 *
 * @param[in] pdev Pointer to PCIe device structure for which link needs to be disabled
 *
 * @returns TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_disable_link(struct tegrabl_pcie_dev *pci_dev);

/**
 * @brief API to put PCIE controller in reset state
 *
 * @param[in] ctrl_num PCIE controller number to be reset
 *
 * @returns TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_reset_state(uint8_t ctrl_num);

/**
 * @brief API to enable regulators of a PCIE controller
 *
 * @param[in] ctrl_num PCIE controller number to have regulators enabled
 *
 * @returns TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_enable_regulators(uint8_t ctrl_num);

/**
 * @brief API to disable regulators of a PCIE controller
 *
 * @param[in] ctrl_num PCIE controller number to have regulators disabled
 *
 * @returns void
 */
void tegrabl_pcie_disable_regulators(uint8_t ctrl_num);

#if defined(CONFIG_ENABLE_NVME_BOOT)
/**
 * @brief API to return a list of PCIe controller numbers
 *
 * @param[in] boot_dev is the boot device string
 *
 * @returns NULL if boot_dev is invalid
 *          list of PCIe controller numbers
 */
int8_t *tegrabl_get_pcie_ctrl_nums(char *boot_dev);
#endif

#endif /* TEGRABL_PCIE_H */
