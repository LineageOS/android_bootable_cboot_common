/*
 * Copyright (c) 2016 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PSCI_PRIV_H

#include <tegrabl_psci.h>

/* PSCI v0.2 interface */
#define TEGRABL_PSCI_0_2_BASE		0x84000000

/* Only the PSCI FN ids which are currently needed by BL are added */
 #define TEGRABL_PSCI_0_2_SYSTEM_OFF    (TEGRABL_PSCI_0_2_BASE + 0x8)
 #define TEGRABL_PSCI_0_2_SYSTEM_RESET  (TEGRABL_PSCI_0_2_BASE + 0x9)

extern __attribute__((__noreturn__)) unsigned long (tegrabl_psci_smc)(
										unsigned long, unsigned long,
										unsigned long, unsigned long);

#endif  /* INCLUDED_PSCI_PRIV_H */
