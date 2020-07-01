/*
 * Copyright (c) 2016, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <stdint.h>
#include <tegrabl_nvdisp.h>

#ifndef TEGRABL_NVDISP_CMU_H
#define TEGRABL_NVDISP_CMU_H

/**
* @brief Initialzes cmu configuration structure with default values.
*
* @param cmu Address of the cmu configuration structure.
*/
void nvdisp_cmu_init_defaults(struct nvdisp_cmu *cmu);

/**
* @brief Programs and enables the given cmu settings to nvdisp.
*
* @param nvdisp Address of the nvdisp structure.
* @param cmu Address of the cmu configuration structure.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t nvdisp_cmu_set(struct tegrabl_nvdisp *nvdisp,
							   struct nvdisp_cmu *cmu);

#endif
