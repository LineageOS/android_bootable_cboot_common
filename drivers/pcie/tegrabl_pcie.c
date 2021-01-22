/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file tegrabl_pcie.c
 */

/** @brief Module identifier for printing logs. */
#define MODULE TEGRABL_ERR_PCIE

#include <inttypes.h>
#include <tegrabl_pcie.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_timer.h>
#include <tegrabl_utils.h>
#include <tegrabl_malloc.h>
#include "tegrabl_pcie_soc_common.h"

/**
 * @brief This count is for maxmimum allowed pcie_dev structures.
 * struct tegrabl_pcie_dev is required for each device and function.
 * Currently, enumeration of only the immediate device is supported and hence
 * a value of 8 is selected to cater to maxinum number of functions a device
 * can have.
 */
#define MAX_PDEV 8

/** Definitions related to section 7.5.1.2.1 of the PCIe 4.0 Specification */
/** Offset of BAR-0 registers */
#define PCI_BASE_ADDRESS_0		0x10
/** Offset of BAR-5 registers */
#define PCI_BASE_ADDRESS_5		0x24
/** Mask to check for a valid BAR resource */
#define PCI_BASE_ADDRESS_SPACE		0x01UL
/** Mask for Memory type of resource */
#define PCI_BASE_ADDRESS_MEM_TYPE_MASK	0x06UL
/** Mask for Memory (64-bit) resource */
#define PCI_BASE_ADDRESS_MEM_TYPE_64	0x04UL
/** Mask for Memory resource */
#define PCI_BASE_ADDRESS_MEM_MASK	(~0x0fULL)
/** Mask for IO resource */
#define PCI_BASE_ADDRESS_IO_MASK	(~0x03ULL)

#define IO_EN		0x01
#define MSE			0x02
#define BME			0x04

/**
 * @brief Allocate memory for maximum number of PCIe devices that could get
 * enumerated by this driver
 */
static struct tegrabl_pcie_dev enumeration_data[MAX_PDEV];

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
tegrabl_error_t tegrabl_pcie_conf_read(struct tegrabl_pcie_dev *pdev, int offset, uint32_t *val)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!pdev) {
		return TEGRABL_ERR_INVALID;
	}

	pdev->bus->read(pdev, offset, val);

	return error;
}

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
tegrabl_error_t tegrabl_pcie_conf_write(struct tegrabl_pcie_dev *pdev, int offset, uint32_t val)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!pdev) {
		return TEGRABL_ERR_INVALID;
	}

	pdev->bus->write(pdev, offset, val);

	return error;
}

/**
 * @brief Map endpoint BARs in system address space
 *
 * Each PCIe device can expose upto six IO/memory regions through BARs.
 * An endpoint PCIe device defines six BAR registers in config space to store
 * base address. BAR size can be determined by writing a value of 0xffffffff
 * to the register, and reading it back. The least significant 0s represent the
 * size of that BAR.
 *
 * @param[in] pdev Pointer to PCIe device structure
 */
static void pcie_alloc_bars(struct tegrabl_pcie_dev *pdev)
{
	uint32_t bar_response;
	uint64_t bar_value;
	uint64_t bar_size;
	int bar, found_mem64, bar_num;
	uint32_t io, mem, io_size, mem_size, io_end, mem_end;
	int i;

	io = pdev->bus->io;
	mem = pdev->bus->mem;
	io_size = pdev->bus->io_size;
	mem_size = pdev->bus->mem_size;
	io_end = io + io_size;
	mem_end = mem + mem_size;

	pr_info("PCI Config: I/O=0x%x, Memory=0x%lx\n", io, (uint64_t)mem);

	bar = PCI_BASE_ADDRESS_0;
	while (bar <= PCI_BASE_ADDRESS_5) {
		/* Least significant bits of BAR registers in config space are tied to 0,
		* to inform the size. So write all 1s to determine the size
		*/
		tegrabl_pcie_conf_write(pdev, bar, 0xffffffff);
		tegrabl_pcie_conf_read(pdev, bar, &bar_response);

		if (bar_response == 0UL) {
			goto done;
		}

		found_mem64 = 0;

		/* Check the BAR type and set our address mask */
		if ((bar_response & PCI_BASE_ADDRESS_SPACE) != 0UL) {
			bar_size = (uint32_t)(~(bar_response & PCI_BASE_ADDRESS_IO_MASK) + 1UL);
			if ((io + bar_size) > (io + io_size)) {
				goto done;
			}
			/* round up region base address to a multiple of size */
			io = ((io - 1UL) | (((uint32_t)(bar_size)) - 1UL)) + 1UL;
			bar_value = io;
			/* compute new region base address */
			io = io + ((uint32_t)bar_size);
			io_size = io_end - io;
			bar_num = (bar - PCI_BASE_ADDRESS_0) / 4;
			pdev->bar[bar_num].start = ((uint32_t)bar_value);
			pdev->bar[bar_num].size = ((uint32_t)bar_size);
			pr_info("IO bar_num=%d bar=0x%lx\n", bar_num, bar_value);
		} else {
			if ((bar_response & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_64) {
				uint32_t bar_response_upper;
				uint64_t bar64;
				tegrabl_pcie_conf_write(pdev, bar + 4, 0xffffffff);
				tegrabl_pcie_conf_read(pdev, bar + 4, &bar_response_upper);

				bar64 = ((uint64_t)bar_response_upper << 32) | bar_response;

				bar_size = ~(bar64 & PCI_BASE_ADDRESS_MEM_MASK) + 1UL;
				if ((mem + bar_size) > (mem + mem_size)) {
					goto done;
				}
				found_mem64 = 1;
			} else {
				bar_size = (uint32_t)(~(bar_response & PCI_BASE_ADDRESS_MEM_MASK) + 1UL);
				if ((mem + bar_size) > (mem + mem_size)) {
					goto done;
				}
			}

			/* round up region base address to multiple of size */
			mem = ((mem - 1UL) | (((uint32_t)bar_size) - 1UL)) + 1UL;
			bar_value = mem;
			/* compute new region base address */
			mem = mem + ((uint32_t)bar_size);
			mem_size = mem_end - mem;
			bar_num = (bar - PCI_BASE_ADDRESS_0) / 4;
			pdev->bar[bar_num].start = ((uint32_t)bar_value);
			pdev->bar[bar_num].size = ((uint32_t)bar_size);
			pr_info("%s bar_num=%d bar=0x%lx\n", found_mem64 ? "MEM64" : "MEM", bar_num, bar_value);
		}

		/* Write it out and update our limit */
		tegrabl_pcie_conf_write(pdev, bar, (uint32_t)bar_value);

		if (found_mem64 != 0) {
			bar += 4;
			tegrabl_pcie_conf_write(pdev, bar, (uint32_t)(bar_value >> 32));
		}
done:
		bar += 4;
	}

	pdev->bus->io = io;
	pdev->bus->io_size = io_size;
	pdev->bus->mem = mem;
	pdev->bus->mem_size = mem_size;
	for (i = 0; i < 6; i++) {
		tegrabl_pcie_conf_read(pdev, 0x10 + i * 4, &bar_response);
	}
}

/**
 * @brief Perform PCIe bus walk to enumerate devices
 *
 * In PCIe hierarchy each PCIe port is defined by bus, device and
 * function(BDF). There can be 256 buses in a PCIe hierarchy
 * including root port. At each bus level there can be 64 devices
 * and upto eight functions for each device. PCIe port can be
 * detected by reading vendor ID for each BDF. Non 0xffff or 0x0000
 * vendor ID represents a valid PCIe port.
 * Here bus walk is performed at first level looking for a PCIe device and for
 * functions in that device if any.
 *
 * @param[in] bus Pointer to bus structure
 * @param[in] pdata Pointer to private data structure
 */
static void pcie_bus_walk(struct tegrabl_pcie_bus *bus, void *pdata)
{
	struct tegrabl_pcie_dev *pdev, temp_pdev;
	uint32_t busnr, ids, cc, sts_cmd;
	int devfn;
	int ports = 0;
	uint16_t g_vendor, g_device;

	for (busnr = 0UL; busnr <= 1UL; busnr++) {
		/* TODO Need to extend it for multi device & function if required */
		for (devfn = 0; devfn < 1; devfn++) {
			temp_pdev.busnr = busnr;
			temp_pdev.pdata = pdata;
			temp_pdev.devfn = (uint32_t)devfn;
			temp_pdev.bus = bus;
			pr_info("Scanning busnr: %d devfn: %d\n", busnr, devfn);

			tegrabl_pcie_conf_read(&temp_pdev, 0x0, &ids);
			pr_info("PCIe IDs: 0x%x\n", ids);

			g_vendor = ((uint16_t)ids) & 0xffffU;
			g_device = ((uint16_t)(ids >> 16)) & 0xffffU;

			if (g_vendor == 0xffffUL || g_vendor == 0x0000UL) {
				continue;
			}

			tegrabl_pcie_conf_read(&temp_pdev, 0x8, &cc);
			pr_info("PCIe RID_CC: 0x%x\n", cc);
			cc = cc >> 8;	/* retrieve only class_code */

			pdev = &enumeration_data[ports];
			pdev->busnr = busnr;
			pdev->pdata = pdata;
			pdev->devfn = (uint32_t)devfn;
			pdev->class_code = cc;
			pdev->bus = bus;

			pdev->vendor = g_vendor;
			pdev->device = g_device;

			ports++;

			/**
			 * Allocate BARs for endpoint(BUS=1) only
			 */
			if (busnr == 1UL) {
				pcie_alloc_bars(pdev);
				tegrabl_pcie_conf_read(&temp_pdev, 0x4, &sts_cmd);
				sts_cmd |= BME | MSE | IO_EN;
				tegrabl_pcie_conf_write(&temp_pdev, 0x4, sts_cmd);
			}
		}
	}

	pr_info("Number of PCIe devices detected: %d\n", ports);
	return;
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
tegrabl_error_t tegrabl_pcie_init(uint8_t ctrl_num, uint32_t flags)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	struct tegrabl_pcie_bus *bus = NULL;
	void *pdata = NULL;

	/* enable regulators for this PCIe controller */
	error = tegrabl_pcie_enable_regulators(ctrl_num);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: failed to enable PCIe regulators; err=0x%x\n", __func__, error);
		return error;
	}

	error = tegrabl_pcie_soc_host_init(ctrl_num, flags, &bus, &pdata);
	if (error != TEGRABL_NO_ERROR) {
		pr_info("Failed to initialize SoC Host PCIe controller\n");
		return error;
	}

	if (bus == NULL || pdata == NULL) {
		return TEGRABL_ERR_INVALID;
	}

	pcie_bus_walk(bus, pdata);

	return error;
}

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
struct tegrabl_pcie_dev *tegrabl_pcie_get_dev(uint32_t id, enum pcie_id_type type)
{
	int i;

	for (i = 0; i < MAX_PDEV; i++) {
		if (type == PCIE_ID_TYPE_VID_DID) {
			uint32_t vid_did = enumeration_data[i].vendor | (enumeration_data[i].device << 16);
			if (vid_did == id) {
				return &enumeration_data[i];
			}
		}
		if (type == PCIE_ID_TYPE_CLASS) {
			uint32_t cc = enumeration_data[i].class_code;
			if (cc == id)
				return &enumeration_data[i];
		}

	}
	return NULL;
}

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
tegrabl_error_t tegrabl_pcie_disable_link(struct tegrabl_pcie_dev *pdev)
{
	struct tegrabl_tegra_pcie *pdata;

	if (!pdev) {
		return TEGRABL_ERR_INVALID;
	}

	pdata = (struct tegrabl_tegra_pcie *)pdev->pdata;
	return tegrabl_pcie_soc_disable_link(pdata->ctrl_num);
}
