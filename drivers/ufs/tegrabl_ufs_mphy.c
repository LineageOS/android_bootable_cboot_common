/*
 * Copyright (c) 2016-2018 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_ufs_local.h>
#include <tegrabl_ufs_hci.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <tegrabl_drf.h>
#include <address_map_new.h>

#ifndef _MK_ADDR_CONST
#define _MK_ADDR_CONST(_constant_) _constant_
#endif

#define FUSE_MPHY_NV_CALIB_0_IDLE_DETECTOR_RANGE       ((1) : (0))
#define FUSE_MPHY_NV_CALIB_0_PWM_DETECTOR_3_2_RANGE    ((3) : (2))
#define FUSE_MPHY_NV_CALIB_0_PWM_DETECTOR_4_RANGE      ((5) : (4))
#define MPHY_RX_APB_CAPABILITY_98_9B_0_RX_ADVANCED_MIN_ACTIVATETIME_CAPABILITY_RANGE (19) : (16)
#define MPHY_RX_APB_CAPABILITY_8C_8F_0_RX_MIN_ACTIVATETIME_CAPABILITY_RANGE (27) : (24)
#define MPHY_RX_APB_CAPABILITY_98_9B_0_RX_ADVANCED_STEP_SIZE_GRANULARITY_CAPABILITY_RANGE (2) : (1)
#define MPHY_RX_APB_CAPABILITY_98_9B_0_RX_ADVANCED_FINE_GRANULARITY_CAPABILITY_RANGE (0) : (0)
#define MPHY_RX_APB_CAPABILITY_88_8B_0                  _MK_ADDR_CONST(0x88)
#define MPHY_RX_APB_CAPABILITY_8C_8F_0                  _MK_ADDR_CONST(0x8c)
#define MPHY_RX_APB_CAPABILITY_98_9B_0                  _MK_ADDR_CONST(0x98)
#define MPHY_RX_APB_VENDOR2_0                   _MK_ADDR_CONST(0x184)
#define MPHY_RX_APB_VENDOR2_0_REGS_UPDATE_RANGE        (0) : (0)
#define MPHY_RX_APB_VENDOR2_0_RX_CAL_EN_RANGE          (15) : (15)

#define RX_HS_G1_SYNC_LENGTH_CAPABILITY(x)	(((x) & 0x3f) << 24)
#define RX_HS_G2_SYNC_LENGTH_CAPABILITY(x)	(((x) & 0x3f) << 0)
#define RX_HS_G3_SYNC_LENGTH_CAPABILITY(x)	(((x) & 0x3f) << 8)
#define MPHY_RX_APB_CAPABILITY_94_97_0		0x94
#define RX_HS_SYNC_LENGTH			0xf

tegrabl_error_t tegrabl_ufs_link_mphy_setup(void)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t reg_data;
	uint32_t data;

	/*Update HS_G1 Sync Length MPHY_RX_APB_CAPABILITY_88_8B_0*/
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L0_BASE +
			MPHY_RX_APB_CAPABILITY_88_8B_0);
	reg_data &= ~RX_HS_G1_SYNC_LENGTH_CAPABILITY(~0);
	reg_data |= RX_HS_G1_SYNC_LENGTH_CAPABILITY(RX_HS_SYNC_LENGTH);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L0_BASE +
		MPHY_RX_APB_CAPABILITY_88_8B_0, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L1_BASE +
		MPHY_RX_APB_CAPABILITY_88_8B_0, reg_data);

	/*Update HS_G2&G3 Sync Length MPHY_RX_APB_CAPABILITY_94_97_0*/
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L0_BASE +
			MPHY_RX_APB_CAPABILITY_94_97_0);
	reg_data &= ~RX_HS_G2_SYNC_LENGTH_CAPABILITY(~0);
	reg_data |= RX_HS_G2_SYNC_LENGTH_CAPABILITY(RX_HS_SYNC_LENGTH);
	reg_data &= ~RX_HS_G3_SYNC_LENGTH_CAPABILITY(~0);
	reg_data |= RX_HS_G3_SYNC_LENGTH_CAPABILITY(RX_HS_SYNC_LENGTH);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L0_BASE +
		MPHY_RX_APB_CAPABILITY_94_97_0, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L1_BASE +
		MPHY_RX_APB_CAPABILITY_94_97_0, reg_data);

	/*  RX_Min_ActivateTime_Capability = 5 (500 us) */
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L0_BASE +
			MPHY_RX_APB_CAPABILITY_8C_8F_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_8C_8F,
			RX_MIN_ACTIVATETIME_CAPABILITY, 0x5, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L0_BASE +
		MPHY_RX_APB_CAPABILITY_8C_8F_0, reg_data);

	/*  RX_Min_ActivateTime_Capability = 5 (500 us) */
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L1_BASE +
			MPHY_RX_APB_CAPABILITY_8C_8F_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_8C_8F,
			RX_MIN_ACTIVATETIME_CAPABILITY, 0x5, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L1_BASE +
		MPHY_RX_APB_CAPABILITY_8C_8F_0, reg_data);


	/*  RX_Advanced_Step_Size_Granularity_Capability = 0 */
	/*  RX_Advanced_Fine_Granularity_Capability = 0 */
	/*  RX_Advanced_Min_ActivateTime_Capability = 10 steps of 8 us (80 us)*/
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L0_BASE +
			MPHY_RX_APB_CAPABILITY_98_9B_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_98_9B,
			RX_ADVANCED_MIN_ACTIVATETIME_CAPABILITY, 0xa, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_98_9B,
			RX_ADVANCED_STEP_SIZE_GRANULARITY_CAPABILITY,
			0x0, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_98_9B,
			RX_ADVANCED_FINE_GRANULARITY_CAPABILITY,
			0x0, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L0_BASE +
		MPHY_RX_APB_CAPABILITY_98_9B_0, reg_data);
	reg_data =
		NV_READ32(NV_ADDRESS_MAP_MPHY_L1_BASE + MPHY_RX_APB_CAPABILITY_98_9B_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_98_9B,
			RX_ADVANCED_MIN_ACTIVATETIME_CAPABILITY, 0xa, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_98_9B,
			RX_ADVANCED_STEP_SIZE_GRANULARITY_CAPABILITY,
			0x0, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, CAPABILITY_98_9B,
			RX_ADVANCED_FINE_GRANULARITY_CAPABILITY,
			0x0, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L1_BASE +
		MPHY_RX_APB_CAPABILITY_98_9B_0, reg_data);

        data = 0x0;
	e = tegrabl_ufs_set_dme_command(DME_SET, 0, pa_local_tx_lcc_enable, &data);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("setting pa_local_tx_lcc_enable failed\n");
		return e;
	}

	/* Before link start configuration request from Host controller
	 * burst closure delay needs to be configured to 0
	*/
	data = 0x0;
	e = tegrabl_ufs_set_dme_command(DME_SET, 0,
		vs_txburstclosuredelay, &data);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("setting burst closure delay failed\n");
		return e;
	}

	/* REG UPDATE */
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L0_BASE + MPHY_RX_APB_VENDOR2_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, VENDOR2, REGS_UPDATE,
			0x1, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L0_BASE + MPHY_RX_APB_VENDOR2_0, reg_data);

	tegrabl_mdelay(10);
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L1_BASE + MPHY_RX_APB_VENDOR2_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, VENDOR2,
			REGS_UPDATE, 0x1, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L1_BASE + MPHY_RX_APB_VENDOR2_0, reg_data);

	tegrabl_mdelay(10);
	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L1_BASE + MPHY_RX_APB_VENDOR2_0);

        data = 0x0;
	reg_data = UFS_READ32(HCS);
	if (reg_data != 0x0000000f  && reg_data != 0x0000010f) {
		e = tegrabl_ufs_set_dme_command(DME_LINKSTARTUP, 0, 0, &data);
		if (e != TEGRABL_NO_ERROR) {
			pr_error("link startup dme_set failed\n");
			return e;
		}
	}
	reg_data = 0;
	while (reg_data != 0x0000000f  && reg_data != 0x0000010f) {
		reg_data = UFS_READ32(HCS);
	}
	pr_info("link status is %0x\n", reg_data);

	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L0_BASE +
			MPHY_RX_APB_VENDOR2_0);

	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, VENDOR2, RX_CAL_EN,
                       0x1, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L0_BASE +
			MPHY_RX_APB_VENDOR2_0, reg_data);

	tegrabl_mdelay(10);

	reg_data = NV_READ32(NV_ADDRESS_MAP_MPHY_L1_BASE + MPHY_RX_APB_VENDOR2_0);
	reg_data = NV_FLD_SET_DRF_NUM(MPHY_RX_APB, VENDOR2,
		RX_CAL_EN, 0x1, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_MPHY_L1_BASE + MPHY_RX_APB_VENDOR2_0, reg_data);
	tegrabl_mdelay(10);

	return TEGRABL_NO_ERROR;
}
