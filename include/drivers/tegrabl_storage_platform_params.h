/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_STORAGE_PLATFORM_PARAM_H
#define INCLUDED_TEGRABL_STORAGE_PLATFORM_PARAM_H

#include <tegrabl_sdmmc_param.h>
#include <tegrabl_qspi_flash_param.h>
#include <tegrabl_sata_param.h>
#include <tegrabl_ufs.h>

struct tegrabl_storage_platform_params {
	struct tegrabl_sdmmc_platform_params sdmmc;
	struct tegrabl_qspi_flash_platform_params qspi_flash;
	struct tegrabl_ufs_platform_params ufs;
	struct tegrabl_sata_platform_params sata;
};

#endif /* INCLUDED_TEGRABL_STORAGE_PLATFORM_PARAM_H */
