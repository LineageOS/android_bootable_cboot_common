/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_ARM64_SMCCC_H
#define INCLUDED_TEGRABL_ARM64_SMCCC_H

#include <stdint.h>

/**
 * @brief Input/output parameter to arm64_send_smc()
 */
struct tegrabl_arm64_smc64_params {
	uint64_t reg[7];
};

/**
 * @brief Sends SMC64 to EL3 S/W in compliance to ARM SMC Calling Convention
 *
 * @param regs Input/output params of the SMC
 *
 * Caller should put SMC function-id in regs.reg[0] and upto 6 parameters
 * in regs.reg[1..6].
 * Upto 4 return parameters are returned in regs.reg[0..3]
 */
void tegrabl_arm64_send_smc64(struct tegrabl_arm64_smc64_params *regs);

#endif /* INCLUDED_TEGRABL_ARM64_SMCCC_H */
