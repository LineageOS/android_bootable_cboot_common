/*
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
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
#include <tegrabl_phy.h>

/************************************************************************************************************/
#define PHY_DEBUG										0

#define SET												1
#define RESET											0
#define BIT(p)											(1UL << p)
#define SET_BIT(val, p)									((val) | BIT(p))
#define CLEAR_BIT(val, p)								((val) & (~BIT(p)))
#define GET_BIT(val, pos)								((val >> pos) & 0x1)

#define COPPER_RESET_TIMEOUT_MS							3000
#define AUTO_NEG_TIMEOUT_MS								(15 * 1000)

#define PRINT_REG(page, reg)	pr_info("Page: %2u, Reg: %2u, val: 0x%04x\n", page, reg, phy->read(page, reg))

/************************************************************************************************************/
#define PAGE_COPPER										0

#define REG_COPPER_CONTROL								0
#define COPPER_CONTROL_RESET							15
#define COPPER_CONTROL_ENABLE_AUTO_NEG					12
#define COPPER_RESTART_AUTO_NEG							9

#define REG_COPPER_STATUS								1

#define REG_COPPER_AUTO_NEG_ADVERTISEMENT				4

#define REG_COPPER_LINK_PARTNER_ABILITY					5

#define REG_COPPER_AUTO_NEG_EXPANSION					6

#define REG_1000_BASE_T_STATUS							10

#define REG_COPPER_CONTROL1								16
#define COPPER_CONTROL1_ENABLE_AUTO_CROSSOVER			(BIT(6) | BIT(5))

#define REG_COPPER_STATUS1								17
#define COPPER_STATUS1_SPEED_WIDTH						((15 - 14) + 1)
#define COPPER_STATUS1_SPEED_SHIFT						14
/*
 * bits 15, 14
 * 00 = 10 Mbps
 * 01 = 100 Mbps
 * 10 = 1000 Mbps
 */
#define COPPER_STATUS1_SPEED_10_MBPS					0
#define COPPER_STATUS1_SPEED_100_MBPS					1
#define COPPER_STATUS1_SPEED_1000_MBPS					2
#define COPPER_STATUS1_DUPLEX_MODE						13
#define COPPER_STATUS1_LINK_STATUS						10

#define REG_COPPER_INTR_STATUS							19
#define COPPER_INTR_STATUS_AUTO_NEG_COMPLETED			11

/************************************************************************************************************/
#define PAGE_MAC										2

#define REG_MAC_CONTROL1								16
#define MAC_CONTROL1_ENABLE_RX_CLK						10
#define MAC_CONTROL1_PASS_ODD_NIBBLE_PREAMBLES			6
#define MAC_CONTROL1_RGMII_INTF_POWER_DOWN				3
#define MAC_CONTROL1_TX_FIFO_DEPTH_16_BITS				0
#define MAC_CONTROL1_TX_FIFO_DEPTH_24_BITS				BIT(14)
#define MAC_CONTROL1_TX_FIFO_DEPTH_32_BITS				BIT(15)
#define MAC_CONTROL1_TX_FIFO_DEPTH_40_BITS				(BIT(15) | BIT(14))

#define REG_MAC_CONTROL2								21
/*
 * Bits 6, 13
 * 00 = 10 Mbps
 * 01 = 100 Mbps
 * 10 = 1000 Mbps
 */
#define MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_10_MBPS		0
#define MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_100_MBPS	BIT(13)
#define MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_1000_MBPS	BIT(6)
#define MAC_CONTROL2_RGMII_RX_TIMING_CTRL				5
#define MAC_CONTROL2_RGMII_TX_TIMING_CTRL				4

/************************************************************************************************************/

struct tegrabl_phy_cp_status_reg {
	union {
		uint16_t val;
		struct {
			uint16_t extended_capability:1;
			uint16_t jabber_detected:1;
			uint16_t previos_link_status:1;
			uint16_t auto_neg_ability:1;
			uint16_t remote_fault:1;
			uint16_t auto_neg_complete:1;
			uint16_t mf_preamble_suppression:1;
			uint16_t reserved:1;
			uint16_t extended_status:1;
			uint16_t reserved_2:2;
			uint16_t ten_base_t_half_duplex:1;
			uint16_t ten_base_t_full_duplex:1;
			uint16_t hndrd_base_x_half_duplex:1;
			uint16_t hndrd_base_x_full_duplex:1;
			uint16_t reserved_3:1;
		};
	};
};

struct tegrabl_phy_cp_auto_neg_adv_reg {
	union {
		uint16_t val;
		struct {
			uint16_t selector_field:5;
			uint16_t ten_base_tx_half_duplex:1;
			uint16_t ten_base_tx_full_duplex:1;
			uint16_t hndrd_base_tx_half_duplex:1;
			uint16_t hndrd_base_tx_full_duplex:1;
			uint16_t hndrd_base_t4:1;
			uint16_t pause:1;
			uint16_t asymmetric_pause:1;
			uint16_t reserved:1;
			uint16_t remote_fault:1;
			uint16_t ack:1;
			uint16_t next_page:1;
		};
	};
};

struct tegrabl_phy_cp_link_ptnr_ability_reg {
	union {
		uint16_t val;
		struct {
			uint16_t selector_field:5;
			uint16_t ten_base_t_half_duplex:1;
			uint16_t ten_base_t_full_duplex:1;
			uint16_t hndrd_base_tx_half_duplex:1;
			uint16_t hndrd_base_tx_full_duplex:1;
			uint16_t hndrd_base_t4:1;
			uint16_t pause_capable:1;
			uint16_t asymmetric_pause:1;
			uint16_t tech_ability_field:1;
			uint16_t remote_fault:1;
			uint16_t ack:1;
			uint16_t next_page:1;
		};
	};
};

struct tegrabl_phy_cp_auto_neg_exp_reg {
	union {
		uint16_t val;
		struct {
			uint16_t link_ptnr_auto_neg_able:1;
			uint16_t page_recvd:1;
			uint16_t local_next_page_able:1;
			uint16_t link_ptnr_next_page_able:1;
			uint16_t parallel_detection_fault:1;
			uint16_t reserved:11;
		};
	};
};

struct tegrabl_phy_thsnd_base_reg {
	union {
		uint16_t val;
		struct {
			uint16_t idle_err_cnt:8;
			uint16_t reserved:2;
			uint16_t link_ptnr_half_duplex:1;
			uint16_t link_ptnr_full_duplex:1;
			uint16_t remote_receiver_status:1;
			uint16_t local_receiver_status:1;
			uint16_t ms_config_resolved:1;
			uint16_t master_slave_config_fault:1;
		};
	};
};

struct tegrabl_phy_cp_spec_status_1_reg {
	union {
		uint16_t val;
		struct {
			uint16_t jabber:1;
			uint16_t polarity:1;
			uint16_t dte_power_status:1;
			uint16_t global_link_status:1;
			uint16_t energy_detect_status:1;
			uint16_t downshift_status:1;
			uint16_t mdi_crossover_status:1;
			uint16_t reserved:1;
			uint16_t recv_pause:1;
			uint16_t transmit_pause:1;
			uint16_t link_status:1;
			uint16_t speed_and_duplex_rslvd:1;
			uint16_t page_recvd:1;
			uint16_t duplex:1;
			uint16_t speed:2;
		};
	};
};

/************************************************************************************************************/

static tegrabl_error_t wait_for_bit(const struct phy_dev * const phy, uint32_t page, uint32_t reg_addr,
									uint32_t pos, bool set, uint32_t timeout_ms)
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

		val = phy->read(page, reg_addr);
		if ((!!(val & BIT(pos))) == set) {
			break;
		}
		tegrabl_mdelay(1000);
	}

fail:
	return err;
}

#if PHY_DEBUG
static void tegrabl_phy_print_advertisement(const struct phy_dev * const phy)
{
	struct tegrabl_phy_cp_auto_neg_adv_reg reg;

	reg.val = phy->read(PAGE_COPPER, REG_COPPER_AUTO_NEG_ADVERTISEMENT);
	pr_info("\n");
	pr_info("[ Local PHY advertisement ]\n");
	pr_info("Remote fault           : %s\n", reg.remote_fault ? "yes" : "no");
	pr_info("Asymmetric pause       : %s\n", reg.asymmetric_pause ? "yes" : "no");
	pr_info("Pause capable          : %s\n", reg.pause ? "yes" : "no");
	pr_info("100base-t4             : %s\n", reg.hndrd_base_t4 ? "yes" : "no");
	pr_info("100base-tx full duplex : %s\n", reg.hndrd_base_tx_full_duplex ? "yes" : "no");
	pr_info("100base-tx half duplex : %s\n", reg.hndrd_base_tx_half_duplex ? "yes" : "no");
	pr_info("10base-tx  full duplex : %s\n", reg.ten_base_tx_full_duplex ? "yes" : "no");
	pr_info("10base-tx  half duplex : %s\n", reg.ten_base_tx_half_duplex ? "yes" : "no");
	pr_info("\n");
}

static void tegrabl_phy_print_local_phy_status_n_abilities(const struct phy_dev * const phy)
{
	struct tegrabl_phy_cp_status_reg cp_status_reg;
	struct tegrabl_phy_cp_auto_neg_exp_reg cp_auto_neg_exp_reg;
	struct tegrabl_phy_thsnd_base_reg thsnd_base_reg;
	struct tegrabl_phy_cp_spec_status_1_reg cp_spec_status_1_reg;

	cp_status_reg.val = phy->read(PAGE_COPPER, REG_COPPER_STATUS);
	cp_auto_neg_exp_reg.val = phy->read(PAGE_COPPER, REG_COPPER_AUTO_NEG_EXPANSION);
	thsnd_base_reg.val = phy->read(PAGE_COPPER, REG_1000_BASE_T_STATUS);
	cp_spec_status_1_reg.val = phy->read(PAGE_COPPER, REG_COPPER_STATUS1);

	pr_info("\n");
	pr_info("[ Local PHY status and abilities ]\n");
	pr_info("Auto-negotiation complete : %s\n", cp_status_reg.auto_neg_complete ? "yes" : "no");
	pr_info("1000base-t full duplex    : %s\n", thsnd_base_reg.link_ptnr_full_duplex ? "yes" : "no");
	pr_info("1000base-t half duplex    : %s\n", thsnd_base_reg.link_ptnr_half_duplex ? "yes" : "no");
	pr_info("100base-x  full duplex    : %s\n", cp_status_reg.hndrd_base_x_full_duplex ? "yes" : "no");
	pr_info("100base-x  half duplex    : %s\n", cp_status_reg.hndrd_base_x_half_duplex ? "yes" : "no");
	pr_info("10base-t   full duplex    : %s\n", cp_status_reg.ten_base_t_full_duplex ? "yes" : "no");
	pr_info("10base-t   half duplex    : %s\n", cp_status_reg.ten_base_t_half_duplex ? "yes" : "no");
	pr_info("Receiver status           : %s\n", thsnd_base_reg.local_receiver_status ? "yes" : "no");
	pr_info("Remote fault detected     : %s\n", cp_status_reg.remote_fault ? "yes" : "no");
	pr_info("Parallel detection fault  : %s\n", cp_auto_neg_exp_reg.parallel_detection_fault ? "yes" : "no");
	pr_info("PHY config resolved to    : %s\n", thsnd_base_reg.ms_config_resolved ? "master" : "slave");
	pr_info("Master/slave config fault : %s\n", thsnd_base_reg.master_slave_config_fault ? "yes" : "no");
	pr_info("MDI crossover status      : %s\n", cp_spec_status_1_reg.mdi_crossover_status ? "MDIX" : "MDI");
	pr_info("Transmit pause            : %s\n", cp_spec_status_1_reg.transmit_pause ? "enabled" : "disabled");
	pr_info("Receive  pause            : %s\n", cp_spec_status_1_reg.recv_pause ? "enabled" : "disabled");
	pr_info("Jabber                    : %s\n", cp_spec_status_1_reg.jabber ? "yes" : "no");
	pr_info("Polarity                  : %s\n", cp_spec_status_1_reg.polarity ? "reversed" : "normal");
	pr_info("Link status               : %s\n", cp_spec_status_1_reg.link_status ? "up" : "down");
	pr_info("Speed and duplex resolved : %s\n", cp_spec_status_1_reg.speed_and_duplex_rslvd ? "yes" : "no");
	pr_info("Duplex                    : %s\n", cp_spec_status_1_reg.duplex ? "full" : "half");
	if (cp_spec_status_1_reg.speed == 0x2) {
		pr_info("Speed                     : 1000 mbps\n");
	} else if (cp_spec_status_1_reg.speed == 0x1) {
		pr_info("Speed                     : 100 mbps\n");
	} else if (cp_spec_status_1_reg.speed == 0x0) {
		pr_info("Speed                     : 10 mbps\n");
	} else {
	}
	pr_info("\n");
}

static void tegrabl_phy_print_link_ptnr_phy_status_n_abilities(const struct phy_dev * const phy)
{
	struct tegrabl_phy_cp_link_ptnr_ability_reg cp_link_ptnr_ability_reg;
	struct tegrabl_phy_cp_auto_neg_exp_reg cp_auto_neg_exp_reg;
	struct tegrabl_phy_thsnd_base_reg thsnd_base_reg;

	cp_link_ptnr_ability_reg.val = phy->read(PAGE_COPPER, REG_COPPER_LINK_PARTNER_ABILITY);
	cp_auto_neg_exp_reg.val = phy->read(PAGE_COPPER, REG_COPPER_AUTO_NEG_EXPANSION);
	thsnd_base_reg.val = phy->read(PAGE_COPPER, REG_1000_BASE_T_STATUS);

	pr_info("\n");
	pr_info("[ Link partner PHY status and abilities ]\n");
	pr_info("Auto-negotiation able  : %s\n", cp_auto_neg_exp_reg.link_ptnr_auto_neg_able ? "yes" : "no");
	pr_info("100base-t4             : %s\n", cp_link_ptnr_ability_reg.hndrd_base_t4 ? "yes" : "no");
	pr_info("100base-tx full duplex : %s\n", cp_link_ptnr_ability_reg.hndrd_base_tx_full_duplex ? "yes" : "no");
	pr_info("100base-tx half duplex : %s\n", cp_link_ptnr_ability_reg.hndrd_base_tx_half_duplex ? "yes" : "no");
	pr_info("10base-t   full duplex : %s\n", cp_link_ptnr_ability_reg.ten_base_t_full_duplex ? "yes" : "no");
	pr_info("10base-t   half duplex : %s\n", cp_link_ptnr_ability_reg.ten_base_t_half_duplex ? "yes" : "no");
	pr_info("Receiver status        : %s\n", thsnd_base_reg.remote_receiver_status ? "yes" : "no");
	pr_info("Remote fault detected  : %s\n", cp_link_ptnr_ability_reg.remote_fault ? "yes" : "no");
	pr_info("Asymmetric pause       : %s\n", cp_link_ptnr_ability_reg.asymmetric_pause ? "yes" : "no");
	pr_info("Pause capable          : %s\n", cp_link_ptnr_ability_reg.pause_capable ? "yes" : "no");
	pr_info("\n");
}
#endif

static tegrabl_error_t tegrabl_phy_cp_reset(const struct phy_dev * const phy)
{
	uint32_t val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	val = phy->read(PAGE_COPPER, REG_COPPER_CONTROL);
	val = SET_BIT(val, COPPER_CONTROL_RESET);
	err = wait_for_bit(phy, PAGE_COPPER, REG_COPPER_CONTROL, COPPER_CONTROL_RESET, RESET,
					   COPPER_RESET_TIMEOUT_MS);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Software PHY reset couldn't complete within timeout (%u ms)\n", COPPER_RESET_TIMEOUT_MS);
	}

	return err;
}

void tegrabl_phy_config(const struct phy_dev * const phy)
{
	uint32_t val;

	pr_info("Configuring PHY\n");

	/* Program Page: 2, Register: 0 */
	phy->write(PAGE_MAC, REG_COPPER_CONTROL, 0);

	tegrabl_phy_cp_reset(phy);

	/* Program Page: 2, Register: 16 */
	val = MAC_CONTROL1_TX_FIFO_DEPTH_24_BITS;
	val = SET_BIT(val, MAC_CONTROL1_ENABLE_RX_CLK);
	val = SET_BIT(val, MAC_CONTROL1_PASS_ODD_NIBBLE_PREAMBLES);
	val = SET_BIT(val, MAC_CONTROL1_RGMII_INTF_POWER_DOWN);
	phy->write(PAGE_MAC, REG_MAC_CONTROL1, val);

	/* Program Page: 2, Register: 21 */
	val = MAC_CONTROL2_DEFAULT_MAC_INTF_SPEED_1000_MBPS;   /* MAC interface speed during link down */
	val = SET_BIT(val, MAC_CONTROL2_RGMII_RX_TIMING_CTRL);
	val = SET_BIT(val, MAC_CONTROL2_RGMII_TX_TIMING_CTRL);
	phy->write(PAGE_MAC, REG_MAC_CONTROL2, val);

	/* Program Page: 0, Register: 16 */
	/* Automatically detect whether it needs to crossover between pairs or not */
	val = COPPER_CONTROL1_ENABLE_AUTO_CROSSOVER;
	phy->write(PAGE_COPPER, REG_COPPER_CONTROL1, val);
}

tegrabl_error_t tegrabl_phy_auto_neg(const struct phy_dev * const phy)
{
	uint32_t val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("Start auto-negotiation\n");

	val = phy->read(PAGE_COPPER, REG_COPPER_CONTROL);
	val = SET_BIT(val, COPPER_CONTROL_ENABLE_AUTO_NEG);
	val = SET_BIT(val, COPPER_RESTART_AUTO_NEG);
	phy->write(PAGE_COPPER, REG_COPPER_CONTROL, val | BIT(COPPER_CONTROL_RESET));
	err = wait_for_bit(phy, PAGE_COPPER, REG_COPPER_CONTROL, COPPER_CONTROL_RESET, RESET,
					   COPPER_RESET_TIMEOUT_MS);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Software PHY reset couldn't complete within timeout (%u ms)\n", COPPER_RESET_TIMEOUT_MS);
		goto fail;
	}

#if PHY_DEBUG
	tegrabl_phy_print_advertisement(phy);
#endif

	pr_info("Wait till auto-negotiation completes...\n");
	err = wait_for_bit(phy, PAGE_COPPER, REG_COPPER_INTR_STATUS, COPPER_INTR_STATUS_AUTO_NEG_COMPLETED, SET,
					   AUTO_NEG_TIMEOUT_MS);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Auto-negotiation couldn't complete within timeout (%u ms)\n", AUTO_NEG_TIMEOUT_MS);
	}

#if PHY_DEBUG
	tegrabl_phy_print_local_phy_status_n_abilities(phy);
	tegrabl_phy_print_link_ptnr_phy_status_n_abilities(phy);
#endif

fail:
	return err;
}

void tegrabl_phy_detect_link(struct phy_dev * const phy)
{
	uint32_t val = 0;
	uint32_t speed_code;

	val = phy->read(PAGE_COPPER, REG_COPPER_STATUS1);

	phy->is_link_up = GET_BIT(val, COPPER_STATUS1_LINK_STATUS);
	phy->duplex_mode = GET_BIT(val, COPPER_STATUS1_DUPLEX_MODE);

	speed_code = BITFIELD_GET(val, COPPER_STATUS1_SPEED_WIDTH, COPPER_STATUS1_SPEED_SHIFT);
	if (speed_code == COPPER_STATUS1_SPEED_1000_MBPS) {
		phy->speed = 1000;
	} else if (speed_code == COPPER_STATUS1_SPEED_100_MBPS) {
		phy->speed = 100;
	} else if (speed_code == COPPER_STATUS1_SPEED_10_MBPS) {
		phy->speed = 10;
	} else {
		phy->speed = 0;
	}
}
