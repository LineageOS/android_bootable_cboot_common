/*
 * Copyright (c) 2018-2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PHY

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_phy.h>

#define PAGE_PHY							0

#define REG_PHY_IDENTIFIER_1				2

#define REG_PHY_IDENTIFIER_2				3
#define REG_PHY_IDENTIFIER_2_WIDTH			((15 - 10) + 1)
#define REG_PHY_IDENTIFIER_2_SHIFT			10

tegrabl_error_t tegrabl_phy_wait_for_bit(const struct phy_dev * const phy,
										 uint32_t page,
										 uint32_t reg_addr,
										 uint32_t pos,
										 bool set,
										 uint32_t timeout_ms)
{
	uint32_t val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	time_t elapsed_time_ms = 0U;
	time_t start_time_ms = tegrabl_get_timestamp_ms();

	while (true) {

		elapsed_time_ms = tegrabl_get_timestamp_ms() - start_time_ms;
		if (elapsed_time_ms > timeout_ms) {
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}

		val = phy->read(phy->mdio_addr, page, reg_addr);
		if ((!!(val & BIT(pos))) == set) {
			break;
		}
		tegrabl_mdelay(1000);
	}

fail:
	return err;
}

uint32_t tegrabl_phy_get_oui(struct phy_dev * const phy)
{
	uint32_t oui_msb;
	uint32_t oui_lsb;
	uint32_t val;

	/* page = 0, reg = 2 */
	oui_msb = phy->read(phy->mdio_addr, PAGE_PHY, REG_PHY_IDENTIFIER_1);
	pr_trace("OUI msb: 0x%08x\n", oui_msb);
	val = phy->read(phy->mdio_addr, PAGE_PHY, REG_PHY_IDENTIFIER_2);
	pr_trace("OUI lsb: 0x%08x\n", val);
	oui_lsb = BITFIELD_GET(val, REG_PHY_IDENTIFIER_2_WIDTH, REG_PHY_IDENTIFIER_2_SHIFT);
	pr_trace("OUI lsb: 0x%08x\n", oui_lsb);
	val = (oui_msb << 6) | oui_lsb;
	pr_trace("OUI    : 0x%08x\n", val);

	return val;
}
