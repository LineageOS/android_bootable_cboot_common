/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_UPHY_H
#define TEGRABL_UPHY_H

#include <tegrabl_error.h>

/**
 * @brief Lane owners of uphy
 */
enum tegrabl_uphy_owner {
	TEGRABL_UPHY_SATA,
	TEGRABL_UPHY_XUSB,
	TEGRABL_UPHY_PCIE,
	TEGRABL_UPHY_UFS,
	TEGRABL_UPHY_MAX
};

/**
 * @brief Handle to uphy driver.
 */
struct tegrabl_uphy_handle {
	tegrabl_error_t (*init)(enum tegrabl_uphy_owner owner);
	void (*power_down)(enum tegrabl_uphy_owner owner);
};

/**
 * @brief Initializes the lane for specified owner
 *
 * @param owner Lane owner
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_uphy_init(enum tegrabl_uphy_owner owner);


/**
 * @brief Power downs the uphy for particular owner.
 *
 * @param owner Lane owner.
 */
void tegrabl_uphy_power_down(enum tegrabl_uphy_owner owner);

#endif /* TEGRABL_UPHY_H */
