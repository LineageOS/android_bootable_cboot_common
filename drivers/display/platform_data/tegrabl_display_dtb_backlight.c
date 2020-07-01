/*
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DISPLAY_PDATA

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <lp8556.h>
#include <tegrabl_display_dtb_backlight.h>

#if defined(CONFIG_ENABLE_EDP)
static tegrabl_error_t get_i2c_instance(const void *fdt, int32_t node_offset,
	struct backlight_pdata *bl_pdata)
{
	int32_t i2c_nodeoffset = -1;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	i2c_nodeoffset = fdt_parent_offset(fdt, node_offset);
	if (i2c_nodeoffset <= 0) {
		pr_warn("%s: Cannot get the i2c nodeoffset\n", __func__);
		goto fail;
	}

	err = tegrabl_dt_get_prop(fdt, i2c_nodeoffset, "nvidia,hw-instance-id", 4, &bl_pdata->i2c_instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("nvidia,hw-instance-id not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

fail:
	return err;
}

static tegrabl_error_t get_pwm_instance(const void *fdt, int32_t node_offset,
	struct lp8556_platform_data *pdata)
{
	int32_t pwm_offset = -1;
	uint32_t pwm_handle;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_dt_get_prop_by_idx(fdt, node_offset, "pwms", 4, 0, &pwm_handle);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("pwm_handle not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
		goto fail;
	}

	pwm_offset = fdt_node_offset_by_phandle(fdt, pwm_handle);
	if (pwm_offset < 0) {
		pr_debug("invalid pwm node offset\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);
		goto fail;
	}

	err = tegrabl_dt_get_prop(fdt, pwm_offset, "nvidia,hw-instance-id", 4, &pdata->bl_pwm.instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("nvidia,hw-instance-id not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 3);
		goto fail;
	}

fail:
	return err;
}

static tegrabl_error_t lp8556_parse_dt(struct backlight_pdata *bl_pdata)
{
	void *fdt = NULL;
	int32_t node_offset = -1;
	struct lp8556_platform_data *pdata = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pdata = tegrabl_malloc(sizeof(struct lp8556_platform_data));
	if (pdata == NULL) {
		pr_error("lp8556: Could not allocate memory for pdata\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bl-dtb handle\n");
		goto fail;
	}

	node_offset = fdt_node_offset_by_compatible(fdt, node_offset, BACKLIGHT_LP8556);
	if (node_offset < 0) {
		pr_debug("%s: backlight node not found\n", __func__);
		goto fail;
	}

	err = tegrabl_dt_get_prop(fdt, node_offset, "init-brt", 1, &pdata->init_brt);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("init-brt not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 4);
		goto fail;
	}

	err = tegrabl_dt_get_prop(fdt, node_offset, "dev-ctrl", 1, &pdata->dev_ctrl);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("dev-ctrl not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 5);
		goto fail;
	}

	err = tegrabl_dt_get_prop(fdt, node_offset, "pwm-period", 4, &pdata->bl_pwm.freq);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("pwm-period not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 6);
		goto fail;
	}

	err = get_pwm_instance(fdt, node_offset, pdata);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("error reading pwm instance from dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 7);
		goto fail;
	}

	err = tegrabl_dt_get_prop(fdt, node_offset, "reg", 4, &bl_pdata->i2c_slave_addr);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("i2c slave addr not found in dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 8);
		goto fail;
	}
	bl_pdata->i2c_slave_addr <<= 1;

	err = get_i2c_instance(fdt, node_offset, bl_pdata);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("error reading i2c instance from dt\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 9);
		goto fail;
	}

	pr_debug("I2C instance is %d\n", bl_pdata->i2c_instance);
	pr_debug("i2c slave addr = %#x\n", bl_pdata->i2c_slave_addr);
	pr_debug("init brt = %#x\n", pdata->init_brt);
	pr_debug("dev ctrl = %#x\n", pdata->dev_ctrl);
	pr_debug("pwm period = %#x\n", pdata->bl_pwm.freq);
	pr_debug("pwm channel = %#x\n", pdata->bl_pwm.instance);

	bl_pdata->lp_pdata = pdata;

fail:
	if ((pdata != NULL) && (err != TEGRABL_NO_ERROR)) {
		tegrabl_free(pdata);
	}
	return err;
}

tegrabl_error_t tegrabl_display_panel_backlight_enable(void)
{
	struct backlight_pdata *bl_pdata = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	bl_pdata = tegrabl_malloc(sizeof(struct backlight_pdata));
	if (bl_pdata == NULL) {
		pr_error("%s: Could not allocate memory for struct lp\n", __func__);
		goto fail;
	}

	err = lp8556_parse_dt(bl_pdata);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to parse backlight dtb data\n", __func__);
		goto fail;
	}

	err = lp8556_init(bl_pdata);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if ((bl_pdata != NULL) && (err != TEGRABL_NO_ERROR)) {
		tegrabl_free(bl_pdata);
	}
	return err;
}
#endif

