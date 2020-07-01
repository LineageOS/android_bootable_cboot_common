/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_HDMI

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_clock.h>
#include <tegrabl_malloc.h>
#include <tegrabl_timer.h>
#include <tegrabl_drf.h>
#include <ardisplay.h>
#include <arsor1.h>
#include <address_map_new.h>
#include <tegrabl_hdmi.h>
#include <tegrabl_sor.h>
#include <string.h>
#include <tegrabl_i2c_dev.h>
#include <kernel/thread.h>

#define SCDC_STABILIZATION_DELAY_MS 20
#define SCDC_THREAD_SLEEP_MS 5000
#define SCDC_SLAVE 0xA8

#define CLK_4K_30 297000000
#define WIDTH_4K_3840 3840
#define WIDTH_4K_4096 4096
#define HEIGHT_4K 2160

#define MHZ 1000000
#define KHZ 1000

#define DUMP_SOR_REGS 1

static struct tegrabl_i2c_dev *hi2c;

/* hdmi licensing, LLC vsi playload len as per hdmi1.4b  */
#define HDMI_INFOFRAME_LEN_VENDOR_LLC	(6)

static tegrabl_error_t hdmi_init(struct tegrabl_nvdisp *nvdisp,
	struct tegrabl_display_pdata *pdata)
{
	struct hdmi *hdmi;
	uint32_t err = TEGRABL_NO_ERROR;
	struct sor_data *sor;

	pr_debug("%s: entry hdmi_init\n", __func__);
	if (!nvdisp || !nvdisp->mode) {
		pr_error("%s: nvdisp or nvdisp->mode is NULL\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	hdmi = tegrabl_malloc(sizeof(struct hdmi));
	if (!hdmi) {
		pr_error("%s, memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(hdmi, 0, sizeof(struct hdmi));

	pr_debug("%s: initialize HDMI on SOR%d\n", __func__, pdata->sor_instance);
	err = sor_init(&sor, pdata->sor_instance);
	if (err != TEGRABL_NO_ERROR) {
		tegrabl_free(hdmi);
		goto fail;
	}
	sor->nvdisp = nvdisp;

	hdmi->sor_instance = pdata->sor_instance;
	hdmi->sor = sor;
	hdmi->nvdisp = nvdisp;
	hdmi->enabled = false;
	hdmi->hdmi_dtb = &pdata->hdmi_dtb;
	nvdisp->out_data = hdmi;

	pr_debug("%s: exit hdmi_init\n", __func__);

fail:
	return err;
}

static void hdmi_destroy(struct tegrabl_nvdisp *nvdisp)
{
	/* Dummy function */
}

static void hdmi_config(struct hdmi *hdmi)
{
	struct sor_data *sor = hdmi->sor;
	struct tegrabl_nvdisp *nvdisp = hdmi->nvdisp;
	uint32_t reg_val;
	uint32_t dispclk_div_8_2;
	uint32_t hblank, max_ac, rekey, val;

	pr_debug("%s entry\n", __func__);

	reg_val = sor_readl(sor, SOR_NV_PDISP_INPUT_CONTROL_0);
	if (nvdisp->instance == 0) {
		reg_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, INPUT_CONTROL,
									 HDMI_SRC_SELECT, DISPLAY, reg_val);
	} else {
		reg_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, INPUT_CONTROL,
									 HDMI_SRC_SELECT, DISPLAYB, reg_val);
	}

	reg_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, INPUT_CONTROL, ARM_VIDEO_RANGE,
								 FULL, reg_val);
	sor_writel(sor, SOR_NV_PDISP_INPUT_CONTROL_0, reg_val);

	dispclk_div_8_2 = (nvdisp->mode->pclk / MHZ) * 4;
	sor_writel(sor, SOR_NV_PDISP_SOR_REFCLK_0,
			   NV_SOR_REFCLK_DIV_INT(dispclk_div_8_2 >> 2) |
			   NV_SOR_REFCLK_DIV_FRAC(dispclk_div_8_2));

	/*
	 * The rekey register and corresponding eq want to operate
	 * on "-2" of the desired rekey value
	 */
	rekey = NV_SOR_HDMI_CTRL_REKEY_DEFAULT - 2;
	hblank = nvdisp->mode->h_sync_width + nvdisp->mode->h_back_porch +
			nvdisp->mode->h_front_porch;
	max_ac = (hblank - rekey - 18) / 32;

	val = 0;
	val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_CTRL, ENABLE, EN);
	val |= NV_SOR_HDMI_CTRL_REKEY(rekey);
	val |= NV_SOR_HDMI_CTRL_MAX_AC_PACKET(max_ac);
	sor_writel(sor, SOR_NV_PDISP_SOR_HDMI_CTRL_0, val);

	pr_debug("%s: exit\n", __func__);
}

static uint32_t hdmi_get_aspect_ratio(struct hdmi *hdmi)
{
	uint32_t aspect_ratio;

	switch (hdmi->nvdisp->mode->avi_m) {
	case NVDISP_MODE_AVI_M_4_3:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_4_3;
		break;
	case NVDISP_MODE_AVI_M_16_9:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_16_9;
		break;
	/*
	 * no avi_m field for picture aspect ratio 64:27 and 256:135.
	 * sink detects via VIC, avi_m is 0.
	 */
	case NVDISP_MODE_AVI_M_64_27: /* fall through */
	case NVDISP_MODE_AVI_M_256_135: /* fall through */
	default:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_NO_DATA;
	}

	return aspect_ratio;
}

static void hdmi_avi_infoframe_update(struct hdmi *hdmi)
{
	struct avi_infoframe *avi = &hdmi->avi;
	struct tegrabl_nvdisp *nvdisp = hdmi->nvdisp;

	memset(&hdmi->avi, 0, sizeof(hdmi->avi));

	avi->scan = HDMI_AVI_UNDERSCAN;
	avi->bar_valid = HDMI_AVI_BAR_INVALID;
	avi->act_fmt_valid = HDMI_AVI_ACTIVE_FORMAT_INVALID;

	/* TODO: Support ycc format */
	avi->rgb_ycc = HDMI_AVI_RGB;

	avi->act_format = HDMI_AVI_ACTIVE_FORMAT_SAME;
	avi->aspect_ratio = hdmi_get_aspect_ratio(hdmi);
	avi->colorimetry = HDMI_AVI_COLORIMETRY_DEFAULT;

	avi->scaling = HDMI_AVI_SCALING_UNKNOWN;
	avi->rgb_quant = HDMI_AVI_RGB_QUANT_DEFAULT;
	avi->ext_colorimetry = HDMI_AVI_EXT_COLORIMETRY_INVALID;
	avi->it_content = HDMI_AVI_IT_CONTENT_FALSE;

	/* set correct vic if video format is cea defined else set 0 */
	avi->video_format = nvdisp->mode->vic;

	avi->pix_rep = HDMI_AVI_NO_PIX_REPEAT;
	avi->it_content_type = HDMI_AVI_IT_CONTENT_NONE;
	avi->ycc_quant = HDMI_AVI_YCC_QUANT_NONE;

	avi->top_bar_end_line_low_byte = 0;
	avi->top_bar_end_line_high_byte = 0;

	avi->bot_bar_start_line_low_byte = 0;
	avi->bot_bar_start_line_high_byte = 0;

	avi->left_bar_end_pixel_low_byte = 0;
	avi->left_bar_end_pixel_high_byte = 0;

	avi->right_bar_start_pixel_low_byte = 0;
	avi->right_bar_start_pixel_high_byte = 0;
}

static void hdmi_infoframe_pkt_write(struct hdmi *hdmi, uint32_t header_reg,
									 uint8_t pkt_type, uint8_t pkt_vs,
									 uint8_t pkt_len, void *reg_payload,
									 uint32_t reg_payload_len)
{
	struct sor_data *sor = hdmi->sor;
	uint32_t val;
	uint32_t *data = reg_payload;
	uint32_t data_reg = header_reg + 1;

	val = NV_SOR_HDMI_INFOFRAME_HEADER_TYPE(pkt_type) |
		NV_SOR_HDMI_INFOFRAME_HEADER_VERSION(pkt_vs) |
		NV_SOR_HDMI_INFOFRAME_HEADER_LEN(pkt_len);
	sor_writel(sor, header_reg, val);

	for (val = 0; val < reg_payload_len; val += 4, data_reg++, data++)
		sor_writel(sor, data_reg, *data);
}

static void hdmi_avi_infoframe(struct hdmi *hdmi)
{
	struct sor_data *sor = hdmi->sor;

	pr_debug("%s: entry\n", __func__);

	/* disable avi infoframe before configuring */
	sor_writel(sor, SOR_NV_PDISP_SOR_HDMI_AVI_INFOFRAME_CTRL_0, 0);

	hdmi_avi_infoframe_update(hdmi);

	hdmi_infoframe_pkt_write(hdmi,
							 SOR_NV_PDISP_SOR_HDMI_AVI_INFOFRAME_HEADER_0,
							 HDMI_INFOFRAME_TYPE_AVI,
							 HDMI_INFOFRAME_VS_AVI,
							 HDMI_INFOFRAME_LEN_AVI,
							 &hdmi->avi, sizeof(hdmi->avi));

	/* Send infoframe every frame, checksum hw generated */
	sor_writel(sor, SOR_NV_PDISP_SOR_HDMI_AVI_INFOFRAME_CTRL_0,
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_AVI_INFOFRAME_CTRL, ENABLE, YES) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_AVI_INFOFRAME_CTRL, OTHER, DIS) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_AVI_INFOFRAME_CTRL, SINGLE, DIS) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_AVI_INFOFRAME_CTRL, CHKSUM_HW,
				  ENABLE));

	pr_debug("%s: exit\n", __func__);
}

static void hdmi_vendor_infoframe_update(struct hdmi *hdmi)
{
	struct hdmi_vendor_infoframe *vsi = &hdmi->vsi;
	struct tegrabl_nvdisp *nvdisp = hdmi->nvdisp;

	pr_debug("%s: entry\n", __func__);

	memset(&hdmi->vsi, 0, sizeof(hdmi->vsi));

	vsi->oui = HDMI_LICENSING_LLC_OUI;

	if ((nvdisp->mode->pclk == CLK_4K_30) && ((nvdisp->mode->h_active ==
		WIDTH_4K_3840) || (nvdisp->mode->h_active == WIDTH_4K_4096)) &&
		(nvdisp->mode->v_active == HEIGHT_4K)) {
		vsi->video_format = HDMI_VENDOR_VIDEO_FORMAT_EXTENDED;
		vsi->extended_vic = 1;
	} else {
		vsi->video_format = HDMI_VENDOR_VIDEO_FORMAT_NONE;
		vsi->extended_vic = 0;
	}
	pr_debug("%s: exit\n", __func__);
}

static void hdmi_vendor_infoframe(struct hdmi *hdmi)
{
	struct sor_data *sor = hdmi->sor;

	pr_debug("%s: entry\n", __func__);

	/* disable vsi infoframe before configuring */
	sor_writel(sor, SOR_NV_PDISP_SOR_HDMI_VSI_INFOFRAME_CTRL_0, 0);

	hdmi_vendor_infoframe_update(hdmi);

	hdmi_infoframe_pkt_write(hdmi,
							 SOR_NV_PDISP_SOR_HDMI_VSI_INFOFRAME_HEADER_0,
							 HDMI_INFOFRAME_TYPE_VENDOR,
							 HDMI_INFOFRAME_VS_VENDOR,
							 HDMI_INFOFRAME_LEN_VENDOR_LLC,
							 &hdmi->vsi, sizeof(hdmi->vsi));

	/* Send infoframe every frame, checksum hw generated */
	sor_writel(sor, SOR_NV_PDISP_SOR_HDMI_VSI_INFOFRAME_CTRL_0,
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_VSI_INFOFRAME_CTRL, ENABLE, YES) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_VSI_INFOFRAME_CTRL, OTHER, DIS) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_VSI_INFOFRAME_CTRL, SINGLE, DIS) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_HDMI_VSI_INFOFRAME_CTRL, CHKSUM_HW,
				  ENABLE));

	pr_debug("%s: exit\n", __func__);
}

static void hdmi_config_lanes(struct hdmi *hdmi)
{
	struct sor_data *sor = hdmi->sor;

	pr_debug("%s: entry\n", __func__);

	sor_writel(sor, (SOR_NV_PDISP_SOR_LANE_PREEMPHASIS0_0 + sor->portnum), 0x0);

	pr_debug("%s: exit\n", __func__);
}

static void hdmi_clock_config(struct tegrabl_nvdisp *nvdisp,  int32_t instance,
							  uint32_t clk_type)
{
	uint32_t clk_rate = 0;
	uint32_t pclk = nvdisp->mode->pclk / KHZ;

	pr_debug("%s: entry\n", __func__);

	pr_debug("%s: config HDMI clk on SOR%d, clk_type %d\n", __func__, instance,
			 clk_type);

	if (clk_type == TEGRA_SOR_SAFE_CLK) {
		tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_SAFE, 0,
								TEGRABL_CLK_SRC_PLLP_OUT0);
		tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_SAFE, 0, NULL);

		tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_OUT, instance,
								TEGRABL_CLK_SRC_SOR_SAFE_CLK);
		tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_OUT, instance, NULL);
		tegrabl_udelay(20);

		tegrabl_car_rst_set(TEGRABL_MODULE_SOR, instance);
		if (instance == 0) { /* SOR */
			tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR, instance,
									TEGRABL_CLK_SRC_PLLD3_OUT0);
		} else { /* SOR1 */
			tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR, instance,
									TEGRABL_CLK_SRC_PLLD2_OUT0);
		}
		if (pclk > (MAX_1_4_FREQUENCY / KHZ)) {
			tegrabl_car_set_clk_rate(TEGRABL_MODULE_SOR, instance, pclk >> 1,
									 &clk_rate);
		} else {
			tegrabl_car_set_clk_rate(TEGRABL_MODULE_SOR, instance, pclk,
									 &clk_rate);
		}
		tegrabl_car_clk_enable(TEGRABL_MODULE_SOR, instance, NULL);
		tegrabl_car_rst_clear(TEGRABL_MODULE_SOR, instance);
		tegrabl_udelay(20);
	} else if (clk_type == TEGRA_SOR_LINK_CLK) {
		if (pclk > (MAX_1_4_FREQUENCY / KHZ)) {
			tegrabl_car_set_clk_rate(TEGRABL_MODULE_SOR_PAD_CLKOUT, instance,
									 pclk >> 1, &clk_rate);
		} else {
			tegrabl_car_set_clk_rate(TEGRABL_MODULE_SOR_PAD_CLKOUT, instance,
									 pclk, &clk_rate);
		}
		tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_PAD_CLKOUT, instance, NULL);
		if (instance == 0) { /* SOR */
			tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_OUT, instance,
									TEGRABL_CLK_SRC_SOR0_PAD_CLKOUT);
		} else { /* SOR1 */
			tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_OUT, instance,
									TEGRABL_CLK_SRC_SOR1_PAD_CLKOUT);
		}
		tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_OUT, instance, NULL);
		tegrabl_udelay(250);
	} else {
		pr_error("%s: incorrect clk type\n", __func__);
	}

	pr_debug("%s: exit\n", __func__);
}

static tegrabl_error_t hdmi_v2_x_mon_config(bool enable)
{
	uint8_t reg;
	uint8_t offset = HDMI_SCDC_TMDS_CONFIG_OFFSET;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (enable) {
		reg = (HDMI_SCDC_TMDS_CONFIG_BIT_CLK_RATIO_40 |
		HDMI_SCDC_TMDS_CONFIG_SCRAMBLING_EN);
	} else {
		reg = 0;
	}

	err = tegrabl_i2c_dev_write(hi2c, &reg, offset, sizeof(uint8_t));
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: i2c write failed for SCDC slave\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED, 1);
		return err;
	}

	return err;
}

static void hdmi_v2_x_host_config(struct hdmi *hdmi, bool enable)
{
	uint32_t val;
	struct sor_data *sor = hdmi->sor;

	if (enable) {
		sor_writel_def(SOR_HDMI2_CTRL, SCRAMBLE, ENABLE, val);
		sor_writel_def(SOR_HDMI2_CTRL, CLOCK_MODE, MODE_DIV_BY_4, val);
	} else {
		sor_writel_def(SOR_HDMI2_CTRL, SCRAMBLE, DISABLE, val);
		sor_writel_def(SOR_HDMI2_CTRL, CLOCK_MODE, NORMAL, val);
	}
}

static tegrabl_error_t hdmi_v2_x_config(struct hdmi *hdmi)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* reset hdmi2.x config on host and monitor */
	err = hdmi_v2_x_mon_config(false);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	hdmi_v2_x_host_config(hdmi, false);

	err = hdmi_v2_x_mon_config(true);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_mdelay(SCDC_STABILIZATION_DELAY_MS);
	hdmi_v2_x_host_config(hdmi, true);

fail:
	return err;
}

static int monitor_scdc_block(void *arg)
{
	uint8_t reg;
	uint8_t offset = HDMI_SCDC_TMDS_CONFIG_OFFSET;
	struct hdmi *hdmi = arg;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (true) {
		reg = 0x0;
		err = tegrabl_i2c_dev_read(hi2c, &reg, offset, sizeof(uint8_t));
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: i2c read failed for SCDC slave\n", __func__);
			thread_exit(0);
		}

		if (reg == 0) {
			err = hdmi_v2_x_config(hdmi);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("programming scdc block failed\n");
				thread_exit(0);
			}
		}
		thread_sleep(SCDC_THREAD_SLEEP_MS);
	}

	/*Not reached, but could return an error here*/
	return err;
}

static void schedule_delayed_work(struct hdmi *hdmi)
{
	thread_t *t;
	t = thread_create("monitor_scdc_block", monitor_scdc_block, hdmi,
					  DEFAULT_PRIORITY, DEFAULT_STACK_SIZE);
	if (!t)
		pr_error("thread creation failed\n");
	else {
		if (thread_detach(t))
			pr_error("thread detach failed\n");
		else if (thread_resume(t))
			pr_error("thread resume failed\n");
	}
}

static void hdmi_prod_settings(struct hdmi *hdmi)
{
	uint32_t i;
	uint32_t j;
	struct prod_settings *prod_settings;
	struct prod_tuple prod_tuple;
	uint32_t val;
	struct sor_data *sor = hdmi->sor;
	struct prod_list *prod_list = hdmi->hdmi_dtb->prod_list;

	if (!prod_list) {
		pr_info("HDMI prod settings not found!\n");
		return;
	}

	prod_settings = prod_list->prod_settings;
	for (i = 0; i < prod_list->num; i++) {
		if (hdmi->nvdisp->mode->pclk <= tmds_config_modes[i].clk) {
			break;
		}
	}

	if (i < prod_list->num) {
		for (j = 0; j < prod_list->prod_settings[i].count; j++) {
			prod_tuple = prod_settings[i].prod_tuple[j];
			val = sor_readl(sor, prod_tuple.addr / 4);
			val = ((val & prod_tuple.mask) |
				(prod_tuple.val & prod_tuple.mask));
				/*the above logic is based on 1-style masking*/
			sor_writel(sor, prod_tuple.addr / 4, val);
		}
	}
}

static tegrabl_error_t hdmi_enable(struct tegrabl_nvdisp *nvdisp)
{
	struct hdmi *hdmi = nvdisp->out_data;
	struct sor_data *sor = hdmi->sor;
	uint32_t status = TEGRABL_NO_ERROR;

	pr_info("%s, starting HDMI initialisation\n", __func__);

	hdmi_clock_config(nvdisp, hdmi->sor_instance, TEGRA_SOR_SAFE_CLK);

	sor_writel(sor, SOR_NV_PDISP_SOR_SEQ_INST0_0, 0x8080);

	sor_hdmi_pad_power_up(sor);
	sor_power_lanes(sor, 4, true);

	sor_set_internal_panel(sor, false);

	hdmi_config_lanes(hdmi);

	hdmi_config(hdmi);

	hdmi_avi_infoframe(hdmi);

	hdmi_vendor_infoframe(hdmi);

	status = sor_set_power_state(sor, 1);
	if (status != TEGRABL_NO_ERROR) {
		return status;
	}

	hdmi_prod_settings(hdmi);

	sor_config_hdmi_clk(sor, nvdisp->mode->pclk);
	hdmi_clock_config(nvdisp, hdmi->sor_instance, TEGRA_SOR_LINK_CLK);

	sor_attach(sor);

	sor_writel(hdmi->sor, SOR_NV_PDISP_SOR_XBAR_CTRL_0, 0x8d111a20);

	if (nvdisp->mode->pclk > MAX_1_4_FREQUENCY) {
		hi2c = tegrabl_i2c_dev_open(TEGRABL_INSTANCE_I2C4, SCDC_SLAVE, 1, 1);
		if (!hi2c) {
			pr_debug("%s, invalid i2c handle\n", __func__);
			return TEGRABL_ERROR(TEGRABL_ERR_I2C_DEV, 0);
		}
		if (hdmi_v2_x_config(hdmi) == TEGRABL_NO_ERROR) {
			schedule_delayed_work(hdmi);
		}
	}

#if DUMP_SOR_REGS /*enable register dump if required*/
	sor_dump_registers(sor);
#endif
	pr_info("%s, HDMI initialisation complete\n", __func__);
	return status;
}

static uint64_t hdmi_setup_clk(struct tegrabl_nvdisp *nvdisp, uint32_t clk_id)
{
	/* Dummy function */
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t hdmi_disable(struct tegrabl_nvdisp *nvdisp)
{
	/* Dummy function */
	return TEGRABL_NO_ERROR;
}

struct nvdisp_out_ops hdmi_ops = {
	.init = hdmi_init,
	.enable = hdmi_enable,
	.disable = hdmi_disable,
	.setup_clk = hdmi_setup_clk,
};
