/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <tegrabl_asm.h>

/*
 * Reference: ARM SMC Calling convention (ARM DEN 0028A)
 *
 * void tegrabl_arm64_send_smc64(struct tegrabl_arch_smc64_params *regs)
 */
FUNCTION(tegrabl_arm64_send_smc64)
	sub sp, sp, #16
	/* Only x19 is used as scratch register, but need to keep sp aligned to
	 * 16-bytes across function-calls */
	stp x19, x20, [sp]
	/* Use x19 as scratch register to store 'regs' pointer as this is
	 * guaranteed to be preserved across SMC calls */
	mov x19, x0
	/* Load parameters (func-id in x0 and params in x1..x6) */
	ldp x0, x1, [x19, #00]
	ldp x2, x3, [x19, #16]
	ldp x4, x5, [x19, #32]
	ldr x6, [x19, #48]
	isb
	smc #0
	/* Restore result (4 registers) */
	stp x0, x1, [x19, #00]
	stp x2, x3, [x19, #16]
	ldp x19, x20, [sp], #16
	ret
