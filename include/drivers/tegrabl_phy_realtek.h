/*
 * Copyright (c) 2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_PHY_REALTEK_H
#define TEGRABL_PHY_REALTEK_H

#include <tegrabl_phy.h>

#define PHY_REALTEK_OUI         0x000732

/*
 * @brief Configure Realtek PHY
 *
 * @param phy PHY object
 */
void tegrabl_phy_realtek_config(struct phy_dev * const phy);

/*
 * @brief Start auto-negotiation from Realtek PHY
 *
 * @param phy PHY object
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t tegrabl_phy_realtek_auto_neg(const struct phy_dev * const phy);

/*
 * @brief Detect link between Realtek PHY and MAC
 *
 * @param phy PHY object
 */
void tegrabl_phy_realtek_detect_link(struct phy_dev * const phy);

#endif
