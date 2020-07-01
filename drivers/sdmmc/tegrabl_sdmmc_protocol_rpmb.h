/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMMC_PROTOCOL_RPMB_H
#define TEGRABL_SDMMC_PROTOCOL_RPMB_H

#include <tegrabl_sdmmc_defs.h>

tegrabl_error_t sdmmc_rpmb_io(uint8_t is_write,
							  sdmmc_rpmb_context_t *rpmb_context,
							  struct tegrabl_sdmmc *hsdmmc);
#endif
