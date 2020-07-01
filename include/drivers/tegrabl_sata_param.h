/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SATA_PARAM_H
#define INCLUDED_TEGRABL_SATA_PARAM_H

/**
* @brief structure for sata platform parameters
*
* transfer_speed speed mode GEN1 or GEN2
* is_skip_init	Boolean flag to determine whether to do full init or skip init
*				true = skip init
*				flase = full init
*/
struct tegrabl_sata_platform_params {
	uint8_t transfer_speed;
	bool is_skip_init;
};

#endif /* INCLUDED_TEGRABL_SATA_PARAM_H */
