/*
 * Copyright (c) 2016 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "psci_priv.h"

/**
* @brief reset the board
*/
void tegrabl_psci_sys_reset(void)
{
	tegrabl_psci_smc(TEGRABL_PSCI_0_2_SYSTEM_RESET, 0, 0, 0);
}

/**
* @brief power-off the board
*/
void tegrabl_psci_sys_off(void)
{
	tegrabl_psci_smc(TEGRABL_PSCI_0_2_SYSTEM_OFF, 0, 0, 0);
}

