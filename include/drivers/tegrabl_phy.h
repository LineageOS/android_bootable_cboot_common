/*
 * Copyright (c) 2018-2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_PHY_H
#define TEGRABL_PHY_H

#define SET                                             1
#define RESET                                           0
#define BIT(p)                                          (1UL << p)
#define SET_BIT(val, p)                                 ((val) | BIT(p))
#define CLEAR_BIT(val, p)                               ((val) & (~BIT(p)))
#define GET_BIT(val, pos)                               ((val >> pos) & 0x1)

struct phy_dev {
	uint16_t (*read)(uint32_t, uint32_t, uint32_t);
	void (*write)(uint32_t, uint32_t, uint32_t, uint32_t);
	void (*config)(struct phy_dev * const);
	tegrabl_error_t (*auto_neg)(const struct phy_dev * const);
	void (*detect_link)(struct phy_dev * const);
	uint32_t mdio_addr;
	uint32_t curr_page;
	uint32_t page_sel_reg;
	bool is_link_up;
	uint32_t speed;
	bool duplex_mode;
};

/*
 * @brief Wait till particular bit is set/reset
 *
 * @param phy PHY Object
 * @param page Register page
 * @param reg_addr REgister address
 * @param pos Bit position
 * @param set Check for set/reset
 * @param timeout_ms Timeout to wait
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t tegrabl_phy_wait_for_bit(const struct phy_dev * const phy,
										 uint32_t page,
										 uint32_t reg_addr,
										 uint32_t pos,
										 bool set,
										 uint32_t timeout_ms);

/*
 * @brief Get Organizationally Unique Identifier (OUI)
 *
 * @param phy PHY Object
 *
 * @return OUI of the PHY chip
 */
uint32_t tegrabl_phy_get_oui(struct phy_dev * const phy);

#endif
