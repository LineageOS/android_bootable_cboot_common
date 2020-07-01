/*
 * Copyright (c) 2016 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <tegrabl_asm.h>

/* int tegrabl_psci_smc(u64 function_id, u64 arg0, u64 arg1, u64 arg2) */
FUNCTION(tegrabl_psci_smc)
	smc #0

