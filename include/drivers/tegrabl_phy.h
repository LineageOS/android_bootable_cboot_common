/*
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_PHY_H
#define TEGRABL_PHY_H

struct phy_dev {
	uint16_t (*read)(uint32_t, uint32_t);
	void (*write)(uint32_t, uint32_t, uint32_t);
	bool is_link_up;
	uint32_t speed;
	bool duplex_mode;
};

void tegrabl_phy_config(const struct phy_dev * const phy);
tegrabl_error_t tegrabl_phy_auto_neg(const struct phy_dev * const phy);
void tegrabl_phy_detect_link(struct phy_dev * const phy);

#endif
