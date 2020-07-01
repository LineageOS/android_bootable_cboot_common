/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */


#include <stdint.h>
#include <stdbool.h>

struct qspi_soc_info {
	uint32_t trig_len;
};

void qspi_get_soc_info(struct qspi_soc_info **gqspi_info);
