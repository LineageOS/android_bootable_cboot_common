/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_USBH

#include <string.h>
#include <stdbool.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <arxusb_padctl.h>
#include <xhci_priv.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_xusbh_soc.h>

#define NV_XUSB_PADCTL_READ(reg, value) \
	value = NV_READ32((NV_ADDRESS_MAP_XUSB_PADCTL_BASE + XUSB_PADCTL_##reg##_0))

#define NV_XUSB_PADCTL_WRITE(reg, value) \
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE + XUSB_PADCTL_##reg##_0, value)

void xhci_dump_padctl(void)
{
	uint32_t *addr;
	uint32_t *end;

	pr_info("dumping xusb padctl registers\n");
	addr = (uint32_t *)(NV_ADDRESS_MAP_XUSB_PADCTL_BASE);
	end = (uint32_t *)(NV_ADDRESS_MAP_XUSB_PADCTL_BASE + 0x36c);

	while (addr <= end) {
		pr_info("%p: %08x %08X %08X %08X\n", addr, *(addr), *(addr + 1),
						*(addr + 2), *(addr + 3));
		addr += 4;
	}
	pr_info("\n");
}

void xhci_init_pinmux(void)
{
	/* TODO */
}

void xhci_release_ss_wakestate_latch(void)
{
	uint32_t reg_val;

	NV_XUSB_PADCTL_READ(ELPG_PROGRAM_1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP3_ELPG_CLAMP_EN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP3_ELPG_CLAMP_EN_EARLY, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP3_ELPG_VCORE_DOWN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP2_ELPG_CLAMP_EN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP2_ELPG_CLAMP_EN_EARLY, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP2_ELPG_VCORE_DOWN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP1_ELPG_CLAMP_EN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP1_ELPG_CLAMP_EN_EARLY, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP1_ELPG_VCORE_DOWN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP0_ELPG_CLAMP_EN, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP0_ELPG_CLAMP_EN_EARLY, 0x0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP0_ELPG_VCORE_DOWN, 0x0, reg_val);
	NV_XUSB_PADCTL_WRITE(ELPG_PROGRAM_1, reg_val);
}

void xhci_usb3_phy_power_off(void)
{
	uint32_t reg_val;

	NV_XUSB_PADCTL_READ(ELPG_PROGRAM_1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP3_ELPG_CLAMP_EN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP3_ELPG_CLAMP_EN_EARLY, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP3_ELPG_VCORE_DOWN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP2_ELPG_CLAMP_EN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP2_ELPG_CLAMP_EN_EARLY, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP2_ELPG_VCORE_DOWN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP1_ELPG_CLAMP_EN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP1_ELPG_CLAMP_EN_EARLY, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP1_ELPG_VCORE_DOWN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP0_ELPG_CLAMP_EN, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP0_ELPG_CLAMP_EN_EARLY, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1, SSP0_ELPG_VCORE_DOWN, 0x1, reg_val);
	NV_XUSB_PADCTL_WRITE(ELPG_PROGRAM_1, reg_val);
}

tegrabl_error_t xhci_init_bias_pad(void)
{
	uint32_t reg_val;
	uint32_t hs_squelch_level;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	e = tegrabl_fuse_read(FUSE_USB_CALIB, &hs_squelch_level, 4);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("Failed to read USB_CALIB fuse\n");
		goto fail;
	}

	NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_0, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, PD, SW_DEFAULT, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, HS_SQUELCH_LEVEL,
								  (hs_squelch_level >> 29), reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, HS_DISCON_LEVEL,
								  0x7, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_0, reg_val);
	tegrabl_udelay(1);

	/* Program BIAS pad tracking */
	/* enable tracking clocks */
	/**
	 * 1. CLK_OUT_ENB_USB2_HSIC_TRK_SET => CLK_ENB_USB2_TRK
	 * 2. CLK_SOURCE_USB2_HSIC_TRK => USB2_HSIC_TRK_CLK_DM_SOR
	 */
	tegrabl_usbf_program_tracking_clock(true);

	NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1, TRK_COMPLETED, 0x1, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_1, reg_val);

	NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, reg_val);
	reg_val |= NV_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1, TRK_START_TIMER, 0x1E);
	reg_val |= NV_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1, TRK_DONE_RESET_TIMER, 0xA);
	NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_1, reg_val);

	NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_0, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, PD, SW_DEFAULT, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_0, reg_val);
	tegrabl_udelay(1);
	NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1, PD_TRK, SW_DEFAULT, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_1, reg_val);
	tegrabl_udelay(1);

	do {
		NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, reg_val);
		reg_val = NV_DRF_VAL(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1, TRK_COMPLETED, reg_val);
	} while (reg_val != 0x1);

	/* disable tracking clock: CLK_OUT_ENB_USB2_HSIC_TRK_CLR => CLK_ENB_USB2_TRK */
	tegrabl_usbf_program_tracking_clock(false);
fail:
	return e;
}

void xhci_power_down_bias_pad(void)
{
	uint32_t reg_val;

	/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, power down the bias pad */
	NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_0, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, PD, 0x1, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_0, reg_val);
}
tegrabl_error_t xhci_init_usb2_padn(void)
{
	uint32_t reg_data;
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t usb_calib;
	uint32_t usb_calib_ext;
	uint32_t hs_curr_level;
	uint32_t term_range_adj;
	uint32_t rpd_ctrl;

	e = tegrabl_fuse_read(FUSE_USB_CALIB, &usb_calib, 4);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("Failed to read USB_CALIB fuse\n");
		goto fail;
	}
	hs_curr_level = usb_calib & 0x3F;
	term_range_adj = usb_calib & 0x780;
	term_range_adj = term_range_adj >> 7;

	e = tegrabl_fuse_read(FUSE_USB_CALIB_EXT, &usb_calib_ext, 4);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("Failed to read USB_CALIB_EXT fuse\n");
		goto fail;
	}
	rpd_ctrl = usb_calib_ext & 0x1F;

	/* USB2_OTG_PADn_CTL_0 */
	NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_0, reg_data);

	/* USB2_OTG_PADn_CTL_1 */
	NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, PD_DR, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, TERM_RANGE_ADJ, term_range_adj, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, RPD_CTRL, rpd_ctrl, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, PD_DR, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, TERM_RANGE_ADJ, term_range_adj, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, RPD_CTRL, rpd_ctrl, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, PD_DR, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, TERM_RANGE_ADJ, term_range_adj, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, RPD_CTRL, rpd_ctrl, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, PD_DR, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, TERM_RANGE_ADJ, term_range_adj, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, RPD_CTRL, rpd_ctrl, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_1, reg_data);

	/* Assign port capabilities for 2.0 and superspeed ports */
	NV_XUSB_PADCTL_READ(USB2_PORT_CAP, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT0_CAP, HOST_ONLY, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT1_CAP, HOST_ONLY, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT2_CAP, HOST_ONLY, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT3_CAP, HOST_ONLY, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_PORT_CAP, reg_data);

	NV_XUSB_PADCTL_READ(USB2_PAD_MUX, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT0, XUSB, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT1, XUSB, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT2, XUSB, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT3, XUSB, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_PAD_MUX, reg_data);

fail:
	return e;
}

void xhci_power_down_usb2_padn(void)
{

	uint32_t reg_data;

	/* XUSB_PADCTL_USB2_OTG_PADX_CTL_0_0 */
	NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD_ZI, 0x1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD_ZI, 0x1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD_ZI, 0x1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD_ZI, 0x1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_0, reg_data);

	/* XUSB_PADCTL_USB2_OTG_PADX_CTL_1_0 */
	NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, PD_DR, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, PD_DR, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, PD_DR, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, PD_DR, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_1, reg_data);


}

bool xhci_set_root_port(struct xusb_host_context *ctx)
{
	uint32_t val;
	int i;

	if (ctx->root_port_number != 0xff) {
		i = ctx->root_port_number + 3;
		pr_debug("port[%d] = 0x%x\n", i, xusbh_xhci_readl(OP_PORTSC(i)));
		if ((xusbh_xhci_readl(OP_PORTSC(i)) & PORT_CONNECT) == PORT_CONNECT) {
			return true;
		} else {
#if 0
			NV_XUSB_PADCTL_READ(USB2_PORT_CAP, val);
			val &= ~(PORT_CAP_MASK << PORT_CAP_SHIFT(i-3));
			NV_XUSB_PADCTL_WRITE(USB2_PORT_CAP, val);
#endif
			ctx->root_port_number = 0xff;
			return false;
		}
	}

	for (i = HOST_PORTS_NUM; i < HOST_PORTS_NUM*2; i++) {
		val = xusbh_xhci_readl(OP_PORTSC(i));
		pr_debug("port[%d] = 0x%x\n", i, val);
		if ((val & PORT_CONNECT) == PORT_CONNECT) {
			ctx->root_port_number = i - 3;
		} else {
			/* disable the pad */
#if 0
			NV_XUSB_PADCTL_READ(USB2_PORT_CAP, val);
			val &= ~(PORT_CAP_MASK << PORT_CAP_SHIFT(i-3));
			NV_XUSB_PADCTL_WRITE(USB2_PORT_CAP, val);
#endif
		}
	}

	if (ctx->root_port_number == 0xff) {
		return false;
	} else {
		return true;
	}
}
