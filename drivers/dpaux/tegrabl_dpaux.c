/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_DPAUX

#include <tegrabl_ar_macro.h>
#include <tegrabl_dpaux.h>
#include <ardpaux.h>
#include <tegrabl_clock.h>
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_malloc.h>
#include <tegrabl_debug.h>
#include <string.h>
#include <tegrabl_i2c.h>
#include <tegrabl_dpaux_soc.h>
#include <tegrabl_dpaux_soc_common.h>
#include <tegrabl_devicetree.h>
#include <libfdt.h>
#include <tegrabl_addressmap.h>

#define DPAUX_TIMEOUT_MS 1000
#define DPAUX_RETRY_WAIT_TIME_US 100
#define DPAUX_TIMEOUT_MAX_TRIES 2
#define DPAUX_DEFER_MAX_TRIES 7
#define DPAUX_MAX_BYTES 16
#define DP_DPCP_RETRY_WAIT_TIME_US 1

#define HPD_UNPLUG_MIN_US  2000
#define HPD_PLUG_MIN_US	250

#define SHIFT 0x2 /*Offset is in word's size so multiplying offset by 4*/

#define dpaux_writel(hdpaux, reg, val) \
	NV_WRITE32(hdpaux->base + (DPAUX_##reg##_0 << SHIFT), val)

#define dpaux_readl(hdpaux, val) \
	NV_READ32(hdpaux->base + (DPAUX_##val##_0 << SHIFT))

/**
* @brief DPAUX Pad modes
*/
typedef uint32_t dpaux_pad_mode_t;
#define DPAUX_PAD_MODE_I2C 0
#define DPAUX_PAD_MODE_AUX 1

/**
* @brief DPAUX Signal voltage level
*/
typedef uint32_t dpaux_sig_volt_t;
#define DPAUX_SIG_VOLT_3V3 0
#define DPAUX_SIG_VOLT_1V8 1

static tegrabl_error_t dpaux_init(dpaux_instance_t instance, struct tegrabl_dpaux **phdpaux)
{
	struct tegrabl_dpaux *hdpaux;
	struct dpaux_soc_info *hdpaux_info = NULL;
	static bool is_initialized;
	static bool is_dpaux_initialized[4];
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);

	*phdpaux = NULL;

	if (instance >= DPAUX_MAX) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	hdpaux = tegrabl_malloc(sizeof(struct tegrabl_dpaux));
	if (hdpaux == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	hdpaux->instance = instance;

	dpaux_get_soc_info(&hdpaux_info);

	hdpaux->base = (void *)(uintptr_t)(hdpaux_info[instance].base_addr);
	hdpaux->module = hdpaux_info[instance].module;

	if (is_initialized != true) {
		/* enable host1x and sor safe clock. this is required for dpaux */
		err = tegrabl_car_rst_set(TEGRABL_MODULE_HOST1X, 0);
		if (err != TEGRABL_NO_ERROR)
			goto fail;

		err = tegrabl_car_clk_enable(TEGRABL_MODULE_HOST1X, 0, NULL);
		if (err != TEGRABL_NO_ERROR)
			goto fail;

		err = tegrabl_car_rst_clear(TEGRABL_MODULE_HOST1X, 0);
		if (err != TEGRABL_NO_ERROR)
			goto fail;

		err = tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_SAFE, 0, NULL);
		if (err != TEGRABL_NO_ERROR)
			goto fail;
	}

	pr_debug("%s: set clock for DPAUX instance = %d\n", __func__, instance);
	if (is_dpaux_initialized[instance] != true) {
		/* Reset and Enable clock based on Instance */
		err = tegrabl_car_rst_set(hdpaux->module, 0);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		err = tegrabl_car_clk_enable(hdpaux->module, 0, NULL);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		err = tegrabl_car_rst_clear(hdpaux->module, 0);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	*phdpaux = hdpaux;
	is_initialized = true;
	is_dpaux_initialized[instance] = true;
fail:
	return err;
}

static void dpaux_config_pad_mode(struct tegrabl_dpaux *hdpaux,
	dpaux_pad_mode_t mode)
{
	uint32_t val = 0;

	pr_debug("%s: entry\n", __func__);

	val = dpaux_readl(hdpaux, HYBRID_PADCTL);

	/* If it is I2C mode, enable I2C clock and data
		Otherwise disable */
	if (mode == DPAUX_PAD_MODE_I2C) {
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_PADCTL, MODE, I2C, val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_PADCTL, I2C_SDA_INPUT_RCV,
				ENABLE, val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_PADCTL, I2C_SCL_INPUT_RCV,
				ENABLE, val);
	} else {
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_PADCTL, MODE, AUX, val);

		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_PADCTL, I2C_SDA_INPUT_RCV,
				DISABLE, val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_PADCTL, I2C_SCL_INPUT_RCV,
				DISABLE, val);
	}

	dpaux_writel(hdpaux, HYBRID_PADCTL, val);

	return;
}

tegrabl_error_t dpaux_prod_set(struct tegrabl_dpaux *hdpaux, uint32_t mode)
{
	void *fdt = NULL;
	int32_t prod_offset;
	int32_t dpaux_off;
	int32_t off = 0, mask = 0, val = 0;
	int32_t temp_val;
	int32_t propval;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t dpaux_prod_tuple[4] = {0};
	uint32_t count;

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bl-dtb handle\n");
		goto fail;
	}

	propval = fdt32_to_cpu(hdpaux->instance);
	dpaux_off = fdt_node_offset_by_prop_value(fdt, -1, "nvidia,dpaux-ctrlnum", (void *)&propval, 4);
	if (dpaux_off < 0) {
		pr_error("dpaux node not found for instance = %d\n", hdpaux->instance);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	/* get prod-settings offset of dp*/
	prod_offset = fdt_subnode_offset(fdt, dpaux_off, "prod-settings");
	if (prod_offset < 0) {
		pr_error("prod-settings subnode not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
		goto fail;
	}

	if (mode == DPAUX_PAD_MODE_I2C) {
		prod_offset = fdt_subnode_offset(fdt, prod_offset, "prod_c_dpaux_hdmi");
		if (prod_offset < 0) {
			pr_error("prod_c_dpaux_hdmi subnode not found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);
			goto fail;
		}
	} else if (mode == DPAUX_PAD_MODE_AUX) {
		prod_offset = fdt_subnode_offset(fdt, prod_offset, "prod_c_dpaux_dp");
		if (prod_offset < 0) {
			pr_error("prod_c_dpaux_dp subnode not found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 3);
			goto fail;
		}
	} else {
		pr_error("%s: invalid dpaux mode\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 13);
		goto fail;
	}

	err = tegrabl_dt_get_prop_u32_array(fdt, prod_offset, "prod", 0, dpaux_prod_tuple, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error in reading dpaux \"prod\" property\n", __func__);
		goto fail;
	}

#if defined(IS_T186)
	off = dpaux_prod_tuple[0];
	mask = dpaux_prod_tuple[1];
	val = dpaux_prod_tuple[2];
#else
	off = dpaux_prod_tuple[1];
	mask = dpaux_prod_tuple[2];
	val = dpaux_prod_tuple[3];
#endif
	if ((off < 0) || (off > (NV_ADDRESS_MAP_DPAUX_SIZE - *(int32_t *)hdpaux->base))) {
		pr_error("dpaux address offset is out of bounds\n");
		goto fail;
	}
	pr_debug("dpaux prod settings: addr = %#x, mask = %#x, val = %#x\n", off, mask, val);

	temp_val = NV_READ32(hdpaux->base + off);
	temp_val = ((temp_val & ~mask) | (val & mask));
	NV_WRITE32(hdpaux->base + off, temp_val);

fail:
	return err;
}

void dpaux_pad_power(struct tegrabl_dpaux *hdpaux, bool is_on)
{
	uint32_t val = 0;

	pr_debug("%s: entry\n", __func__);
	val = dpaux_readl(hdpaux, HYBRID_SPARE);
	if (is_on)
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_SPARE, PAD_PWR, POWERUP, val);
	else
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_SPARE, PAD_PWR, POWERDOWN, val);

	dpaux_writel(hdpaux, HYBRID_SPARE, val);
	return;
}

static void dpaux_set_voltage(struct tegrabl_dpaux *hdpaux,
	dpaux_sig_volt_t vol)
{
	uint32_t val = 0;

	pr_debug("%s: entry\n", __func__);
	val = dpaux_readl(hdpaux, HYBRID_SPARE);

	if (vol == DPAUX_SIG_VOLT_3V3)
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_SPARE, RCV_33_18_SEL, SEL_3_3V,
				val);
	else
		val = NV_FLD_SET_DRF_DEF(DPAUX, HYBRID_SPARE, RCV_33_18_SEL, SEL_1_8V,
				val);

	dpaux_writel(hdpaux, HYBRID_SPARE, val);
	return;
}


tegrabl_error_t tegrabl_dpaux_init_ddc_i2c(dpaux_instance_t instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_dpaux *hdpaux;

	pr_debug("%s: entry\n", __func__);

	/* Enable DPAUX and get the dpaux handle */
	err = dpaux_init(instance, &hdpaux);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = dpaux_prod_set(hdpaux, DPAUX_PAD_MODE_I2C);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* set in I2C mode */
	dpaux_config_pad_mode(hdpaux, DPAUX_PAD_MODE_I2C);

	/* enable pad power */
	dpaux_pad_power(hdpaux, true);

	/* set voltage level to 3.3 */
	dpaux_set_voltage(hdpaux, DPAUX_SIG_VOLT_3V3);

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error = 0x%08x\n", __func__, err);
	}
	return err;
}

#if defined(CONFIG_ENABLE_DP)
void dpaux_int_disable(struct tegrabl_dpaux *hdpaux)
{
	uint32_t val;

	/* enable interrupts */
	val = NV_DRF_DEF(DPAUX, INTR_EN_AUX, PLUG_EVENT, DISABLED) |
			NV_DRF_DEF(DPAUX, INTR_EN_AUX, UNPLUG_EVENT, DISABLED) |
			NV_DRF_DEF(DPAUX, INTR_EN_AUX, IRQ_EVENT, DISABLED) |
			NV_DRF_DEF(DPAUX, INTR_EN_AUX, AUX_DONE, DISABLED);

	dpaux_writel(hdpaux, INTR_EN_AUX, val);

	val = NV_DRF_DEF(DPAUX, INTR_AUX, PLUG_EVENT, PENDING) |
			NV_DRF_DEF(DPAUX, INTR_AUX, UNPLUG_EVENT, PENDING) |
			NV_DRF_DEF(DPAUX, INTR_AUX, IRQ_EVENT, PENDING) |
			NV_DRF_DEF(DPAUX, INTR_AUX, AUX_DONE, PENDING);
	dpaux_writel(hdpaux, INTR_AUX, val);

	return;
}

void dpaux_int_enable(struct tegrabl_dpaux *hdpaux)
{
	uint32_t val;

	/* clear pending interrupts */
	val = NV_DRF_DEF(DPAUX, INTR_AUX, PLUG_EVENT, PENDING) |
			NV_DRF_DEF(DPAUX, INTR_AUX, UNPLUG_EVENT, PENDING) |
			NV_DRF_DEF(DPAUX, INTR_AUX, IRQ_EVENT, PENDING) |
			NV_DRF_DEF(DPAUX, INTR_AUX, AUX_DONE, PENDING);
	dpaux_writel(hdpaux, INTR_AUX, val);

	/* disable interrupts */
	val = NV_DRF_DEF(DPAUX, INTR_EN_AUX, PLUG_EVENT, ENABLED) |
			NV_DRF_DEF(DPAUX, INTR_EN_AUX, UNPLUG_EVENT, ENABLED) |
			NV_DRF_DEF(DPAUX, INTR_EN_AUX, IRQ_EVENT, ENABLED) |
			NV_DRF_DEF(DPAUX, INTR_EN_AUX, AUX_DONE, ENABLED);

	dpaux_writel(hdpaux, INTR_EN_AUX, val);

	return;
}


void dpaux_hpd_config(struct tegrabl_dpaux *hdpaux)
{
	uint32_t val;

	val = NV_DRF_NUM(DPAUX, HPD_CONFIG, UNPLUG_MIN_TIME, HPD_UNPLUG_MIN_US) |
			NV_DRF_NUM(DPAUX, HPD_CONFIG, PLUG_MIN_TIME, HPD_PLUG_MIN_US);

	dpaux_writel(hdpaux, HPD_CONFIG, val);
	return;
}

tegrabl_error_t tegrabl_dpaux_init_aux(dpaux_instance_t instance,
	struct tegrabl_dpaux **phdpaux)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_dpaux *hdpaux;

	pr_debug("%s: entry\n", __func__);

	/* Enable DPAUX and get the dpaux handle */
	err = dpaux_init(instance, &hdpaux);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* disable all interrupts */
	dpaux_int_disable(hdpaux);

	err = dpaux_prod_set(hdpaux, DPAUX_PAD_MODE_AUX);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* set in Aux mode */
	dpaux_config_pad_mode(hdpaux, DPAUX_PAD_MODE_AUX);

	/* enable pad power */
	dpaux_pad_power(hdpaux, true);

	/* set voltage level to 1.8 */
	dpaux_set_voltage(hdpaux, DPAUX_SIG_VOLT_1V8);

	/* configure hpd with plug and unplug min times */
	dpaux_hpd_config(hdpaux);

	*phdpaux = hdpaux;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error = 0x%08x\n", __func__, err);
	}
	return err;
}

tegrabl_error_t tegrabl_dpaux_write_chunk(struct tegrabl_dpaux *hdpaux,
	uint32_t cmd, uint32_t addr, uint8_t *data, uint32_t *size,
	uint32_t *aux_stat)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t val = 0;
	uint32_t req;
	uint32_t timeout_retries = DPAUX_TIMEOUT_MAX_TRIES;
	uint32_t defer_retries   = DPAUX_DEFER_MAX_TRIES;
	time_t start_time, curr_time;
	bool hpd_status;
	uint32_t temp[4] = {0, 0, 0, 0};

	pr_debug("%s: entry\n", __func__);
	if ((cmd != AUX_CMD_I2CWR) && (cmd != AUX_CMD_MOTWR) &&
		(cmd != AUX_CMD_AUXWR)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	if (*size > DPAUX_MAX_BYTES) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	memcpy(temp, data, *size);

	val = dpaux_readl(hdpaux, DP_AUXCTL);

	val = NV_FLD_SET_DRF_NUM(DPAUX, DP_AUXCTL, CMD, cmd, val);
	if (*size == 0) {
		val = NV_FLD_SET_DRF_NUM(DPAUX, DP_AUXCTL, CMDLEN, 0, val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, DP_AUXCTL, ADDRESS_ONLY, YES, val);
	} else {
		val = NV_FLD_SET_DRF_NUM(DPAUX, DP_AUXCTL, CMDLEN, (*size - 1), val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, DP_AUXCTL, ADDRESS_ONLY, NO, val);
	}
	dpaux_writel(hdpaux, DP_AUXCTL, val);

	dpaux_writel(hdpaux, DP_AUXADDR, addr);

	/* write the data */
	dpaux_writel(hdpaux, DP_AUXDATA_WRITE_W0, temp[0]);
	dpaux_writel(hdpaux, DP_AUXDATA_WRITE_W1, temp[1]);
	dpaux_writel(hdpaux, DP_AUXDATA_WRITE_W2, temp[2]);
	dpaux_writel(hdpaux, DP_AUXDATA_WRITE_W3, temp[3]);

	/* check connection status */
	err = tegrabl_dpaux_hpd_status(hdpaux, &hpd_status);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	if (hpd_status != true) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_CONNECTED, 0);
		goto fail;
	}

	/* do transfer and if any error retry in next interation */
	while (1) {
		if ((timeout_retries != DPAUX_TIMEOUT_MAX_TRIES) ||
			(defer_retries != DPAUX_DEFER_MAX_TRIES))
			tegrabl_udelay(DP_DPCP_RETRY_WAIT_TIME_US);

		val = dpaux_readl(hdpaux, DP_AUXCTL);
		val = NV_FLD_SET_DRF_DEF(DPAUX, DP_AUXCTL, TRANSACTREQ, PENDING, val);
		dpaux_writel(hdpaux, DP_AUXCTL, val);

		/* wait for the trans act req */
		start_time = tegrabl_get_timestamp_ms();
		val = dpaux_readl(hdpaux, DP_AUXCTL);
		req = NV_DRF_VAL(DPAUX, DP_AUXCTL, TRANSACTREQ, val);
		while (req & DPAUX_DP_AUXCTL_TRANSACTREQ_PENDING) {
			curr_time = tegrabl_get_timestamp_ms();
			if (curr_time - start_time > DPAUX_TIMEOUT_MS) {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
				goto fail;
			}
			tegrabl_udelay(DPAUX_RETRY_WAIT_TIME_US);
			val = dpaux_readl(hdpaux, DP_AUXCTL);
			req = NV_DRF_VAL(DPAUX, DP_AUXCTL, TRANSACTREQ, val);
		}

		/* check if any errors */
		val = dpaux_readl(hdpaux, DP_AUXSTAT);

		if ((NV_DRF_VAL(DPAUX, DP_AUXSTAT, TIMEOUT_ERROR, val) ==
			 DPAUX_DP_AUXSTAT_0_TIMEOUT_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, RX_ERROR, val) ==
				DPAUX_DP_AUXSTAT_0_RX_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, SINKSTAT_ERROR, val) ==
				 DPAUX_DP_AUXSTAT_0_SINKSTAT_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, NO_STOP_ERROR, val) ==
				DPAUX_DP_AUXSTAT_0_NO_STOP_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_NACK) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_I2CNACK)) {

			if (timeout_retries-- > 0) {
				pr_debug("%s: defer retry = %d\n", __func__, timeout_retries);
				dpaux_writel(hdpaux, DP_AUXSTAT, val);
				continue;
			} else {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 1);
				goto fail;
			}
		}

		/* check if any defer errors */
		if ((NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_DEFER) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_I2CDEFER)) {
			if (defer_retries-- > 0) {
				pr_debug("%s: defer retry = %d\n", __func__, defer_retries);
				dpaux_writel(hdpaux, DP_AUXSTAT, val);
				continue;
			} else {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 2);
				goto fail;
			}
		}

		/* check if any ack, if yes then done */
		if (NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_ACK) {
			(*size)++;
			break;
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 3);
			goto fail;
		}
	}
	*aux_stat = val;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error = 0x%08x\n", __func__, err);
	}
	return err;
}

tegrabl_error_t tegrabl_dpaux_read_chunk(struct tegrabl_dpaux *hdpaux,
	uint32_t cmd, uint32_t addr, uint8_t *data, uint32_t *size,
	uint32_t *aux_stat)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t val = 0;
	uint32_t req;
	uint32_t timeout_retries = DPAUX_TIMEOUT_MAX_TRIES;
	uint32_t defer_retries  = DPAUX_DEFER_MAX_TRIES;
	time_t start_time, curr_time;
	uint32_t temp[4];
	bool hpd_status;

	pr_debug("%s: entry\n", __func__);

	if ((cmd != AUX_CMD_I2CRD) && (cmd != AUX_CMD_MOTRD) &&
		(cmd != AUX_CMD_AUXRD)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	if (*size > DPAUX_MAX_BYTES) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	val = dpaux_readl(hdpaux, DP_AUXCTL);

	val = NV_FLD_SET_DRF_NUM(DPAUX, DP_AUXCTL, CMD, cmd, val);
	if (*size == 0) {
		val = NV_FLD_SET_DRF_NUM(DPAUX, DP_AUXCTL, CMDLEN, 0, val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, DP_AUXCTL, ADDRESS_ONLY, YES, val);
	} else {
		val = NV_FLD_SET_DRF_NUM(DPAUX, DP_AUXCTL, CMDLEN, (*size - 1), val);
		val = NV_FLD_SET_DRF_DEF(DPAUX, DP_AUXCTL, ADDRESS_ONLY, NO, val);
	}
	dpaux_writel(hdpaux, DP_AUXCTL, val);

	dpaux_writel(hdpaux, DP_AUXADDR, addr);

	/* check connnection status */
	err = tegrabl_dpaux_hpd_status(hdpaux, &hpd_status);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	if (hpd_status != true) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_CONNECTED, 1);
		goto fail;
	}

	/* do transfer and if any error retry in next interation */
	while (1) {
		pr_debug("%s: retry\n", __func__);
		if ((timeout_retries != DPAUX_TIMEOUT_MAX_TRIES) ||
			(defer_retries != DPAUX_DEFER_MAX_TRIES))
			tegrabl_udelay(DP_DPCP_RETRY_WAIT_TIME_US);

		val = dpaux_readl(hdpaux, DP_AUXCTL);
		val = NV_FLD_SET_DRF_DEF(DPAUX, DP_AUXCTL, TRANSACTREQ, PENDING, val);
		dpaux_writel(hdpaux, DP_AUXCTL, val);

		/* wait for the trans act req */
		start_time = tegrabl_get_timestamp_ms();
		val = dpaux_readl(hdpaux, DP_AUXCTL);
		req = NV_DRF_VAL(DPAUX, DP_AUXCTL, TRANSACTREQ, val);
		while (req & DPAUX_DP_AUXCTL_TRANSACTREQ_PENDING) {
			pr_debug("%s: wait on transactreq\n", __func__);
			curr_time = tegrabl_get_timestamp_ms();
			if (curr_time - start_time > DPAUX_TIMEOUT_MS) {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 4);
				goto fail;
			}
			tegrabl_udelay(DPAUX_RETRY_WAIT_TIME_US);
			val = dpaux_readl(hdpaux, DP_AUXCTL);
			req = NV_DRF_VAL(DPAUX, DP_AUXCTL, TRANSACTREQ, val);
		}

		val = dpaux_readl(hdpaux, DP_AUXSTAT);

		/* check if any errors */
		if ((NV_DRF_VAL(DPAUX, DP_AUXSTAT, TIMEOUT_ERROR, val) ==
			 DPAUX_DP_AUXSTAT_0_TIMEOUT_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, RX_ERROR, val) ==
				DPAUX_DP_AUXSTAT_0_RX_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, SINKSTAT_ERROR, val) ==
				 DPAUX_DP_AUXSTAT_0_SINKSTAT_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, NO_STOP_ERROR, val) ==
				DPAUX_DP_AUXSTAT_0_NO_STOP_ERROR_PENDING) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_NACK) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_I2CNACK)) {

			if (timeout_retries-- > 0) {
				pr_debug("%s: timeout retry = %d\n", __func__, timeout_retries);
				dpaux_writel(hdpaux, DP_AUXSTAT, val);
				continue;
			} else {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 5);
				goto fail;
			}
		}

		/* check if any defer errors */
		if ((NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_DEFER) ||
			(NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_I2CDEFER)) {
			if (defer_retries-- > 0) {
				pr_debug("%s: defer retry = %d\n", __func__, defer_retries);
				dpaux_writel(hdpaux, DP_AUXSTAT, val);
				continue;
			} else {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 6);
				goto fail;
			}
		}

		/* check if any ack, if yes then done */
		if (NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLYTYPE, val) ==
				DPAUX_DP_AUXSTAT_0_REPLYTYPE_ACK) {
			/* read the data */
			temp[0] = dpaux_readl(hdpaux, DP_AUXDATA_READ_W0);
			temp[1] = dpaux_readl(hdpaux, DP_AUXDATA_READ_W1);
			temp[2] = dpaux_readl(hdpaux, DP_AUXDATA_READ_W2);
			temp[3] = dpaux_readl(hdpaux, DP_AUXDATA_READ_W3);

			*size = NV_DRF_VAL(DPAUX, DP_AUXSTAT, REPLY_M, val);
			memcpy(data, temp, *size);
			break;
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 7);
			goto fail;
		}
	}
	*aux_stat = val;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error = 0x%08x\n", __func__, err);
	}
	return err;
}

tegrabl_error_t tegrabl_dpaux_write(struct tegrabl_dpaux *hdpaux, uint32_t cmd,
	uint32_t addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat)
{
	uint32_t cur_size = 0;
	uint32_t finished = 0;
	tegrabl_error_t err;

	pr_debug("%s: entry\n", __func__);

	if ((hdpaux == NULL) || (data == NULL) || (size == NULL) ||
			(aux_stat == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		goto fail;
	}

	if (*size == 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
		goto fail;
	}

	do {
		cur_size = *size - finished;
		if (cur_size > DPAUX_MAX_BYTES) {
			cur_size = DPAUX_MAX_BYTES;
		}

		err = tegrabl_dpaux_write_chunk(hdpaux, cmd, addr, data, &cur_size,
				aux_stat);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}

		finished += cur_size;
		addr += cur_size;
		data += cur_size;

	} while (*size > finished);

	*size = finished;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error = 0x%08x\n", __func__, err);
	}
	return err;
}

tegrabl_error_t tegrabl_dpaux_read(struct tegrabl_dpaux *hdpaux, uint32_t cmd,
	uint32_t addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat)
{
	uint32_t cur_size = 0;
	uint32_t finished = 0;
	tegrabl_error_t err;

	pr_debug("%s: entry\n", __func__);

	if ((hdpaux == NULL) || (data == NULL) || (size == NULL) ||
			(aux_stat == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}
	if (*size == 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

	do {
		cur_size = *size - finished;
		if (cur_size > DPAUX_MAX_BYTES) {
			cur_size = DPAUX_MAX_BYTES;
		}

		err = tegrabl_dpaux_read_chunk(hdpaux, cmd, addr, data, &cur_size,
				aux_stat);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		pr_debug("cur size = %d\n", cur_size);
		finished += cur_size;
		addr += cur_size;
		data += cur_size;

	} while (*size > finished);

	*size = finished;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: error = 0x%08x\n", __func__, err);
	}
	return err;
}

tegrabl_error_t tegrabl_dpaux_hpd_status(struct tegrabl_dpaux *hdpaux,
	bool *hpd_status)
{
	uint32_t val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((hdpaux == NULL) || (hpd_status == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 9);
		goto fail;
	}

	/* check hpd status */
	val = dpaux_readl(hdpaux, DP_AUXSTAT);
	*hpd_status = NV_DRF_VAL(DPAUX, DP_AUXSTAT, HPD_STATUS, val);

	pr_debug("hpd status = %d\n", *hpd_status);

fail:
	return err;
}

tegrabl_error_t tegrabl_dpaux_i2c_write(struct tegrabl_dpaux *hdpaux,
	uint16_t slave_addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t cur_size = 0;
	uint32_t finished = 0;

	if ((hdpaux == NULL) || (data == NULL) || (size == NULL) ||
		(*size == 0) || (aux_stat == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 10);
		goto fail;
	}

	pr_debug("%s: MOTWR, slave_addr = %x, size = %d\n",
			 __func__, slave_addr, *size);
	do {
		cur_size = *size - finished;
		if (cur_size > DPAUX_MAX_BYTES) {
			cur_size = DPAUX_MAX_BYTES;
		}

		err = tegrabl_dpaux_write_chunk(hdpaux, AUX_CMD_MOTWR, slave_addr, data,
										&cur_size, aux_stat);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		pr_debug("cur size = %d\n", cur_size);
		finished += cur_size;
		data += cur_size;

	} while (*size > finished);

	*size += cur_size;

fail:
	return err;
}

tegrabl_error_t tegrabl_dpaux_i2c_read(struct tegrabl_dpaux *hdpaux,
	uint16_t slave_addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t cur_size;
	uint32_t finished = 0;

	if ((hdpaux == NULL) || (data == NULL) || (size == NULL) ||
		(*size == 0) || (aux_stat == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 11);
		goto fail;
	}

	do {
		cur_size = *size - finished;
		if (cur_size > DPAUX_MAX_BYTES) {
			cur_size = DPAUX_MAX_BYTES;
		}

		pr_debug("%s: MOTRD, slave_addr = %x, size = %d\n",
				 __func__, slave_addr, cur_size);
		err = tegrabl_dpaux_read_chunk(hdpaux, AUX_CMD_MOTRD, slave_addr, data,
									   &cur_size, aux_stat);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		pr_debug("cur size = %d\n", cur_size);
		finished += cur_size;
		data += cur_size;

	} while (*size > finished);

	*size += cur_size;

	cur_size = 0;
	pr_debug("%s: I2CRD, slave_addr = %x, size = %d\n",
			 __func__, slave_addr, cur_size);
	err = tegrabl_dpaux_read_chunk(hdpaux, AUX_CMD_I2CRD, slave_addr, data,
								   &cur_size, aux_stat);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	*size += cur_size;

fail:
	return err;
}

tegrabl_error_t tegrabl_dpaux_i2c_transactions(struct tegrabl_dpaux *hdpaux,
	struct tegrabl_i2c_transaction *trans, uint32_t num)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_i2c_transaction *pmsg;
	uint32_t i;
	uint32_t aux_stat;
	uint32_t len = 0;

	if ((hdpaux == NULL) || (trans == NULL) || (num == 0)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		goto fail;
	}

	for (i = 0; i < num; i++) {
		pmsg = &trans[i];

		if (pmsg->is_write) {
			len = pmsg->len;

			err = tegrabl_dpaux_i2c_write(hdpaux, pmsg->slave_addr,
										  pmsg->buf, &len, &aux_stat);
			if (err != TEGRABL_NO_ERROR) {
				pr_debug("%s: i2c write error addr:%d, size:%d, stat:0x%x\n",
						 __func__, pmsg->slave_addr, len, aux_stat);
				goto fail;
			}
		} else {
			len = pmsg->len;

			err = tegrabl_dpaux_i2c_read(hdpaux, pmsg->slave_addr,
										 pmsg->buf, &len, &aux_stat);
			if (err != TEGRABL_NO_ERROR) {
				pr_debug("%s: i2c read error addr:%d, size:%d, stat:0x%x\n",
						 __func__, pmsg->slave_addr, len, aux_stat);
				goto fail;
			}
		}
	}

fail:
	return err;
}
#endif
