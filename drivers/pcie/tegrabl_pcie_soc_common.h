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
 * @file tegrabl_pcie_soc_common.h
 */

#ifndef TEGRABL_PCIE_SOC_COMMON_H
#define TEGRABL_PCIE_SOC_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_pcie.h>
#include <tegrabl_pcie_soc_local.h>

/**
 * @brief Performs initialization of the Host PCIe controller.
 *
 * @param[in] ctrl_num Controller number which needs to be initialized
 * @param[in] flags Specifies configuration information like link speed Etc.
 * @param[out] bus Pointer to pointer of a PCIe bus structure
 * @param[out] pdata Pointer to PCIe private data structure
 *
 * @return TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_soc_host_init(uint8_t ctrl_num, uint32_t flags, struct tegrabl_pcie_bus **bus,
										   void **pdata);

/**
 * @brief API to disable PCIe host controller link with the endpoint
 *
 * @param[in] pdev Pointer to PCIe device structure for which link needs to be disabled
 *
 * @returns TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_soc_disable_link(uint8_t ctrl_num);

#endif /* TEGRABL_PCIE_SOC_COMMON_H */
