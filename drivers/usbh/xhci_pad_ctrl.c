/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <arxusb_padctl.h>
#include <xhci_priv.h>
#include <tegrabl_timer.h>

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

void xhci_enable_vbus(void)
{
	uint32_t reg_val;

	/* TODO: assign over current signal mapping for usb 2.0 and SS ports */
	NV_XUSB_PADCTL_READ(USB2_OC_MAP, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT3_OC_PIN, OC_DETECTED3, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT2_OC_PIN, OC_DETECTED2, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT1_OC_PIN, OC_DETECTED1, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT0_OC_PIN, OC_DETECTED0, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_OC_MAP, reg_val);

	NV_XUSB_PADCTL_READ(SS_OC_MAP, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT3_OC_PIN, OC_DETECTED3, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT2_OC_PIN, OC_DETECTED2, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT1_OC_PIN, OC_DETECTED1, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT0_OC_PIN, OC_DETECTED0, reg_val);
	NV_XUSB_PADCTL_WRITE(SS_OC_MAP, reg_val);

	NV_XUSB_PADCTL_READ(VBUS_OC_MAP, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE1_OC_MAP, OC_DETECTED1, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE0_OC_MAP, OC_DETECTED0, reg_val);
	NV_XUSB_PADCTL_WRITE(VBUS_OC_MAP, reg_val);

	/* clear false reporting of over current events */
	NV_XUSB_PADCTL_READ(OC_DET, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED3, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED2, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED1, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED0, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD3, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD2, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD1, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD0, YES, reg_val);
	NV_XUSB_PADCTL_WRITE(OC_DET, reg_val);
	tegrabl_udelay(1);

#if 0
	/* enable VBUS for the host ports */
	NV_XUSB_PADCTL_READ(VBUS_OC_MAP, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE3, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE2, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE1, YES, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE0, YES, reg_val);
	NV_XUSB_PADCTL_WRITE(VBUS_OC_MAP, reg_val);
#endif
}

void xhci_vbus_override(void)
{
	uint32_t reg_val;

	/* Local override for VBUS and ID status reporting. */
	NV_XUSB_PADCTL_READ(USB2_VBUS_ID, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_VBUS_ID, ID_SOURCE_SELECT, ID_OVERRIDE, reg_val);
	reg_val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_VBUS_ID, VBUS_SOURCE_SELECT, VBUS_OVERRIDE, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, ID_OVERRIDE, 0x0, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_VBUS_ID, reg_val);

	/* Clear false reporting of VBUS and ID status changes. */
	NV_XUSB_PADCTL_READ(USB2_VBUS_ID, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, IDDIG_ST_CHNG, 0x1, reg_val);
	reg_val = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, VBUS_VALID_ST_CHNG, 0x1, reg_val);
	NV_XUSB_PADCTL_WRITE(USB2_VBUS_ID, reg_val);
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
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, TERM_SEL, 1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, LS_FSLEW, 6, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, LS_RSLEW, 6, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, TERM_SEL, 1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, LS_FSLEW, 6, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, LS_RSLEW, 6, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, TERM_SEL, 1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, LS_FSLEW, 6, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, LS_RSLEW, 6, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_0, reg_data);

	NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_0, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD_ZI, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD, SW_DEFAULT, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, HS_CURR_LEVEL, hs_curr_level, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, TERM_SEL, 1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, LS_FSLEW, 6, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, LS_RSLEW, 6, reg_data);
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

	/* USB Pad protection circuit activation */
	NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD2_CTL1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD2_CTL1, PD_VREG, 0x1, reg_data);
//	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD2_CTL1, VREG_DIR, 0x1, reg_data);
	/* TODO: program this based on VBUS */
//	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD2_CTL1, VREG_LEV, 0x0, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD2_CTL1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD0_CTL1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD2_CTL1, PD_VREG, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD0_CTL1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD1_CTL1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD2_CTL1, PD_VREG, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD1_CTL1, reg_data);

	NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD3_CTL1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD2_CTL1, PD_VREG, 0x1, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD3_CTL1, reg_data);

	/* Assign port capabilities for 2.0 and superspeed ports */
	NV_XUSB_PADCTL_READ(USB2_PORT_CAP, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT0_CAP, HOST_ONLY, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT1_CAP, HOST_ONLY, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT2_CAP, HOST_ONLY, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT3_CAP, HOST_ONLY, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_PORT_CAP, reg_data);

	/* Disable over current signal mapping for 2.0 and SS ports */
	NV_XUSB_PADCTL_READ(USB2_OC_MAP, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT0_OC_PIN, OC_DETECTION_DISABLED, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT1_OC_PIN, OC_DETECTION_DISABLED, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT2_OC_PIN, OC_DETECTION_DISABLED, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT3_OC_PIN, OC_DETECTION_DISABLED, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OC_MAP, reg_data);

	NV_XUSB_PADCTL_READ(VBUS_OC_MAP, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE1_OC_MAP, OC_DETECTION_DISABLED,
								  reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE0_OC_MAP, OC_DETECTION_DISABLED,
								  reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_OC_MAP, reg_data);

	NV_XUSB_PADCTL_READ(USB2_PAD_MUX, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT0, XUSB, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT1, XUSB, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT2, XUSB, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT3, XUSB, reg_data);
	NV_XUSB_PADCTL_WRITE(USB2_PAD_MUX, reg_data);

fail:
	return e;
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

	for (i = 4; i < 8; i++) {
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
