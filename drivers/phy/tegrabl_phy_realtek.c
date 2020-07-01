/*
 * Copyright (c) 2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PHY

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <tegrabl_utils.h>
#include <tegrabl_phy.h>
#include <tegrabl_phy_realtek.h>

/************************************************************************************************************/
#define PAGE_0								0

/***************************************************************/
#define REG_BMCR							0
#define BMCR_ANE							12
#define BMCR_RESTART_AN						9

/***************************************************************/
#define REG_BMSR							1
#define BMSR_AUTO_NEG_COMPLETED				5

/***************************************************************/
#define REG_ANAR							4
#define ANAR_100_BASE_T4					9
#define ANAR_100_BASE_TX_FULL_DUPLEX		8
#define ANAR_100_BASE_TX_HALF_DUPLEX		7
#define ANAR_10_BASE_T_FULL_DUPLEX			6
#define ANAR_10_BASE_T_HALF_DUPLEX			5

/***************************************************************/
#define REG_GBCR							9
#define GBCR_ADVT_1000_BASE_T_FULL_DUPLEX	9

/************************************************************************************************************/
#define PAGE_A43							0xA43

/***************************************************************/
#define REG_PHYSR							26
#define PHYSR_SPEED_WIDTH					((5 - 4) + 1)
#define PHYSR_SPEED_SHIFT					4
#define PHYSR_DUPLEX_MODE					3
#define PHYSR_LINK							2

/***************************************************************/
#define REG_PAGSR							31

/************************************************************************************************************/
#define PAGE_LED							0xd04

/***************************************************************/
#define REG_LCR								16
#define LCR_LED1_ACT						9
#define LCR_LED1_LINK_1000					8
#define LCR_LED1_LINK_100					6
#define LCR_LED1_LINK_10					5
#define LCR_LED0_LINK_1000					3

/***************************************************************/
#define REG_EEELCR							17

/************************************************************************************************************/
#define PHYSR_SPEED_1000_MBPS				2
#define PHYSR_SPEED_100_MBPS				1
#define PHYSR_SPEED_10_MBPS					0

#define AUTO_NEG_TIMEOUT_MS					(15 * 1000)
/************************************************************************************************************/

void tegrabl_phy_realtek_config(struct phy_dev * const phy)
{
	uint32_t val;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	phy->page_sel_reg = REG_PAGSR;

	/* Enable link and activity indication for all speeds on LED1 and LED0 for GBE */
	val = phy->read(phy->mdio_addr, PAGE_LED, REG_LCR);
	val = SET_BIT(val, LCR_LED1_ACT);
	val = SET_BIT(val, LCR_LED1_LINK_1000);
	val = SET_BIT(val, LCR_LED1_LINK_100);
	val = SET_BIT(val, LCR_LED1_LINK_10);
	val = SET_BIT(val, LCR_LED0_LINK_1000);
	phy->write(phy->mdio_addr, PAGE_LED, REG_LCR, val);

	/* Disable Energy Efficient Ethernet (EEE) LED indication */
	phy->write(phy->mdio_addr, PAGE_LED, REG_EEELCR, 0);
}

tegrabl_error_t tegrabl_phy_realtek_auto_neg(const struct phy_dev * const phy)
{
	uint32_t val;
	tegrabl_error_t err;

	/* Advertise 1000 MBPS full duplex mode */
	val = phy->read(phy->mdio_addr, PAGE_0, REG_GBCR);
	val = SET_BIT(val, GBCR_ADVT_1000_BASE_T_FULL_DUPLEX);
	phy->write(phy->mdio_addr, PAGE_0, REG_GBCR, val);

	/* Advertise 100, 10 MBPS with full and half duplex mode */
	val = phy->read(phy->mdio_addr, PAGE_0, REG_ANAR);
	val = SET_BIT(val, ANAR_100_BASE_T4);
	val = SET_BIT(val, ANAR_100_BASE_TX_FULL_DUPLEX);
	val = SET_BIT(val, ANAR_100_BASE_TX_HALF_DUPLEX);
	val = SET_BIT(val, ANAR_10_BASE_T_FULL_DUPLEX);
	val = SET_BIT(val, ANAR_10_BASE_T_HALF_DUPLEX);
	phy->write(phy->mdio_addr, PAGE_0, REG_ANAR, val);

	pr_info("Start auto-negotiation\n");
	val = phy->read(phy->mdio_addr, PAGE_0, REG_BMCR);
	val = SET_BIT(val, BMCR_ANE);
	val = SET_BIT(val, BMCR_RESTART_AN);
	phy->write(phy->mdio_addr, PAGE_0, REG_BMCR, val);

	pr_info("Wait till it completes...\n");
	err = tegrabl_phy_wait_for_bit(phy, PAGE_0, REG_BMSR, BMSR_AUTO_NEG_COMPLETED, SET, AUTO_NEG_TIMEOUT_MS);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Couldn't complete within timeout (%u ms)\n", AUTO_NEG_TIMEOUT_MS);
	}

	return err;
}

void tegrabl_phy_realtek_detect_link(struct phy_dev * const phy)
{
	uint32_t val =0;
	uint32_t speed_code;

	pr_trace("%s()\n", __func__);

	val = phy->read(phy->mdio_addr, PAGE_A43, REG_PHYSR);
	phy->is_link_up = GET_BIT(val, PHYSR_LINK);
	phy->duplex_mode = GET_BIT(val, PHYSR_DUPLEX_MODE);

	/* Adjust MAC speed */
	speed_code = BITFIELD_GET(val, PHYSR_SPEED_WIDTH, PHYSR_SPEED_SHIFT);
	if (speed_code == PHYSR_SPEED_1000_MBPS) {
		phy->speed = 1000;
	} else if (speed_code == PHYSR_SPEED_100_MBPS) {
		phy->speed = 100;
	} else if (speed_code == PHYSR_SPEED_10_MBPS) {
		phy->speed = 10;
	} else {
		phy->speed = 0;
	}
}
