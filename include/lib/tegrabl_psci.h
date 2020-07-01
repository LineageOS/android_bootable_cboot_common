/*
 * Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_PSCI_H
#define INCLUDED_TEGRABL_PSCI_H

/**
* @brief reset the board
*/
void tegrabl_psci_sys_reset(void);

/**
* @brief power-off the board
*/
void tegrabl_psci_sys_off(void);

#endif /*INCLUDED_TEGRABL_PSCI_H*/

