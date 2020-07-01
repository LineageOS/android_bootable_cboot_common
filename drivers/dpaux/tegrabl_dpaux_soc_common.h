/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDE_TEGRABL_DPAUX_SOC_COMMON_H
#define INCLUDE_TEGRABL_DPAUX_SOC_COMMON_H

#include <stdint.h>
#include <stdbool.h>

struct dpaux_soc_info {
	uint32_t base_addr;
	uint32_t module;
};

void dpaux_get_soc_info(struct dpaux_soc_info **hdpaux_info);

#endif /* INCLUDE_TEGRABL_DPAUX_SOC_COMMON_H */
