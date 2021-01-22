/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file tegrabl_pcie_soc_local.h
 */

#ifndef TEGRABL_PCIE_SOC_LOCAL_H
#define TEGRABL_PCIE_SOC_LOCAL_H

#include <stdint.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_debug.h>

/** PCIe IO map offset in aperture */
#define PCIE_IO_OFFSET 0x100000
/** PCIe IO map size in aperture */
#define PCIE_IO_SIZE 0x100000
/** PCIe Memory map offset in aperture */
#define PCIE_MEM_OFFSET 0x200000
/** PCIe Memory map size in aperture */
#define PCIE_MEM_SIZE 0x1e00000

/**
 * @brief Structure to represent a PCIe host controller private information
 */
struct tegrabl_tegra_pcie {
	/** PCIe Controller (Root Port) number */
	uint32_t ctrl_num;
	/** Root Port config base address which is also the DBI base address */
	uint32_t cfg0_base;
	/** Root Port config size */
	uint32_t cfg0_size;
	/** Endpoint config base address */
	uint32_t cfg1_base;
	/** Ednpoint config size */
	uint32_t cfg1_size;
	/** ATU & DMA base address */
	uint32_t atu_dma_base;
};

/**
 * @brief API to get DBI base address
 *
 * @retval Pointer to the array of DBI base addresses for each controller
 */
uint32_t *tegrabl_pcie_get_dbi_reg(void);

/**
 * @brief API to get iATU base address
 *
 * @retval Pointer to the array of iATU base addresses for each controller
 */
uint32_t *regrabl_pcie_get_iatu_reg(void);

/**
 * @brief API to get IO map starting address
 *
 * @retval Pointer to the array of IO map addresses for each controller
 */
uint32_t *regrabl_pcie_get_io_base(void);

/**
 * @brief API to get Memory map starting address
 *
 * @retval Pointer to the array of Memory map addresses for each controller
 */
uint32_t *regrabl_pcie_get_mem_base(void);

/**
 * @brief API to power on a PCIe controller's phy
 *
 * @ctrl_num - PCIe controller number to power on phy
 *
 @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_power_on_phy(uint8_t ctrl_num);

/**
 * @brief API to pre-initialize a PCIe controller
 *
 * @ctrl_num - PCIe controller number to initialize
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_pcie_soc_preinit(uint8_t ctrl_num);

/**
 * @brief API to initialize a PCIe controller
 *
 * @ctrl_num - PCIe controller number to initialize
 * @link_speed - link speed
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_pcie_soc_init(uint8_t ctrl_num, uint8_t link_speed);
#endif
