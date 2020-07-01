/*
* Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_UFS

#include <tegrabl_ufs_defs.h>
#include "tegrabl_ufs_local.h"
#include <address_map_new.h>
#include <tegrabl_ufs_otp.h>
#include <tegrabl_ufs_hci.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_clock.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_utils.h>
#include <tegrabl_ufs_int.h>
#include <tegrabl_clk_rst_soc.h>
#include <tegrabl_clk_ufs.h>
#include <tegrabl_drf.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <arfuse.h>

/* Global structure pointers */

static struct transfer_request_descriptor *ptx_rx_desc;
static struct task_mgmt_request_descriptor *ptask_mgmnt_desc;
static struct cmd_descriptor *pcmd_descriptor;
static struct tegrabl_ufs_context *pufs_context;

/* Global structures */
static struct tegrabl_ufs_params pufs_params;
static struct tegrabl_ufs_internal_params pufs_internal_params;

#define TARGET_SUCCESS 0x0
#define TARGET_FAILURE 0x1
#define BLOCK_SIZE 4096
#define ALIGN_1K 1024
#define MAX_BLOCKS 64
#define FDEV_INIT_CLR_MAX_TIME 60000000 /*1 min */

static uint8_t ufs_granularity_us[] = {
	0,
	1,
	4,
	8,
	16,
	32,
	100
};

static uint8_t ufs_tactivate_64us[] = {
	0,
	64,
	16,
	8,
	4,
	2,
	1
};

/** 4 sets of timing/threshold values for PWM G1 ~ G4 */
static uint16_t dme_afc0_req_timeout_val_x1[] = {
	1533,
	766,
	383,
	191
};

static uint16_t dme_afc0_req_timeout_val_x2[] = {
	833,
	441,
	220,
	110
};

static uint16_t dme_tc0_replay_timeout_val_x1[] = {
	2986,
	1493,
	746,
	373
};

static uint16_t dme_tc0_replay_timeout_val_x2[] = {
	1746,
	873,
	436,
	218
};

static uint16_t dme_fc0_protection_timeout_val_x1[] = {
	2786,
	1393,
	696,
	348
};

static uint16_t dme_fc0_protection_timeout_val_x2[] = {
	1620,
	810,
	405,
	202
};

tegrabl_error_t
tegrabl_ufs_get_cmd_descriptor(uint32_t *cmd_desc_index)
{
	uint32_t next_cmd_index;

	if (pufs_context->cmd_desc_in_use < MAX_CMD_DESC_NUM) {
		next_cmd_index =
			NEXT_CD_IDX(pufs_context->last_cmd_desc_index);
		pufs_context->last_cmd_desc_index = next_cmd_index;
		*cmd_desc_index = next_cmd_index;
		pufs_context->cmd_desc_in_use++;
		return TEGRABL_NO_ERROR;
	} else
		return TEGRABL_ERROR(TEGRABL_ERR_RESOURCE_MAX, 0U);
}

static void unipro_dump_regs(void)
{
	int cnt;
	uint32_t reg_offset, reg_value;
	int ret;

	/* Phy layer */
	cnt = 0x0;
	while (cnt <= 0xC2) {
		reg_offset = 0x1500 + cnt;
		reg_value = 0xCAFDECAF;
		ret = tegrabl_ufs_set_dme_command(DME_GET, 0,
			reg_offset, &reg_value);
		if (ret != TEGRABL_NO_ERROR)
			pr_info("Unipro Reg offset: 0x%X, val: 0x%X\n",
				reg_offset, reg_value);
		cnt++;
	}

	/* DL layer */
	cnt = 0x0;
	while (cnt <= 0x67) {
		reg_offset = 0x2000 + cnt;
		reg_value = 0xCAFDECAF;
		ret = tegrabl_ufs_set_dme_command(DME_GET, 0,
			reg_offset, &reg_value);
		if (ret != TEGRABL_NO_ERROR)
			pr_info("Unipro Reg offset: 0x%X, val: 0x%X\n",
				reg_offset, reg_value);
		cnt++;
	}

	/* Alternative */
	cnt = 0x0;
	while (cnt <= 0x5) {
		reg_offset = 0xD041 + cnt;
		reg_value = 0xCAFDECAF;
		ret = tegrabl_ufs_set_dme_command(DME_GET, 0,
			reg_offset, &reg_value);
		if (ret != TEGRABL_NO_ERROR)
			pr_info("Unipro Reg offset: 0x%X, val: 0x%X\n",
				reg_offset, reg_value);
		cnt++;
	}
}

static void tegrabl_dump_ufs_regs(void)
{
	int cnt;

	/* Dump registers */
	cnt = 0x0;
	while (cnt <= 0x64) {
		pr_info("UFSHC Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2450000 + cnt));
		cnt += 4;
	}
	/* 0x0 is secure looks like, causes exception */
	cnt = 0x4;
	while (cnt <= 0x18) {
		pr_info("NV UFSHC Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2460000 + cnt));
		cnt += 4;
	}
	/* Lane 0 */
	/* Break this up */
	cnt = 0x0;
	while (cnt < 0x14) {
		pr_info("L0 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2470000 + cnt));
		cnt += 4;
	}
	cnt = 0x20;
	while (cnt < 0x3C) {
		pr_info("L0 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2470000 + cnt));
		cnt += 4;
	}
	cnt = 0x40;
	pr_info("L0 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
		NV_READ32(0x2470000 + cnt));
	cnt = 0x60;
	while (cnt < 0x68) {
		pr_info("L0 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2470000 + cnt));
		cnt += 4;
	}
	/* Rx */
	cnt = 0x80;
	while (cnt < 0x9c) {
		pr_info("L0 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2470000 + cnt));
		cnt += 4;
	}
	cnt = 0xA0;
	while (cnt < 0xAC) {
		pr_info("L0 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2470000 + cnt));
		cnt += 4;
	}
	cnt = 0xC0;
	pr_info("L0 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
		NV_READ32(0x2470000 + cnt));
	cnt = 0xD0;
	while (cnt < 0xE8) {
		pr_info("L0 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2470000 + cnt));
		cnt += 4;
	}

	/* Lane 1 */
	cnt = 0x0;
	while (cnt < 0x14) {
		pr_info("L1 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2480000 + cnt));
		cnt += 4;
	}
	cnt = 0x20;
	while (cnt < 0x3C) {
		pr_info("L1 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2480000 + cnt));
		cnt += 4;
	}
	cnt = 0x40;
	pr_info("L1 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
		NV_READ32(0x2480000 + cnt));
	cnt = 0x60;
	while (cnt < 0x68) {
		pr_info("L1 MPHY TX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2480000 + cnt));
		cnt += 4;
	}
	/* Rx */
	cnt = 0x80;
	while (cnt < 0x9c) {
		pr_info("L1 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2480000 + cnt));
		cnt += 4;
	}
	cnt = 0xA0;
	while (cnt < 0xAC) {
		pr_info("L1 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2480000 + cnt));
		cnt += 4;
	}
	cnt = 0xC0;
	pr_info("L1 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
		NV_READ32(0x2480000 + cnt));
	cnt = 0xD0;
	while (cnt < 0xE8) {
		pr_info("L1 MPHY RX Reg offset: 0x%X, val: 0x%X\n", cnt,
			NV_READ32(0x2480000 + cnt));
		cnt += 4;
	}

	/* Print Unipro regs */
	unipro_dump_regs();
}

static void tegrabl_clear_err_regs(void)
{
	/* Clear any pending interrupts - write to clear */
	UFS_WRITE32(IS, 0xFFFFFFFF);

	/* Clear read to clear registers */
	NV_READ32(UECPA);
	NV_READ32(UECDL);
	NV_READ32(UECN);
	NV_READ32(UECT);
	NV_READ32(UECDME);
}

tegrabl_error_t
tegrabl_ufs_pollfield(uint32_t reg_addr,
			uint32_t mask,
			uint32_t expected_value,
			uint32_t timeout)
{
	uint32_t reg_data;
	do {
		reg_data = NV_READ32(reg_addr);
		if ((reg_data & mask) == expected_value)
			return TEGRABL_NO_ERROR;

		tegrabl_udelay(1);
		timeout--;
	} while (timeout);
	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0U);
}

tegrabl_error_t tegrabl_ufs_set_activate_time()
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t data;
	uint32_t local_granularity = 0;
	uint32_t local_tactivate = 0;
	uint32_t tactivate;

	e = tegrabl_ufs_set_dme_command(DME_GET, 0,
		pa_granularity, &local_granularity);
	if (e != TEGRABL_NO_ERROR)
		return e;

	e = tegrabl_ufs_set_dme_command(DME_GET, 0,
		pa_tactivate, &local_tactivate);
	if (e != TEGRABL_NO_ERROR)
		return e;

	tactivate = ufs_granularity_us[local_granularity] * local_tactivate;
	if (tactivate < 64) {
		data = ufs_tactivate_64us[local_granularity] & 0xff;
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_tactivate, &data);
		if (e != TEGRABL_NO_ERROR)
			return e;
	}
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t
tegrabl_ufs_set_dme_command(uint8_t cmd_op,
				uint16_t gen_sel_idx,
				uint16_t mib_attr,
				uint32_t *data)
{
	struct dme_cmd vdme_cmd;
	struct dme_cmd *pdme_cmd;

	pdme_cmd = &vdme_cmd;
	uint32_t shift_expected_value;
	tegrabl_error_t error;

	memset((void *)pdme_cmd, 0, sizeof(struct dme_cmd));
	pdme_cmd->uic_cmd.cmdop = cmd_op;
	pdme_cmd->uic_cmd_arg1.mib_attribute = mib_attr;
	pdme_cmd->uic_cmd_arg1.gen_selector_index = gen_sel_idx;
	pdme_cmd->read_write_value = *data;

	UFS_WRITE32(UICCMDARG1, pdme_cmd->uic_cmd_arg1.dw);

	if (pdme_cmd->uic_cmd.cmdop == DME_SET ||
		pdme_cmd->uic_cmd.cmdop == DME_PEER_SET) {
		UFS_WRITE32(UICCMDARG2, pdme_cmd->uic_cmd_arg2.dw);
		UFS_WRITE32(UICCMDARG3, pdme_cmd->read_write_value);
	}

	UFS_WRITE32(UICCMD, pdme_cmd->uic_cmd.dw);

	if ((mib_attr == pa_pwr_mode) && (pdme_cmd->uic_cmd.cmdop == DME_SET)) {
		shift_expected_value = 1 << SHIFT(IS_UPMS);
		error = tegrabl_ufs_pollfield(IS,
			SHIFT_MASK(IS_UPMS),
			shift_expected_value, IS_UPMS_TIMEOUT);
		pdme_cmd->read_write_value = UFS_READ32(HCS);
	} else {
		shift_expected_value = 1 << SHIFT(IS_UCCS);
		error = tegrabl_ufs_pollfield(IS,
			SHIFT_MASK(IS_UCCS),
			shift_expected_value, IS_UCCS_TIMEOUT);
	}
	if (error != TEGRABL_NO_ERROR) {
		pr_error("UPMS or UCCS was not set for attr 0x%x set/get %d\n",
			 mib_attr, cmd_op);
		return error;
	}
	UFS_WRITE32(IS, shift_expected_value);

	pdme_cmd->uic_cmd_arg2.dw = UFS_READ32(UICCMDARG2);

	if (pdme_cmd->uic_cmd_arg2.config_error_code)
		return TEGRABL_ERROR(TEGRABL_ERR_DME_COMMAND, 0U);

	if ((pdme_cmd->uic_cmd.cmdop == DME_GET) ||
		(pdme_cmd->uic_cmd.cmdop == DME_PEER_GET))
		*data = pdme_cmd->read_write_value = UFS_READ32(UICCMDARG3);

	return TEGRABL_NO_ERROR;
}

void tegrabl_ufs_setup_trtdm_lists(void)
{
	/* Write Transfer Request List Lower and Upper Base Address */
	dma_addr_t tx_address;
	dma_addr_t tm_address;

	/* Map input buffer as per read/write and get physical address */
	tx_address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
			ptx_rx_desc,
			MAX_TRD_NUM * sizeof(struct transfer_request_descriptor),
			TEGRABL_DMA_TO_DEVICE);

	UFS_WRITE32(UTRLBA, (uintptr_t)tx_address);
	UFS_WRITE32(UTRLBAU, 0x0);
	pr_info("UTRLBA is %0x\n", UFS_READ32(UTRLBA));

	tm_address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
			ptask_mgmnt_desc,
			MAX_TMD_NUM * sizeof(struct task_mgmt_request_descriptor),
			TEGRABL_DMA_TO_DEVICE);
	/* Write Task Management Request List Lower and Upper Base Address */
	UFS_WRITE32(UTMRLBA, (uintptr_t)tm_address);
	UFS_WRITE32(UTMRLBAU, 0x0);
	pr_info("UTMRLBA is %0x\n", UFS_READ32(UTMRLBA));

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		ptx_rx_desc,
		MAX_TRD_NUM * sizeof(struct transfer_request_descriptor),
		TEGRABL_DMA_TO_DEVICE);

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		ptask_mgmnt_desc,
		MAX_TRD_NUM * sizeof(struct task_mgmt_request_descriptor),
		TEGRABL_DMA_TO_DEVICE);
}

#if defined(CONFIG_ENABLE_UFS_HS_MODE)
static tegrabl_error_t tegrabl_ufs_unipro_post_linkup(void)
{
	tegrabl_error_t error;
	uint32_t data = 0;

	/* set cport connection status = 1 */
	data = 1;
	error = tegrabl_ufs_set_dme_command(DME_SET, 0,
			t_connectionstate, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	/* MPHY TX sync length changes to MAX */
	data = 0x4f;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_tx_hs_g1_sync_length, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_tx_hs_g2_sync_length, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_tx_hs_g3_sync_length, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	/* Local Timer Value Changes */
	data = 0x1fff;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			dme_fc0protectiontimeoutval, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	data = 0xffff;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			dme_tc0replaytimeoutval, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	data = 0x7fff;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			dme_afc0reqtimeoutval, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	/* PEER TIMER values changes - PA_PWRModeUserData */
	data = 0x1fff;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pwr_mode_user_data0, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}
	data = 0xffff;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pwr_mode_user_data1, &data);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}
	data = 0x7fff;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pwr_mode_user_data2, &data);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("unipro_post_linkup failed\n");
	}

	return error;
}

static  tegrabl_error_t tegrabl_ufs_enable_hs_mode(void)
{
	tegrabl_error_t error;
	uint32_t reg_data = 0;
	uint32_t data = 0;
	uint32_t gear = 0;

	error = tegrabl_ufs_unipro_post_linkup();
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	error =  tegrabl_ufs_set_dme_command(DME_GET, 0,
			vs_debugsaveconfigtime, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	reg_data &= ~set_tref(~0);
	reg_data |= set_tref(vs_debugsaveconfigtime_tref);
	reg_data &= ~set_st_sct(~0);
	reg_data |= set_tref(vs_debugsaveconfigtime_st_sct);

	pr_debug("pvs_debugsaveconfigtime value is 0x%X\n", reg_data);

	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			vs_debugsaveconfigtime, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	error =  tegrabl_ufs_set_dme_command(DME_GET, 0,
			pa_maxrxhsgear, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_debug("pa_maxrxhsgear value is %d\n", reg_data);

	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_rx_gear, &reg_data);

	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_debug("pa_rx_gear is %d\n", reg_data);

	gear = reg_data;

	error =  tegrabl_ufs_set_dme_command(DME_PEER_GET, 0,
			pa_maxrxhsgear, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_debug("pa_maxrxhsgear value is %d\n", reg_data);

	error = tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_tx_gear, &reg_data);

	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_debug("pa_tx_gear is %d\n", reg_data);

	reg_data = 1;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_rx_termination, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	reg_data = 1;
	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_tx_termination, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	reg_data = 0;
	error =  tegrabl_ufs_set_dme_command(DME_GET, 0,
			pa_hs_series, &reg_data);
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_debug("pa_hs_series is %d\n", reg_data);


	/* set rate to HS B */
	reg_data = 2;

	error =  tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_hs_series, &reg_data);

	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_debug("pa_hs_series is %d\n", reg_data);

	data =  ((PWRMODE_FAST_MODE << 4) | PWRMODE_FAST_MODE);
	error = tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_pwr_mode, &data);

	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}

	pr_info("Shifted to HS mode %d Gear %d successfully\n",
		reg_data, gear);
	return error;

uphy_power_down:
#if defined(CONFIG_UFS_UPHY_INIT)
	pr_info("uphy deinit\n");
	tegrabl_ufs_link_uphy_deinit(pufs_context->num_lanes);
#endif
	return error;
}
#endif

/* Clocks, Resets, UFS Link bringup and Host controller setup.*/
tegrabl_error_t tegrabl_ufs_hw_init(uint32_t re_init)
{
	tegrabl_error_t error = 0;
	uint32_t reg_data = 0;
	uint32_t shift_expected_value = 0;
	uint32_t retry = 5;

	error = tegrabl_ufs_clock_init();
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	pr_info("ufs re_init value is %d\n", re_init);

	if (!re_init) {
#if defined(CONFIG_UFS_UPHY_INIT)
		pr_info("uphy init\n");
		error = tegrabl_ufs_link_uphy_setup(pufs_context->num_lanes);
		if (error != TEGRABL_NO_ERROR) {
			return error;
		}
#endif
	}

	reg_data = UFS_READ32(HCE);
	reg_data = SET_FLD(HCE_HCE, 1, reg_data);
	UFS_WRITE32(HCE, reg_data);

	/* Poll for Initialization sequence to be complete */
	shift_expected_value = 1 << SHIFT(HCE_HCE);
	error = tegrabl_ufs_pollfield(HCE, SHIFT_MASK(HCE_HCE),
		shift_expected_value, HCE_SET_TIMEOUT);

	if (error != TEGRABL_NO_ERROR)
		goto uphy_power_down;

	reg_data = UFS_READ32(HCLKDIV);
#if defined(CONFIG_ENABLE_UFS_HS_MODE)
	reg_data = SET_FLD(HCLKDIV_HCLKDIV, 0xcc, reg_data);
#else
	reg_data = SET_FLD(HCLKDIV_HCLKDIV, 0x33, reg_data);
#endif
	UFS_WRITE32(HCLKDIV, reg_data);

	tegrabl_udelay(2000);
	error = tegrabl_ufs_link_mphy_setup();
	if (error != TEGRABL_NO_ERROR)
		goto uphy_power_down;

	pr_info("Link state is %0x\n", NV_READ32(HCS));

	/* Setup buffers for Transfer Request and Task Managment engines*/
	tegrabl_ufs_setup_trtdm_lists();

	error = tegrabl_ufs_start_tmtr_engines();
	if (error != TEGRABL_NO_ERROR)
		goto uphy_power_down;

	error = tegrabl_ufs_set_activate_time();
	if (error != TEGRABL_NO_ERROR)
		goto uphy_power_down;

	/* check if device is ready for descriptors */
	while (retry != 0) {
		error = tegrabl_ufs_chk_if_dev_ready_to_rec_desc();
		if (error != TEGRABL_NO_ERROR) {
			retry--;
			tegrabl_mdelay(300);
		} else {
			break;
		}
	}
	if (error != TEGRABL_NO_ERROR) {
		goto uphy_power_down;
	}
	if (NV_READ32(HCS) != 0x10f) {
		if (!pufs_context->boot_enabled) {
			error = tegrabl_ufs_complete_init();
			if (error != TEGRABL_NO_ERROR)
				goto uphy_power_down;
		}
	}

	pr_info("UFS Hardware init successful\n");
	return TEGRABL_NO_ERROR;

uphy_power_down:
#if defined(CONFIG_UFS_UPHY_INIT)
	pr_info("uphy deinit\n");
	tegrabl_ufs_link_uphy_deinit(pufs_context->num_lanes);
#endif
	return error;
}

tegrabl_error_t
tegrabl_ufs_init(const struct tegrabl_ufs_params *params,
	struct tegrabl_ufs_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t read_capacity_retry;
#if defined(CONFIG_ENABLE_UFS_HS_MODE)
	uint32_t reference_clk = 1;
#endif
	TEGRABL_UNUSED(params);

	ptx_rx_desc = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			1024,
			(MAX_TRD_NUM *
			sizeof(struct transfer_request_descriptor)));

	if (ptx_rx_desc == NULL) {
		pr_error("failed to allocate memory for ufs context\n");
		error = TEGRABL_ERR_NO_MEMORY;
		return error;
	}
	memset(ptx_rx_desc, 0,
		(MAX_TRD_NUM * sizeof(struct transfer_request_descriptor)));

	pcmd_descriptor = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
				128 ,
				(MAX_CMD_DESC_NUM *
				sizeof(struct cmd_descriptor)));

	if (pcmd_descriptor == NULL) {
		pr_error("failed to allocate memory for ufs context\n");
		error = TEGRABL_ERR_NO_MEMORY;
		return error;
	}
	memset(pcmd_descriptor, 0,
		(MAX_CMD_DESC_NUM * sizeof(struct cmd_descriptor)));

	ptask_mgmnt_desc = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
				1024 ,
				MAX_TMD_NUM *
				sizeof(struct task_mgmt_request_descriptor));

	if (ptask_mgmnt_desc == NULL) {
		pr_error("failed to allocate memory for ufs context\n");
		error = TEGRABL_ERR_NO_MEMORY;
		return error;
	}

	memset(ptask_mgmnt_desc, 0,
		(MAX_TMD_NUM * sizeof(struct task_mgmt_request_descriptor)));

	pufs_context = context;

	if (pufs_internal_params.boot_enabled) {
		pufs_context->boot_enabled = 1;
		pufs_context->boot_lun = 0xB0;
	} else {
		pufs_context->boot_enabled = 0;
		pufs_context->boot_lun = pufs_internal_params.boot_lun;
	}

	pufs_context->page_size_log2 = pufs_internal_params.page_size_log2;
	pufs_context->block_size_log2 = pufs_context->page_size_log2;
	pufs_context->active_lanes = 1;
	pufs_context->num_lanes = pufs_internal_params.num_lanes;

	pr_info("UFS init operation\n");
	error = tegrabl_ufs_hw_init(context->init_done);
	/* Clear status & error registers after init */
	tegrabl_clear_err_regs();
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	pufs_context->current_pwm_gear = 1;
	context->init_done = 1;

	error = tegrabl_ufs_change_num_lanes();
	if (error != TEGRABL_NO_ERROR) {
		pr_error("change lanes failed\n");
		return error;
	}

	for (read_capacity_retry = 0; read_capacity_retry < 5;
		read_capacity_retry++) {
		error = tegrabl_ufs_read_capacity(&pufs_context->boot_lun_num_blocks);
		if ((pufs_context->boot_lun_num_blocks <= 1) ||
			(error != TEGRABL_NO_ERROR)) {
			tegrabl_udelay(100000);
			pr_info("read capacity failed.retrying...\n");
		} else
			break;
	}
	pr_info("device capacity is %0x\n", pufs_context->boot_lun_num_blocks);
	context->boot_lun_num_blocks = pufs_context->boot_lun_num_blocks;

#if defined(CONFIG_ENABLE_UFS_HS_MODE)
	error = tegrabl_ufs_get_attribute(&reference_clk,
			QUERY_ATTRB_REF_CLK_FREQ, QUERY_DESC_DEVICE_DESC_IDN);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("reference clk retrieval failed\n");
		return error;
	}
	pr_info("reference_clk is %d\n", reference_clk);
	if (reference_clk == 0) {
		error = tegrabl_ufs_enable_hs_mode();
		if (error != TEGRABL_NO_ERROR) {
			pr_error("Hs  mode shift failed\n");
			return error;
		}
	} else {
		pr_warn("reference clk not programmed..booting in PWM mode\n");
	}
#else
	error = tegrabl_ufs_change_gear(4);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("pwm g4 mode shift failed\n");
		return error;
	}
#endif
	pr_info(" UFS init successful\n");
#ifdef UFS_DEBUG
{

	uint32_t *buffer = NULL;
	uint32_t *outbuffer = NULL;
	buffer = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			1024, 4096);
	outbuffer = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			1024, 4096);

	buffer[0] = 0xdfadbead;
	outbuffer[1] = 0xaaaaaaaa;

	pr_info("test read %x\n", *buffer);

	NV_WRITE32(0x2450020, 0xffff);
	tegrabl_udelay(2000);

	error = tegrabl_ufs_write(0, 0, 1, buffer);
	error = tegrabl_ufs_read(0, 0, 1, outbuffer);
	error = tegrabl_ufs_write(0, 0, 1, buffer);
	error = tegrabl_ufs_read(0, 0, 1, outbuffer);
	pr_info("read out put %x\n", outbuffer[0]);
	error = tegrabl_ufs_write(0, 0, 1, buffer);
	error = tegrabl_ufs_read(0, 0, 1, outbuffer);
	pr_info("read out put %x\n", outbuffer[0]);

}
#endif
	return TEGRABL_NO_ERROR;
}


tegrabl_error_t tegrabl_ufs_deinit(void)
{
	if (ptx_rx_desc)
		tegrabl_dealloc(TEGRABL_HEAP_DMA, ptx_rx_desc);
	if (pcmd_descriptor)
		tegrabl_dealloc(TEGRABL_HEAP_DMA, pcmd_descriptor);
	if (ptask_mgmnt_desc)
		tegrabl_dealloc(TEGRABL_HEAP_DMA, ptask_mgmnt_desc);

	if (pufs_context) {
#if defined(CONFIG_UFS_UPHY_INIT)
		pr_info("uphy deinit\n");
		tegrabl_ufs_link_uphy_deinit(pufs_context->num_lanes);
#endif
	}

	return TEGRABL_NO_ERROR;
}

/** Start Engines for processing Task Management
 * and TRD List. UTMRLRDY and UTRLRDY should be set
 */
tegrabl_error_t tegrabl_ufs_start_tmtr_engines(void)
{
	uint32_t shift_expected_value;
	uint32_t  reg_data;
	tegrabl_error_t error;

	reg_data = UFS_READ32(UTMRLRSR);
	reg_data = SET_FLD(UTMRLRSR_UTMRLRSR, 1, reg_data);
	UFS_WRITE32(UTMRLRSR, reg_data);

	reg_data = UFS_READ32(UTRLRSR);
	reg_data = SET_FLD(UTRLRSR_UTRLRSR, 1, reg_data);
	UFS_WRITE32(UTRLRSR, reg_data);
	reg_data = UFS_READ32(UTRLRSR);

	shift_expected_value = 1 << SHIFT(HCS_UTMRLRDY);
	error = tegrabl_ufs_pollfield(HCS,
		SHIFT_MASK(HCS_UTMRLRDY),
		shift_expected_value, UTMRLRDY_SET_TIMEOUT);

	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	shift_expected_value = 1 << SHIFT(HCS_UTRLRDY);
	error = tegrabl_ufs_pollfield(HCS, SHIFT_MASK(HCS_UTRLRDY),
			shift_expected_value, UTRLRDY_SET_TIMEOUT);
	return error;
}


/** Get a TRD slot from TRD list if available else
 *  return error.
 */
static
tegrabl_error_t tegrabl_ufs_get_tx_rx_descriptor(uint32_t *ptrd_index)
{
	uint32_t trd_index;
	uint32_t reg_data;

	if (pufs_context->tx_req_des_in_use < MAX_TRD_NUM) {
		trd_index = NEXT_TRD_IDX(pufs_context->last_trd_index);
		reg_data = UFS_READ32(UTRLDBR);
		if (reg_data & (1 << trd_index)) {
			pr_error("reg data is %0x\n", reg_data);
			pr_error("trd index is %0x\n", trd_index);
			return TEGRABL_ERROR(TEGRABL_ERR_FATAL, 0U);
		}

		*ptrd_index = trd_index;
		pufs_context->last_trd_index = trd_index;
		pufs_context->tx_req_des_in_use++;
		return TEGRABL_NO_ERROR;
	} else
		return TEGRABL_ERROR(TEGRABL_ERR_RESOURCE_MAX, 1U);
}

/** Creates TRD from given GenericCmdDescriptor (if slot available in TRD List)
 *  and returns success else returns UFSResourceMax error.
 */

tegrabl_error_t
tegrabl_ufs_create_trd(uint32_t trd_index,
			uint32_t cmd_desc_index,
			uint32_t data_dir, uint32_t prdt_length)
{
	struct transfer_request_descriptor *pptx_rx_desc = NULL;
	dma_addr_t pcmd_address;
	dma_addr_t data_address;
	dma_addr_t tx_rx_address;
	dma_addr_t tx_resp_address;
	dma_addr_t tx_address;
	struct cmd_descriptor *plcmd_descriptor = NULL;

	TEGRABL_UNUSED(tx_address);
	TEGRABL_UNUSED(tx_rx_address);
	TEGRABL_UNUSED(tx_resp_address);
	TEGRABL_UNUSED(data_address);

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	/* Get TRD */
	pr_debug("cmd_desc_index in tegrabl_ufs_create_trd is %d\n",
				cmd_desc_index);
	pptx_rx_desc = &ptx_rx_desc[trd_index];
	memset((void *)pptx_rx_desc, 0,
		sizeof(struct transfer_request_descriptor));

	/* Command Type and Data Direction
	* CmdType is always UFS Storage
	*/
	pptx_rx_desc->dw0.control_type = UFS_TRD_DW0_0_CT_UFS;
	pptx_rx_desc->dw0.dd = data_dir;

	/* Init overall command status to 0xF */
	pptx_rx_desc->dw2.ocs = OCS_INVALID;

	/* Fill in Resp UPIU length and offset in Dwords*/
	pptx_rx_desc->dw6.rul = CMD_DESC_RESP_LENGTH/4;
	pptx_rx_desc->dw6.ruo = CMD_DESC_REQ_LENGTH/4;

	if (data_dir != DATA_DIR_NIL) {
		if (prdt_length > MAX_PRDT_LENGTH) {
			pr_error("ufs %d prdt more than max.\n", prdt_length);
			return TEGRABL_ERROR(TEGRABL_ERR_FATAL, 7U);
		}
		pptx_rx_desc->dw7.prdtl = prdt_length;
		pptx_rx_desc->dw7.prdto =
			(CMD_DESC_RESP_LENGTH+CMD_DESC_REQ_LENGTH)/4;
	}

	/* Fill in Cmd Desc Lower and Upper Address. */
	pcmd_address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
			&(plcmd_descriptor->vucd_generic_req_upiu),
			sizeof(union ucd_generic_req_upiu),
			TEGRABL_DMA_TO_DEVICE);

	pptx_rx_desc->dw4.ctba = ((uintptr_t)pcmd_address >> 7);

	tx_resp_address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
			&(plcmd_descriptor->vucd_generic_resp_upiu),
			sizeof(union ucd_generic_req_upiu),
			TEGRABL_DMA_FROM_DEVICE);

	data_address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
				plcmd_descriptor->vprdt,
				MAX_PRDT_LENGTH * sizeof(struct prdt),
				TEGRABL_DMA_TO_DEVICE);

#ifdef UFS_DEBUG
	pr_debug("tegrabl_ufs_create_trd   -----dump-------\n");
	{
		uint32_t *ptr = (uint32_t *)pptx_rx_desc;
		uint32_t count = 0;
		pr_debug("pptx_rx_desc base address is %p\n", pptx_rx_desc);
		for (count = 0;
			count < sizeof(struct transfer_request_descriptor);
			count += 4) {
			pr_debug("pptx_rx_desc[%d] = %0x\n", count/4, *ptr);
			ptr++;
		}
	}
#endif

	tx_address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
			pptx_rx_desc,
			MAX_TRD_NUM *
			sizeof(struct transfer_request_descriptor),
			TEGRABL_DMA_BIDIRECTIONAL);
	return TEGRABL_NO_ERROR;
}

static
tegrabl_error_t tegrabl_ufs_queue_trd(uint32_t trd_index, uint32_t trd_timeout)
{
	uint32_t reg_data;

	reg_data = UFS_READ32(UTRLDBR);
	if (reg_data & (1 << trd_index)) {
		tegrabl_ufs_free_trd_cmd_desc();
		return TEGRABL_ERROR(TEGRABL_ERR_FATAL, 1U);
	}

	/* All slots except the required should be set to zero */
	reg_data = 1 << trd_index;

	UFS_WRITE32(UTRLDBR, reg_data);

	memset((void *)&pufs_context->trd_info[trd_index],
		0, sizeof(struct trdinfo));
	pufs_context->trd_info[trd_index].trd_starttime =
			tegrabl_get_timestamp_us();
	pufs_context->trd_info[trd_index].trd_timeout =
		trd_timeout;
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
tegrabl_ufs_wait_trd_request_complete(uint32_t trd_index, uint32_t timeout)
{
	uint32_t reg_data;
	tegrabl_error_t error;
	uint32_t shift_expected_value;
	uint32_t mask;
	struct transfer_request_descriptor *pptx_rx_desc;

	pptx_rx_desc = &ptx_rx_desc[trd_index];
	mask = 1 << trd_index;
	shift_expected_value = 0;
	error = tegrabl_ufs_pollfield(UTRLDBR,
			mask, shift_expected_value, timeout);
	if (error != TEGRABL_NO_ERROR) {
		tegrabl_dump_ufs_regs();
		return error;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		pptx_rx_desc,
		MAX_TRD_NUM * sizeof(struct transfer_request_descriptor),
		TEGRABL_DMA_BIDIRECTIONAL);

	if (pptx_rx_desc->dw2.ocs != OCS_SUCCESS) {
		return TEGRABL_ERROR(TEGRABL_ERR_FATAL, 2U);
	}

	reg_data = UFS_READ32(IS);

	if (READ_FLD(IS_SBFES, reg_data) || READ_FLD(IS_HCFES, reg_data) ||
		READ_FLD(IS_UTPES, reg_data) || READ_FLD(IS_DFES, reg_data)) {
		return TEGRABL_ERROR(TEGRABL_ERR_FATAL, 3U);
	}

	return TEGRABL_NO_ERROR;
}

void tegrabl_ufs_free_trd_cmd_desc(void)
{
	pufs_context->tx_req_des_in_use--;
	pufs_context->cmd_desc_in_use--;
}

tegrabl_error_t tegrabl_ufs_chk_if_dev_ready_to_rec_desc(void)
{

	struct nop_upiu *pnop_upiu;
	struct cmd_descriptor *plcmd_descriptor;
	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));

	/* Create NOP_OUT UPIU. Only Transaction Code needed */
	pnop_upiu =
		(struct nop_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;
	pnop_upiu->basic_header.trans_code = UPIU_NOP_OUT_TRANSACTION;

	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_queue_trd(trd_index, NOP_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
			NOP_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);

	pnop_upiu = (struct nop_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;

	if (pnop_upiu->basic_header.trans_code == UPIU_NOP_IN_TRANSACTION)
		pr_info("Nop successful\n");
	else {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

/** Get Descriptor to identify device characteristics
 */
tegrabl_error_t
tegrabl_ufs_get_descriptor(uint8_t *pufsdesc,
				uint32_t descidn,
				uint32_t desc_index)
{
	struct cmd_descriptor *plcmd_descriptor;
	struct query_req_resp_upiu *pquery_req_upiu;
	struct query_req_resp_upiu *pquery_resp_upiu;
	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}
	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pquery_req_upiu = (struct query_req_resp_upiu *)
		&plcmd_descriptor->vucd_generic_req_upiu;
	pquery_req_upiu->basic_header.trans_code =
		UPIU_QUERY_REQUEST_TRANSACTION;
	pquery_req_upiu->basic_header.query_tm_function =
		UPIU_QUERY_FUNC_STD_READ;
	pquery_req_upiu->basic_header.task_tag = 0x36;
	pquery_req_upiu->vtsf.op_code = TSF_OPCODE_READ_DESC;

	/* IDN i.e Device OR Configuration OR Unit descriptor .. */
	pquery_req_upiu->vtsf.vdesc_fields.desc_idn = descidn;
	/* Index */
	pquery_req_upiu->vtsf.vdesc_fields.index = desc_index;
	/* TODO Find out device descriptor size.
	* It is ok to simply specify max desc size
	* of 255 bytes as device will return actual descriptor size if lesser.
	*/

	pquery_req_upiu->vtsf.vdesc_fields.length_bige = BYTE_SWAP16(255);

	/* TODO Task Tags */
	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index,
					DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	/* TODO Timeout for READ DESCRIPTOR */
	error = tegrabl_ufs_queue_trd(trd_index, QUERY_REQ_DESC_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		return error;

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				QUERY_REQ_DESC_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		return error;

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);
	pquery_resp_upiu = (struct query_req_resp_upiu *)
		&plcmd_descriptor->vucd_generic_resp_upiu;

	memcpy((void *)pufsdesc,
		(void *)&(pquery_resp_upiu->data_segment[0]),
		BYTE_SWAP16(pquery_resp_upiu->basic_header.data_seg_len_bige));

	if (pquery_resp_upiu->basic_header.trans_code !=
		UPIU_QUERY_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

tegrabl_error_t
tegrabl_ufs_get_attribute(uint32_t *pufsattrb,
				uint32_t attrbidn,
				uint32_t attrbindex)
{
	struct cmd_descriptor *plcmd_descriptor;
	struct query_req_resp_upiu *pquery_req_upiu;
	struct query_req_resp_upiu *pquery_resp_upiu;

	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR)
		return error;

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR)
		return error;

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pquery_req_upiu =
		(struct query_req_resp_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;
	pquery_req_upiu->basic_header.trans_code = UPIU_QUERY_REQUEST_TRANSACTION;
	pquery_req_upiu->basic_header.query_tm_function = UPIU_QUERY_FUNC_STD_READ;
	pquery_req_upiu->vtsf.op_code = TSF_OPCODE_READ_ATTRB;

	/* IDN i.e Device OR Configuration OR Unit descriptor .. */
	pquery_req_upiu->vtsf.vattrb_fields.attrb_idn = attrbidn;
	/* Index */
	pquery_req_upiu->vtsf.vattrb_fields.index = attrbindex;

	/* TODO Task Tags */
	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, 0, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	/* TODO Timeout for READ ATTRB */
	error = tegrabl_ufs_queue_trd(trd_index, QUERY_REQ_ATTRB_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		return error;

	error =
		tegrabl_ufs_wait_trd_request_complete(trd_index,
			QUERY_REQ_ATTRB_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		return error;

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);
	pquery_resp_upiu =
		(struct query_req_resp_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;
	*pufsattrb = BYTE_SWAP32(pquery_resp_upiu->vtsf.vattrb_fields.value_bige);

	if (pquery_resp_upiu->basic_header.trans_code !=
		UPIU_QUERY_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

static tegrabl_error_t
tegrabl_ufs_get_flag(uint32_t *pufsflag, uint32_t flagidn, uint32_t flag_index)
{
	struct cmd_descriptor *plcmd_descriptor;
	struct query_req_resp_upiu *pquery_req_upiu;
	struct query_req_resp_upiu *pquery_resp_upiu;

	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR)
		return error;

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR)
		return error;

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pquery_req_upiu =
		(struct query_req_resp_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;
	pquery_req_upiu->basic_header.trans_code = UPIU_QUERY_REQUEST_TRANSACTION;
	pquery_req_upiu->basic_header.query_tm_function = UPIU_QUERY_FUNC_STD_READ;
	pquery_req_upiu->vtsf.op_code = TSF_OPCODE_READ_FLAG;

	/* IDN i.e Device OR Configuration OR Unit descriptor .. */
	pquery_req_upiu->vtsf.vflag_fields.flag_idn = flagidn;
	/* Index */
	pquery_req_upiu->vtsf.vflag_fields.index = flag_index;

	/* TODO Task Tags */
	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}


	error = tegrabl_ufs_queue_trd(trd_index, QUERY_REQ_FLAG_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				QUERY_REQ_FLAG_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);
	pquery_resp_upiu =
		(struct query_req_resp_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;
	*pufsflag = pquery_resp_upiu->vtsf.vflag_fields.flag_value;

	if (pquery_resp_upiu->basic_header.trans_code !=
		UPIU_QUERY_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

static tegrabl_error_t
tegrabl_ufs_set_flag(uint32_t flagidn, uint32_t flag_index)
{
	struct cmd_descriptor *plcmd_descriptor;
	struct query_req_resp_upiu *pquery_req_upiu;
	struct query_req_resp_upiu *pquery_resp_upiu;

	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	uint32_t flag_readback = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pquery_req_upiu =
		(struct query_req_resp_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;
	pquery_req_upiu->basic_header.trans_code = UPIU_QUERY_REQUEST_TRANSACTION;
	/* TODO: Is SET FLAG a Read or Write Query function */
	pquery_req_upiu->basic_header.query_tm_function = UPIU_QUERY_FUNC_STD_WRITE;
	pquery_req_upiu->vtsf.op_code = TSF_OPCODE_SET_FLAG;

	/* IDN i.e Device OR Configuration OR Unit descriptor .. */
	pquery_req_upiu->vtsf.vflag_fields.flag_idn = flagidn;
	/* Index */
	pquery_req_upiu->vtsf.vflag_fields.index = flag_index;

	/* TODO Task Tags */
	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}


	/* TODO Timeout for READ FLAG */
	error = tegrabl_ufs_queue_trd(trd_index, QUERY_REQ_FLAG_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		return error;

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				QUERY_REQ_FLAG_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		return error;

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);
	pquery_resp_upiu =
		(struct query_req_resp_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;
	/* OSF6 LSB (Big Endian) TODO contains the returned flag*/
	flag_readback = pquery_resp_upiu->vtsf.vflag_fields.flag_value;
	if (flag_readback != 1) {
		pr_debug("flag_readback is %d\n", flag_readback);
		error = TEGRABL_ERROR(TEGRABL_ERR_FATAL, 4U);
	}

	if (pquery_resp_upiu->basic_header.trans_code !=
		UPIU_QUERY_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

tegrabl_error_t
tegrabl_ufs_common_upiu_write(uint8_t *pdesc_req_data, uint32_t data_size)
{
	struct cmd_descriptor *plcmd_descriptor;

	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	uint32_t i = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (data_size > CMD_DESC_REQ_LENGTH) {
		return tegrabl_error_value(TEGRABL_ERR_UFS, 0,
				TEGRABL_ERR_INVALID_CONFIG);
	}

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	for (i = 0; i < data_size; i++) {
		plcmd_descriptor->cmd_desc_req[i] = pdesc_req_data[i];
	}

	/* TODO Task Tags */
	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_queue_trd(trd_index, QUERY_REQ_FLAG_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				QUERY_REQ_FLAG_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

tegrabl_error_t tegrabl_ufs_complete_init(void)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t flag_device_init = 0;
	time_t fdevice_clear_start_time;
	time_t fdevice_clear_end_time;
	time_t total_time;
	uint32_t retry = 5;
	pr_info("performing fdevice init clear\n");
	while (retry != 0) {
		e = tegrabl_ufs_set_flag(QUERY_FLAG_DEVICE_INIT_IDN, 0);
		if (e != TEGRABL_NO_ERROR) {
			if (retry == 0) {
				return e;
			} else {
				retry--;
				tegrabl_mdelay(5);
			}
		} else {
			break;
		}
	}
	retry = 5;
	fdevice_clear_start_time = tegrabl_get_timestamp_us();
	do {
		e = tegrabl_ufs_get_flag(&flag_device_init,
			QUERY_FLAG_DEVICE_INIT_IDN, 0);
		if (e != TEGRABL_NO_ERROR) {
			if (retry == 0) {
				pr_error("timeout of fdevinit\n");
				return e;
			} else {
				retry--;
				tegrabl_mdelay(5);
			}
		}
		if (flag_device_init == 1) {
			fdevice_clear_end_time = tegrabl_get_timestamp_us();
			total_time = fdevice_clear_end_time - fdevice_clear_start_time;
			if (total_time > FDEV_INIT_CLR_MAX_TIME) {
				pr_error("fdevice init not cleared\n");
				e = TEGRABL_ERROR(TEGRABL_ERR_FATAL, 0U);
				return e;
			}
		}
	} while (flag_device_init != 0);
	pr_debug("tegrabl_ufs_complete_init\n");
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_ufs_read_capacity(uint32_t *size)
{
	struct cmd_descriptor *plcmd_descriptor;
	struct command_upiu *pcommand_upiu;
	struct response_upiu *presponse_upiu;
	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t *buffer = NULL;
	dma_addr_t address;

	buffer = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			ALIGN_1K, BLOCK_SIZE);
	if (buffer == NULL) {
		pr_error("failed to allocate memory for UFS context\n");
		error = TEGRABL_ERR_NO_MEMORY;
		goto fail;
	}
	memset(buffer, 0, BLOCK_SIZE);

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR)
		return error;

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
			buffer, 4096,
			TEGRABL_DMA_FROM_DEVICE);

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pcommand_upiu =
		(struct command_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;

	presponse_upiu =
		(struct response_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;
	pcommand_upiu->basic_header.trans_code = UPIU_COMMAND_TRANSACTION;

	pcommand_upiu->basic_header.flags = 1 << UFS_UPIU_FLAGS_R_SHIFT;
	pcommand_upiu->basic_header.task_tag = 0x1;
	pcommand_upiu->basic_header.cmd_set_type = UPIU_COMMAND_SET_SCSI;
	pcommand_upiu->expected_data_tx_len_bige =
			BYTE_SWAP32(32 * (1 << pufs_context->page_size_log2));

	/* Construct CDB for READ CAPACITY(10) command */
	pcommand_upiu->cdb[0] = SCSI_READ_CAPACITY10_OPCODE;
	/* Byte 1-3 are used for LBA in Big endian format */
	pcommand_upiu->cdb[5] = 0;
	pcommand_upiu->cdb[4] = 0;
	pcommand_upiu->cdb[3] = 0;
	pcommand_upiu->cdb[2] = 0;

	/* Fill in control = 0x00 */
	pcommand_upiu->cdb[9] = 0;

	plcmd_descriptor->vprdt[0].dw0 =
		((uintptr_t)(address & 0xffffffff)) & ~(0x3);
	plcmd_descriptor->vprdt[0].dw1 = 0;

	plcmd_descriptor->vprdt[0].dw3 =
		(32 * (1 << pufs_context->page_size_log2)) - 1;

	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, DATA_DIR_D2H, 1);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_ufs_queue_trd(trd_index, SCSI_REQ_READ_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				 100 * SCSI_REQ_READ_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);

	if (presponse_upiu->basic_header.trans_code !=
		UPIU_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
		goto fail;
	}

	if (presponse_upiu->basic_header.response != TARGET_SUCCESS) {
		error = TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 0U);
		goto fail;
	}

	if (presponse_upiu->basic_header.status != SCSI_STATUS_GOOD) {
		error = TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 1U);
		goto fail;
	}

fail:
	if (buffer != NULL) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
			buffer, BLOCK_SIZE,
				TEGRABL_DMA_FROM_DEVICE);
		*size = (BYTE_SWAP32(buffer[0]) + 1);
		tegrabl_dealloc(TEGRABL_HEAP_DMA, buffer);
	}
	tegrabl_ufs_free_trd_cmd_desc();
	if (error != TEGRABL_NO_ERROR) {
		pr_error("read capacity failed\n");
		*size = 0;
	}
	return error;
}


tegrabl_error_t tegrabl_ufs_erase(uint32_t start_block, uint32_t blocks)
{
	struct cmd_descriptor *plcmd_descriptor;
	struct command_upiu *pcommand_upiu;
	struct response_upiu *presponse_upiu;
	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	uint32_t *buffer = NULL;
	uint8_t provision_type;
	dma_addr_t address;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_provisioning_type(pufs_context->boot_lun,
							&provision_type);
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("can't get provisioning type of LUN.\n");
		return error;
	}

	if (provision_type == 0x0 && (start_block != 0 ||
				blocks != pufs_context->boot_lun_num_blocks)) {
		pr_info("can't erase partial storage under full provision.\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pcommand_upiu =
		(struct command_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;

	presponse_upiu =
		(struct response_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;
	pcommand_upiu->basic_header.trans_code = UPIU_COMMAND_TRANSACTION;

	pcommand_upiu->basic_header.flags = 1 << UFS_UPIU_FLAGS_W_SHIFT;
	pcommand_upiu->basic_header.task_tag = 0x1;
	pcommand_upiu->basic_header.cmd_set_type = UPIU_COMMAND_SET_SCSI;
	pcommand_upiu->basic_header.lun = pufs_context->boot_lun;

	if (provision_type == 0x0) {
		pcommand_upiu->expected_data_tx_len_bige = 0;
		/* Construct CDB for FORMAT UNIT(10) command */
		pcommand_upiu->cdb[0] = SCSI_FORMAT_UNIT_OPCODE;
		error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index,
						DATA_DIR_NIL, 1);
	} else {
		pcommand_upiu->expected_data_tx_len_bige =
			BYTE_SWAP32(1 << pufs_context->page_size_log2);
		/* Construct CDB for UNMAP command */
		pcommand_upiu->cdb[0] = SCSI_UNMAP_OPCODE;
		pcommand_upiu->cdb[7] = 0x00;
		pcommand_upiu->cdb[8] = 0x18;
		buffer = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
				ALIGN_1K, BLOCK_SIZE);
		if (buffer == NULL) {
			pr_error("failed to allocate memory for UFS context\n");
			error = TEGRABL_ERR_NO_MEMORY;
			goto fail;
		}
		memset(buffer, 0, BLOCK_SIZE);
		/* One Block Descriptor */
		*((uint16_t *)buffer + 0) = BYTE_SWAP16(0x16);
		*((uint16_t *)buffer + 1) = BYTE_SWAP16(0x10);
		buffer[3] = BYTE_SWAP32(start_block);
		buffer[4] = BYTE_SWAP32(blocks);
		address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
						buffer, 4096,
						TEGRABL_DMA_TO_DEVICE);
		plcmd_descriptor->vprdt[0].dw0 =
			((uintptr_t)(address & 0xffffffff)) & ~(0x3);
		plcmd_descriptor->vprdt[0].dw1 = 0;

		plcmd_descriptor->vprdt[0].dw3 =
			(1 << pufs_context->page_size_log2) - 1;
		error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index,
						DATA_DIR_H2D, 1);
	}

	if (error != TEGRABL_NO_ERROR)
		goto fail;

	error = tegrabl_ufs_queue_trd(trd_index, SCSI_REQ_READ_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				SCSI_REQ_ERASE_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);

	if (presponse_upiu->basic_header.trans_code !=
		UPIU_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
		goto fail;
	}

	if (presponse_upiu->basic_header.response != TARGET_SUCCESS) {
		error = TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 2U);
		goto fail;
	}

	if (presponse_upiu->basic_header.status != SCSI_STATUS_GOOD) {
		error = TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 3U);
		goto fail;
	}

fail:
	if (error == TEGRABL_NO_ERROR) {
		pr_info("device erase  successfull\n");
	} else {
		pr_info("device erase failed error=0x%08x\n", error);
	}

	tegrabl_ufs_free_trd_cmd_desc();
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		buffer, 4096,
		TEGRABL_DMA_TO_DEVICE);

	if (buffer != NULL) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, buffer);
		buffer = NULL;
	}

	return error;
}

tegrabl_error_t
tegrabl_ufs_rw_common(const uint32_t block, const uint32_t page,
			const uint32_t length, uint32_t *pbuffer, uint32_t opcode)
{
	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	struct cmd_descriptor *plcmd_descriptor;
	struct command_upiu *pcommand_upiu;
	struct response_upiu *presponse_upiu;
	uint32_t pending_length = length;
	uint32_t prdt_length;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
#if defined(UFS_REQUEST_SENSE)
	uint32_t lun_ready = 0;
#endif
	uint32_t direction = (opcode == SCSI_WRITE10_OPCODE) ? 1 : 0;

	TEGRABL_UNUSED(page);

	pr_debug("UFS R/W block %d len %d\n", block, length);

	if (length > MAX_PRDT_LENGTH*MAX_BLOCKS) {
		pr_error("# of blocks %u > %u\n", length,
			 (unsigned int)MAX_PRDT_LENGTH*MAX_BLOCKS);
		return error;
	}

#if defined(UFS_REQUEST_SENSE)
	do {
		error = tegrabl_ufs_test_unit_ready(pufs_context->boot_lun);
		if (error == TEGRABL_NO_ERROR) {
			lun_ready = 1;
		} else if (error == TEGRABL_ERR_BUSY) {
			tegrabl_udelay(10);
		} else if (error == TEGRABL_ERR_CHECK_CONDITION) {
			error = tegrabl_ufs_request_sense(pufs_context->boot_lun);
			if (error == TEGRABL_ERR_BUSY || error == TEGRABL_NO_ERROR)
				continue;
			else {
				pr_info("sense error\n");
				return error;
			}
		} else {
			pr_info("ufs test unit ready error\n");
			return error;
		}
	} while (!lun_ready);
#endif

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("ufs: Tx/Rx Descriptor not available.\n");
		return error;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("ufs: Command Descriptor not available\n");
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pcommand_upiu =
		(struct command_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;

	pcommand_upiu->basic_header.trans_code = UPIU_COMMAND_TRANSACTION;

	pcommand_upiu->basic_header.flags = 1 << (direction ?
				UFS_UPIU_FLAGS_W_SHIFT : UFS_UPIU_FLAGS_R_SHIFT);
	pcommand_upiu->basic_header.lun = pufs_context->boot_lun;
	pcommand_upiu->basic_header.cmd_set_type = UPIU_COMMAND_SET_SCSI;
	pcommand_upiu->basic_header.task_tag = (direction ? 0x14 : 0x15);
	pcommand_upiu->expected_data_tx_len_bige =
		BYTE_SWAP32(length * (1 << pufs_context->page_size_log2));

	/* Construct CDB for READ(10) command */
	pcommand_upiu->cdb[0] = opcode;
	/* Byte 1-3 are used for LBA in Big endian format */
	pcommand_upiu->cdb[5] = (block & 0xFF);
	pcommand_upiu->cdb[4] = (block >> 8) & 0xFF;
	pcommand_upiu->cdb[3] = (block >> 16) & 0xFF;
	pcommand_upiu->cdb[2] = (block >> 24) & 0xFF;

	/* Fill in transfer length in num of blocks*/
	pcommand_upiu->cdb[7] = (length >> 8) & 0xFF;
	pcommand_upiu->cdb[8] = length & 0xff;
	/* Fill in control = 0x00 */
	pcommand_upiu->cdb[9] = 0;

#ifdef UFS_DEBUG
	pr_info("Dumping UPIU commad structure for read\n");
	{
		uint32_t count = 0;
		uint32_t *ptr = (uint32_t *)pcommand_upiu;
		pr_info("-----> Base address %p\n", pcommand_upiu);
		for (count = 0; count <= sizeof(struct command_upiu); (count += 4)) {
			pr_info("pcommand_upiu[%d] is %0x\n", count/4, *ptr);
			ptr++;
		}
	}
#endif

	for (prdt_length = 0;
		(pending_length > 0) && (prdt_length < MAX_PRDT_LENGTH);
		prdt_length++) {
		uint32_t *lbuffer;
		uint16_t num_blocks;
		dma_addr_t address = 0;

		if (pending_length / MAX_BLOCKS)
			num_blocks = MAX_BLOCKS;
		else
			num_blocks = pending_length;

		pending_length -= num_blocks;

		lbuffer = pbuffer + ((prdt_length * (MAX_BLOCKS * BLOCK_SIZE) / 4));
		pr_debug("ufs prdt %d: buf %p, len %d\n",
			prdt_length, lbuffer,
			num_blocks * (1 << pufs_context->page_size_log2));

		address = tegrabl_dma_map_buffer(TEGRABL_MODULE_UFS, 0,
					lbuffer, num_blocks * 4096,
					(direction ? TEGRABL_DMA_TO_DEVICE :
					 TEGRABL_DMA_FROM_DEVICE));

		plcmd_descriptor->vprdt[prdt_length].dw0 =
			((uintptr_t)(address & 0xffffffff)) & ~(0x3);
		plcmd_descriptor->vprdt[prdt_length].dw1 =
			((uintptr_t)(address >> 32) & 0xffffffff);
		plcmd_descriptor->vprdt[prdt_length].dw3 =
			(num_blocks * (1 << pufs_context->page_size_log2)) - 1;
	}

	pr_debug("R/W cmd: prdt cnt %d\n", prdt_length);

	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index,
				(direction ? DATA_DIR_H2D : DATA_DIR_D2H),
				prdt_length);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_queue_trd(trd_index,
					prdt_length * SCSI_REQ_READ_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
				prdt_length * SCSI_REQ_READ_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);

	presponse_upiu =
		(struct response_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;

	if (presponse_upiu->basic_header.trans_code !=
		UPIU_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		return TEGRABL_ERR_COMMAND_FAILED;
	}

	if (presponse_upiu->basic_header.response != TARGET_SUCCESS) {
		pr_error("ufs command response failure\n");
		return TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 5U);
	}

	if (presponse_upiu->basic_header.status != SCSI_STATUS_GOOD) {
		pr_error("ufs command response not good\n");
		return TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 6U);
	}

	tegrabl_ufs_free_trd_cmd_desc();

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		pbuffer, length * 4096,
		(direction ? TEGRABL_DMA_TO_DEVICE : TEGRABL_DMA_FROM_DEVICE));

	pr_debug("R/W successfull\n");

	return error;
}

tegrabl_error_t
tegrabl_ufs_read(const uint32_t block, const uint32_t page,
			const uint32_t length, uint32_t *pbuffer)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(page);

	error = tegrabl_ufs_rw_common(block, page, length, pbuffer,
					SCSI_READ10_OPCODE);

	return error;
}

tegrabl_error_t
tegrabl_ufs_write(const uint32_t block,
			const uint32_t page,
			const uint32_t length,
			uint32_t *pbuffer)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(page);

	error = tegrabl_ufs_rw_common(block, page, length, pbuffer,
					SCSI_WRITE10_OPCODE);

	return error;
}

void
tegrabl_ufs_get_params(
	const uint32_t param_index,
	struct tegrabl_ufs_params **params)
{
	pufs_params.active_lanes =
		NV_DRF_VAL(UFS, FUSE_PARAMS, ACTIVE_LANES, param_index) + 1;
	pufs_params.pwm_gear =
		NV_DRF_VAL(UFS, FUSE_PARAMS, SPEED, param_index) + 1;
	pufs_internal_params.page_size_log2 = UFS_PAGE_SIZE_LOG2 +
			NV_DRF_VAL(UFS, FUSE_PARAMS, PAGE_SIZE, param_index);
	pufs_internal_params.boot_lun =
		NV_DRF_VAL(UFS, FUSE_PARAMS, LUN, param_index);
	pufs_internal_params.boot_enabled =
		NV_DRF_VAL(UFS, FUSE_PARAMS, BOOT_ENABLE, param_index);
	pufs_internal_params.num_lanes =
		NV_DRF_VAL(UFS, FUSE_PARAMS, NUM_LANES, param_index) + 1;
	*params = &pufs_params;
}


bool
tegrabl_ufs_validate_params(const struct tegrabl_ufs_params *params)
{
	if (params->active_lanes > 2 || params->active_lanes < 1) {
		return false;
	}
	if (params->pwm_gear > 4 || params->pwm_gear < 1) {
		return false;
	}
	return true;
}

void
tegrabl_ufs_get_block_sizes(
const struct tegrabl_ufs_params *params,
	uint32_t *blocksizelog2,
	uint32_t *pagesizelog2)
{
	(void)params;
	*blocksizelog2 = pufs_context->block_size_log2;
	*pagesizelog2 = pufs_context->page_size_log2;
}

tegrabl_error_t tegrabl_ufs_change_num_lanes(void)
{
	uint32_t data = 0;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	e = tegrabl_ufs_set_dme_command(DME_GET, 0,
		pa_connected_tx_data_lanes, &data);

	if (e != TEGRABL_NO_ERROR) {
		return e;
	}
	pr_info("pa_connected_tx_data_lanes is %d\n", data);

	e = tegrabl_ufs_set_dme_command(DME_SET, 0,
			pa_active_tx_data_lanes, &data);
	if (e != TEGRABL_NO_ERROR) {
		return e;
	}

	e = tegrabl_ufs_set_dme_command(DME_GET, 0,
			pa_connected_rx_data_lanes, &data);

	if (e != TEGRABL_NO_ERROR) {
		return e;
	}

	pr_info("pa_connected_rx_data_lanes, is %d\n", data);

	e = tegrabl_ufs_set_dme_command(DME_SET, 0,
		pa_active_rx_data_lanes, &data);

	if (e != TEGRABL_NO_ERROR) {
		return e;
	}

	data = ((PWRMODE_SLOWAUTO_MODE << 4) | PWRMODE_SLOWAUTO_MODE);
	e = tegrabl_ufs_set_dme_command(DME_SET, 0, pa_pwr_mode, &data);

	if (e == TEGRABL_NO_ERROR) {
		pr_info("active lanes set successfully\n");
	}
	return e;
}

tegrabl_error_t tegrabl_ufs_change_gear(uint32_t gear)
{
	uint32_t data;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	data = gear;
	e = tegrabl_ufs_set_dme_command(DME_SET, 0, pa_tx_gear, &data);
	if (e != TEGRABL_NO_ERROR) {
		return e;
	}

	data = gear;
	e = tegrabl_ufs_set_dme_command(DME_SET, 0, pa_rx_gear, &data);
	if (e != TEGRABL_NO_ERROR) {
		return e;
	}
#if !defined(CONFIG_ENABLE_UFS_HS_MODE)
	tegrabl_ufs_set_timer_threshold(gear, pufs_context->active_lanes);
#endif

	data =  ((PWRMODE_SLOWAUTO_MODE << 4) | PWRMODE_SLOWAUTO_MODE);
	e = tegrabl_ufs_set_dme_command(DME_SET, 0, pa_pwr_mode, &data);
	return e;
}


tegrabl_error_t
tegrabl_ufs_set_timer_threshold(uint32_t gear,
	uint32_t active_lanes)
{
	tegrabl_error_t e;
	uint32_t data;

	if (active_lanes == 2) {

		data = dme_fc0_protection_timeout_val_x2[gear-1];
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				dme_fc0protectiontimeoutval, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		e = tegrabl_ufs_set_dme_command(DME_SET, 0, pwr_mode_user_data0,
			&data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		data = dme_tc0_replay_timeout_val_x2[gear-1];
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				dme_tc0replaytimeoutval, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		e = tegrabl_ufs_set_dme_command(DME_SET, 0, pwr_mode_user_data1,
			&data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		data = dme_afc0_req_timeout_val_x2[gear-1];
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				dme_afc0reqtimeoutval, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		e = tegrabl_ufs_set_dme_command(DME_SET, 0, pwr_mode_user_data2,
			&data);
		if (e != TEGRABL_NO_ERROR)
			return e;
		} else {
		data = dme_fc0_protection_timeout_val_x1[gear-1];
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				dme_fc0protectiontimeoutval, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		e = tegrabl_ufs_set_dme_command(DME_SET, 0, pwr_mode_user_data0,
			&data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		data = dme_tc0_replay_timeout_val_x1[gear-1];
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				dme_tc0replaytimeoutval, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		e = tegrabl_ufs_set_dme_command(DME_SET, 0, pwr_mode_user_data1,
			&data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}

		data = dme_afc0_req_timeout_val_x1[gear-1];
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				dme_afc0reqtimeoutval, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
		e = tegrabl_ufs_set_dme_command(DME_SET, 0,
				pwr_mode_user_data2, &data);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
	}
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t
tegrabl_ufs_test_unit_ready(uint32_t lun)
{
	struct cmd_descriptor *plcmd_descriptor = NULL;
	struct command_upiu *pcommand_upiu = NULL;
	struct response_upiu *presponse_upiu;

	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));

	pcommand_upiu =
		(struct command_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;

	pcommand_upiu->basic_header.trans_code = UPIU_COMMAND_TRANSACTION;
	pcommand_upiu->basic_header.lun = lun;
	pcommand_upiu->basic_header.cmd_set_type = UPIU_COMMAND_SET_SCSI;
	pcommand_upiu->expected_data_tx_len_bige = 0;
	pcommand_upiu->cdb[0] = 0x0;

	error = tegrabl_ufs_create_trd(trd_index,
			cmd_desc_index, DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_queue_trd(trd_index, REQUEST_SENSE_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
			REQUEST_SENSE_TIMEOUT);

	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);

	presponse_upiu =
		(struct response_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;

	if (presponse_upiu->basic_header.trans_code !=
		UPIU_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	if (presponse_upiu->basic_header.status == SCSI_STATUS_GOOD)
		error = TEGRABL_NO_ERROR;
	else if (presponse_upiu->basic_header.status == SCSI_STATUS_BUSY)
		error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 0U);
	else if (presponse_upiu->basic_header.status == SCSI_STATUS_CHECK_CONDITION)
		error = TEGRABL_ERROR(TEGRABL_ERR_CHECK_CONDITION, 0U);
	else
		error = TEGRABL_ERROR(TEGRABL_ERR_UNKNOWN_SCSI_STATUS, 0U);

	tegrabl_ufs_free_trd_cmd_desc();
	return error;
}

/* This is used to request sense data. */
tegrabl_error_t tegrabl_ufs_request_sense(uint32_t lun)
{
	struct cmd_descriptor *plcmd_descriptor = NULL;
	struct command_upiu *pcommand_upiu = NULL;
	struct response_upiu *presponse_upiu;

	uint32_t trd_index = 0;
	uint32_t cmd_desc_index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_ufs_get_tx_rx_descriptor(&trd_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_get_cmd_descriptor(&cmd_desc_index);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	plcmd_descriptor = &pcmd_descriptor[cmd_desc_index];
	memset((void *)plcmd_descriptor, 0, sizeof(struct cmd_descriptor));
	pcommand_upiu =
		(struct command_upiu *)&plcmd_descriptor->vucd_generic_req_upiu;

	pcommand_upiu->basic_header.trans_code = UPIU_COMMAND_TRANSACTION;
	pcommand_upiu->basic_header.lun = lun;
	pcommand_upiu->basic_header.cmd_set_type = UPIU_COMMAND_SET_SCSI;
	pcommand_upiu->expected_data_tx_len_bige = 0;
	pcommand_upiu->cdb[0] = 0x3;
	pcommand_upiu->cdb[4] = 0x0;

	error = tegrabl_ufs_create_trd(trd_index, cmd_desc_index, DATA_DIR_NIL, 1);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_ufs_queue_trd(trd_index, REQUEST_SENSE_TIMEOUT);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}
	error = tegrabl_ufs_wait_trd_request_complete(trd_index,
			REQUEST_SENSE_TIMEOUT);

	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_UFS, 0,
		&plcmd_descriptor->vucd_generic_resp_upiu,
		sizeof(union ucd_generic_resp_upiu),
		TEGRABL_DMA_FROM_DEVICE);

	presponse_upiu =
		(struct response_upiu *)&plcmd_descriptor->vucd_generic_resp_upiu;

	if (presponse_upiu->basic_header.trans_code !=
		UPIU_RESPONSE_TRANSACTION) {
		pr_error("%s: response transaction failed\n", __func__);
		error = TEGRABL_ERR_COMMAND_FAILED;
	}

	if (presponse_upiu->basic_header.status == SCSI_STATUS_GOOD)
		error = TEGRABL_NO_ERROR;
	else if (presponse_upiu->basic_header.status == SCSI_STATUS_BUSY)
		error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 0U);
	else if (presponse_upiu->basic_header.status == SCSI_STATUS_CHECK_CONDITION)
		error = TEGRABL_ERROR(TEGRABL_ERR_CHECK_CONDITION, 0U);
	else
		error = TEGRABL_ERROR(TEGRABL_ERR_UNKNOWN_SCSI_STATUS, 0U);

	tegrabl_ufs_free_trd_cmd_desc();

	return error;
}

tegrabl_error_t tegrabl_ufs_get_manufacture_id(uint16_t *manufacture_id)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t desc_buf[256];
	uint32_t i;
	static struct vendor_id_info m_info[] = {
		{MANUFACTURE_TOSHIBA, "Toshiba"},
		{MANUFACTURE_SAMSUNG, "Samsung"},
		{MANUFACTURE_HYNIX,   "Hynix"},
		{MANUFACTURE_UNKNOWN, "Unknown"}
	};

	memset(desc_buf, 0, 256);
	err = tegrabl_ufs_get_descriptor(&desc_buf[0],
			QUERY_DESC_DEVICE_DESC_IDN, 0x0);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("read device descriptor failed. (err:0x%x)\n", err);
	} else {
		*manufacture_id = (desc_buf[0x18] << 8) | desc_buf[0x19];
		for (i = 0; i < sizeof(m_info)/sizeof(struct vendor_id_info) - 1;
									 i++) {
			if (*manufacture_id == m_info[i].id) {
				break;
			}
		}
		pr_info("manufacture id(0x%04x) : %s\n", *manufacture_id,
				m_info[i].name);
	}
	return err;
}

tegrabl_error_t
tegrabl_ufs_get_provisioning_type(uint32_t lun, uint8_t *provision_type)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t desc_buf[256];

	memset(desc_buf, 0, 256);
	err = tegrabl_ufs_get_descriptor(&desc_buf[0],
			QUERY_DESC_UNIT_DESC_IDN, lun);
	if (err != TEGRABL_NO_ERROR) {
		*provision_type = 0;
		pr_info("read unit descriptor failed. (err:0x%x)\n", err);
	} else {
		*provision_type = desc_buf[0x17];
	}
	pr_info("provision_type = 0x%02x\n", *provision_type);

	return err;
}

void tegrabl_ufs_default_state(void)
{
	/* disable host controller */
	UFS_WRITE32(HCE, 0);
	tegrabl_ufs_disable_device();
}

#ifdef UFS_TEST
tegrabl_error_t ufs_test_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_ufs_context context = {0};
	struct tegrabl_ufs_params *params;
	uint32_t *buffer = NULL;
	uint32_t *outbuffer = NULL;

	buffer = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			1024, 4096);
	outbuffer = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			1024, 4096);

	buffer[0] = 0xdfadbead;
	outbuffer[1] = 0xaaaaaaaa;

	pr_debug("test read %x\n", *buffer);

	tegrabl_ufs_get_params(0, &params);

	err = tegrabl_ufs_init(params, &context);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("init failed\n");
		return err;
	}
	err = tegrabl_ufs_write(0, 0, 1, buffer);
	err = tegrabl_ufs_read(0, 0, 1, outbuffer);
	err = tegrabl_ufs_write(0, 0, 1, buffer);
	err = tegrabl_ufs_read(0, 0, 1, outbuffer);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("read failed\n");
	}
	pr_info("read out put %x\n",outbuffer[0]);

	if (buffer) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, buffer);
	}
	if (outbuffer) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, outbuffer);
	}
	return TEGRABL_NO_ERROR;
}
#endif
