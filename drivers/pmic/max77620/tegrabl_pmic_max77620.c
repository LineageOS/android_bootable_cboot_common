/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PMIC

/* TODO
 * 1. Error handling / returns
 * 2. ISR/ Event handlers
 */

#include <string.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_i2c.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_regulator.h>
#include <tegrabl_pmic.h>
#include <tegrabl_gpio.h>
#include <tegrabl_malloc.h>
#include <tegrabl_devicetree.h>
#include <libfdt.h>
#include "tegrabl_max77620_priv.h"

struct tegrabl_pmic_config max77620_config;

static int8_t gpio_reg[] = {
	MAX77620_GPIO0_REG,
	MAX77620_GPIO1_REG,
	MAX77620_GPIO2_REG,
	MAX77620_GPIO3_REG,
	MAX77620_GPIO4_REG,
	MAX77620_GPIO5_REG,
	MAX77620_GPIO6_REG,
	MAX77620_GPIO7_REG
};

typedef struct {
	tegrabl_error_t (*set_voltage)(int32_t phandle, uint32_t volts);
	tegrabl_error_t (*enable)(int32_t phandle);
	tegrabl_error_t (*disable)(int32_t phandle);
} max77620_ops;

/* macro max77620 regulator type */
typedef uint32_t max77620_regulator_type_t;
#define MAX77620_REGULATOR_TYPE_SD 0
#define MAX77620_REGULATOR_TYPE_LDO_N 1
#define MAX77620_REGULATOR_TYPE_LDO_P 2

typedef struct {
	/* regulator phandle */
	int32_t phandle;
	/* node name */
	char *name;
	/* rehulator type */
	max77620_regulator_type_t regulator_type;
	/* active-discharge prop */
	bool act_dischrg_en;
	bool act_dischrg_dis;
	/* regulator register */
	uint32_t reg;
	uint32_t cfg_reg;
	/* steps in micro volts */
	uint32_t step_volts;
	/* limitations in micro volts */
	uint32_t min_volts;
	uint32_t max_volts;
	/* regulator operations */
	max77620_ops *ops;
} max77620_regulator_props;

/* global fdt handle */
static void *fdt;
/* max77620 node offset */
static int internal_node_offset;
/* i2c dev handle */
static struct tegrabl_i2c_dev *hi2c_dev;
/* max77620 gpio driver */
static struct gpio_driver *drv;
/* max77620 pmic handle */
static tegrabl_pmic_t *pmic_p;

static tegrabl_error_t set_sd_voltage(int32_t phandle, uint32_t volts);
static tegrabl_error_t enable_sd(int32_t phandle);
static tegrabl_error_t disable_sd(int32_t phandle);
static tegrabl_error_t set_ldo_voltage(int32_t phandle, uint32_t volts);
static tegrabl_error_t enable_ldo(int32_t phandle);
static tegrabl_error_t disable_ldo(int32_t phandle);

/* step down regulator operations */
static max77620_ops sd_ops = {
	set_sd_voltage,
	enable_sd,
	disable_sd,
};

/* linear regulator operations */
static max77620_ops ldo_ops = {
	set_ldo_voltage,
	enable_ldo,
	disable_ldo,
};

static max77620_regulator_props s_props[] = {
#if defined(CONFIG_ENABLE_PMIC_MAX20024)
	REG_DATA_MAX20024("sd0", SD0, SD, 12500, 800000, 1587500, &sd_ops),
	REG_DATA_MAX20024("sd1", SD1, SD, 12500, 600000, 3787500, &sd_ops),
	REG_DATA_MAX20024("sd2", SD2, SD, 12500, 600000, 3787500, &sd_ops),
	REG_DATA_MAX20024("sd3", SD3, SD, 12500, 600000, 3787500, &sd_ops),
	REG_DATA_MAX20024("sd4", SD4, SD, 12500, 600000, 3787500, &sd_ops),
	REG_DATA_MAX20024("ldo0", LDO0, LDO_N, 25000, 800000, 2375000, &ldo_ops),
	REG_DATA_MAX20024("ldo1", LDO1, LDO_N, 25000, 800000, 2375000, &ldo_ops),
	REG_DATA_MAX20024("ldo2", LDO2, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA_MAX20024("ldo3", LDO3, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA_MAX20024("ldo4", LDO4, LDO_P, 12500, 800000, 1587500, &ldo_ops),
	REG_DATA_MAX20024("ldo5", LDO5, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA_MAX20024("ldo6", LDO6, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA_MAX20024("ldo7", LDO7, LDO_N, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA_MAX20024("ldo8", LDO8, LDO_N, 50000, 800000, 3950000, &ldo_ops),
#else
	REG_DATA("sd0", SD0, SD, 12500, 600000, 1400000, &sd_ops),
	REG_DATA("sd1", SD1, SD, 12500, 600000, 1600000, &sd_ops),
	REG_DATA("sd2", SD2, SD, 12500, 600000, 3787500, &sd_ops),
	REG_DATA("sd3", SD3, SD, 12500, 600000, 3787500, &sd_ops),
	REG_DATA("ldo0", LDO0, LDO_N, 25000, 800000, 2375000, &ldo_ops),
	REG_DATA("ldo1", LDO1, LDO_N, 25000, 800000, 2375000, &ldo_ops),
	REG_DATA("ldo2", LDO2, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA("ldo3", LDO3, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA("ldo4", LDO4, LDO_P, 12500, 800000, 1587500, &ldo_ops),
	REG_DATA("ldo5", LDO5, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA("ldo6", LDO6, LDO_P, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA("ldo7", LDO7, LDO_N, 50000, 800000, 3950000, &ldo_ops),
	REG_DATA("ldo8", LDO8, LDO_N, 50000, 800000, 3950000, &ldo_ops),
#endif

};

static tegrabl_error_t enable_sd_regulator(int32_t phandle, bool is_enable)
{
	/*TBD*/
	TEGRABL_UNUSED(phandle);
	TEGRABL_UNUSED(is_enable);
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t enable_ldo_regulator(int32_t phandle, bool is_enable)
{
	/*TBD*/
	TEGRABL_UNUSED(phandle);
	TEGRABL_UNUSED(is_enable);
	return TEGRABL_NO_ERROR;
}
static tegrabl_error_t enable_sd(int32_t phandle)
{
	return enable_sd_regulator(phandle, true);
}

static tegrabl_error_t disable_sd(int32_t phandle)
{
	return enable_sd_regulator(phandle, false);
}

static tegrabl_error_t enable_ldo(int32_t phandle)
{
	return enable_ldo_regulator(phandle, true);
}

static tegrabl_error_t disable_ldo(int32_t phandle)
{
	return enable_ldo_regulator(phandle, false);
}

static tegrabl_error_t set_regulator_voltage(int32_t phandle, uint32_t volts)
{
	uint8_t i = 0;
	uint8_t data = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	for (i = 0; i < ARRAY_SIZE(s_props); i++) {
		if (s_props[i].phandle == phandle) {
			break;
		}
	}

	if (i == ARRAY_SIZE(s_props)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	if ((volts < s_props[i].min_volts) || (volts > s_props[i].max_volts))
		data = 0;
	else
		data = (volts - s_props[i].min_volts) / s_props[i].step_volts;

	/* check if its ldo - FIXME - dont hardcode */
	if (i > 3)
		data |= (3 << 6);

	if (hi2c_dev)
		err = tegrabl_i2c_dev_write(hi2c_dev, &data, s_props[i].reg, 1);
	else
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (err != TEGRABL_NO_ERROR)
		TEGRABL_SET_HIGHEST_MODULE(err);

fail:
	return err;
}

static tegrabl_error_t set_sd_voltage(int32_t phandle, uint32_t volts)
{
	return set_regulator_voltage(phandle, volts);
}

static tegrabl_error_t set_ldo_voltage(int32_t phandle, uint32_t volts)
{
	return set_regulator_voltage(phandle, volts);
}

static tegrabl_error_t max77620_gpio_config(uint32_t gpio_num,
				gpio_pin_mode_t mode, void *drv_data)
{
	/* Stub for now - Done in pinmux cfg
	 * FIXME: Implement this ?
	 */
	TEGRABL_UNUSED(gpio_num);
	TEGRABL_UNUSED(mode);
	TEGRABL_UNUSED(drv_data);
	return TEGRABL_ERR_NOT_SUPPORTED;
}

static tegrabl_error_t max77620_gpio_read(uint32_t gpio_num,
				gpio_pin_state_t *state, void *drv_data)
{
	/* Stub for now - no use case for read in any tegrabl_*
	 * FIXME: Implement this ?
	 */
	TEGRABL_UNUSED(gpio_num);
	TEGRABL_UNUSED(state);
	TEGRABL_UNUSED(drv_data);
	return TEGRABL_ERR_NOT_SUPPORTED;
}

static tegrabl_error_t max77620_gpio_write(uint32_t gpio_num,
				gpio_pin_state_t state, void *drv_data)
{
	uint8_t data = 0;
	uint8_t reg;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	TEGRABL_UNUSED(drv_data);

	/* step 1 : clear the alternate mode enable on gpio */
	reg = MAX77620_AME_GPIO;

	if (hi2c_dev)
		err = tegrabl_i2c_dev_read(hi2c_dev, &data, reg, 1);
	else
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	data = data & (~(1 << gpio_num));

	err = tegrabl_i2c_dev_write(hi2c_dev, &data, reg, 1);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* step 2: set gpio push-pull out logic high */
	data = 0x00;
	if (state)
		data = (data & 0xF0) | ((GPIO_ST & 0x1) << 3) | 1;
	else
		data = 0; /* FIXME */

	err = tegrabl_i2c_dev_write(hi2c_dev, &data, gpio_reg[gpio_num], 1);

fail:
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static struct gpio_driver_ops max77620_gpio_ops = {
	.config =  max77620_gpio_config,
	.read =  max77620_gpio_read,
	.write =  max77620_gpio_write
};

static tegrabl_error_t get_gpio_info_from_dt(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t offset = 0;
	char *ptr;

	if (!internal_node_offset) {
		err = TEGRABL_ERR_INVALID;
		goto fail;
	}

	/* get pinmux node offset */
	err = tegrabl_dt_get_child_with_name(fdt,
										 internal_node_offset,
										 "pinmux@0",
										 &offset);
	if (err != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	drv = (struct gpio_driver *)tegrabl_calloc(1, sizeof(struct gpio_driver));
	if (!drv) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	ptr = (char *)tegrabl_calloc(1, strlen(MAXPMIC_GPIO_DRIVER) + 1);
	if (!ptr) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	drv->name = ptr;
	strcpy(ptr, MAXPMIC_GPIO_DRIVER);
	/* use the max77620 phandle */
	drv->phandle = fdt_get_phandle(fdt, internal_node_offset);
	/* FIXME GPIO list is chip_id based.
	 * Keep this until phandle/DT support is available */
	drv->chip_id = drv->phandle;
	drv->ops = &max77620_gpio_ops;

	/* register max77620 gpio functionality with gpio framework */
	tegrabl_gpio_driver_register(drv);

fail:
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static tegrabl_error_t get_regulator_info_from_dt(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_regulator_t *regulator = NULL;
	int32_t regulator_offset = 0;
	int32_t sub_node_offset = 0;
	uint32_t i = 0;
	uint32_t phandle;
	const char *name = NULL;
	uint32_t min_volts = 0, max_volts = 0;

	if (!internal_node_offset) {
		err = TEGRABL_ERR_INVALID;
		goto fail;
	}

	err = tegrabl_dt_get_child_with_name(fdt,
										 internal_node_offset,
										 "regulators",
										 &regulator_offset);

	if (err != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	for (i = 0; i < (sizeof(s_props) / sizeof(s_props[0])); i++) {
		/* find each regulator with the name */
		err = tegrabl_dt_get_child_with_name(fdt,
										 regulator_offset,
										 s_props[i].name,
										 &sub_node_offset);
		if (err != TEGRABL_NO_ERROR) {
			pr_info("node NOT found in dts -%s\n", s_props[i].name);
			continue;
		} else {
			pr_debug("node found in dts - %s\n", s_props[i].name);
		}

		/* get active-discharge node property */
		if (TEGRABL_NO_ERROR == tegrabl_dt_get_prop_string(
									fdt,
									sub_node_offset,
									"regulator-enable-active-discharge",
									NULL)) {
			s_props[i].act_dischrg_en = true;
		}
		if (TEGRABL_NO_ERROR == tegrabl_dt_get_prop_string(
									fdt,
									sub_node_offset,
									"regulator-disable-active-discharge",
									NULL)) {
			s_props[i].act_dischrg_dis = true;
		}

		/* see if the node is been referenced anywhere in dts.
		   phandle is only present if a node had been referenced
		   from someplace else in the dtb */
		phandle = fdt_get_phandle(fdt, sub_node_offset);
		if (!phandle)
			continue;

		regulator = (tegrabl_regulator_t *)tegrabl_calloc(1,
											sizeof(tegrabl_regulator_t));
		if (!regulator) {
			err = TEGRABL_ERR_NO_MEMORY;
			goto fail;
		}

		/* initialize regulator properties */
		s_props[i].phandle = phandle;
		regulator->phandle = s_props[i].phandle;
		regulator->set_voltage = s_props[i].ops->set_voltage;
		/* this is programmable regulator and not fixed */
		regulator->is_fixed = false;
		regulator->is_enabled = false;

		/* get regulator name */
		if (TEGRABL_NO_ERROR != tegrabl_dt_get_prop_string(
									fdt,
									sub_node_offset,
									"regulator-name",
									&name)){
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			continue;
		}
		if (name) {
			strlcpy(regulator->name, name, sizeof(regulator->name));
		}

		/* get regulator min microvolt */
		err = tegrabl_dt_get_prop_u32(fdt, sub_node_offset,
									  "regulator-min-microvolt", &min_volts);
		if (err != TEGRABL_NO_ERROR) {
			goto skip_volt_setting;
		}

		/* get regulator max microvolt */
		err = tegrabl_dt_get_prop_u32(fdt, sub_node_offset,
									"regulator-max-microvolt", &max_volts);
		if (err != TEGRABL_NO_ERROR) {
			goto skip_volt_setting;
		}

		/* kernel dts files have min and max microvolt parameters of
		   a regulator set to same value. This is because tegra soc doesn't
		   expect the voltage to be altered */
		if (min_volts == max_volts) {
			regulator->set_volts = min_volts;
		} else {
skip_volt_setting:
			regulator->set_volts = 0;
		}

		/* register regulator */
		err = tegrabl_regulator_register(regulator);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("failed to register regulator '%s' with error %d\n",
					 s_props[i].name, err);
			goto fail;
		}
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static tegrabl_error_t max77620_poweroff(void)
{
	uint8_t data = 0;
	uint8_t reg;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	reg = MAX77620_REG_ONOFF_CFG1;

	if (hi2c_dev)
		err = tegrabl_i2c_dev_read(hi2c_dev, &data, reg, 1);
	else
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);


	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	data |=  (1 << PWR_OFF_BIT) | (1 << SW_RST_BIT);

	err = tegrabl_i2c_dev_write(hi2c_dev, &data, reg, 1);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* TODO - Add a few ms delay? to wait for PMIC to poweroff
	 * Optimize with nonreturn attribute */

fail:
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static tegrabl_error_t max77620_get_reset_reason(uint32_t *buf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!buf) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
	}

	if (hi2c_dev)
		err = tegrabl_i2c_dev_read(hi2c_dev, buf, MAX77620_REG_NVERC, 1);
	else
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static tegrabl_error_t max77620_reg8_update(
		uint8_t reg_addr,
		uint8_t mask,
		uint8_t val)
{
	uint8_t regread = 0;
	uint8_t regwrite;
	tegrabl_error_t err_status = TEGRABL_NO_ERROR;

	if (hi2c_dev)
		err_status = tegrabl_i2c_dev_read(hi2c_dev, &regread, reg_addr, 1);
	else
		err_status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (err_status != TEGRABL_NO_ERROR) {
		goto fail;
	}

	val *= ((mask) & ~((mask) << 1));
	regread = regread & (~mask);
	regwrite = (val | regread);

	if (regwrite != regread) {
		err_status = tegrabl_i2c_dev_write(hi2c_dev, &regwrite, reg_addr, 1);
		if (err_status != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
fail:
	if (err_status != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err_status);
	}

	return err_status;
}

/* set active-discharge for sd/ldo */
static tegrabl_error_t max77620_set_active_discharge(void)
{
	uint32_t i;
	uint8_t mask, val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	for (i = 0; i < (sizeof(s_props) / sizeof(s_props[0])); i++) {
		if (s_props[i].act_dischrg_en || s_props[i].act_dischrg_dis) {
			if (s_props[i].regulator_type == MAX77620_REGULATOR_TYPE_SD) {
				mask = MAX77620_REG_SD_NADE_MASK;
				val = 1;
				if (s_props[i].act_dischrg_en)
					val = 0;
			} else {
				mask = MAX77620_REG_LDO_ADE_MASK;
				val = 0;
				if (s_props[i].act_dischrg_en)
					val = 1;
			}

			if (max77620_reg8_update(s_props[i].cfg_reg, mask, val)
								!= TEGRABL_NO_ERROR)
				pr_error("Failed to set active_discharge for %s\n",
						 s_props[i].name);
		}
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to set max77620_set_active_discharge prop\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
	}
	return err;
}

static tegrabl_error_t max77620_probe(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!internal_node_offset) {
		err = TEGRABL_ERR_INVALID;
		goto fail;
	}

	err = get_regulator_info_from_dt();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("failed to get regulator info from dts\n");
		goto fail;
	}

	err = get_gpio_info_from_dt();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("failed to get gpio info from dts\n");
		goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR)
		TEGRABL_SET_HIGHEST_MODULE(err);

	return err;
}

/**
 * @brief - Registers max77620 driver routines with the pmic interface
 * @return - Error code
 */
tegrabl_error_t tegrabl_max77620_init(uint32_t i2c_instance)
{
	int32_t phandle = 0;
	uintptr_t reg;
	char *ptr = NULL;
	uint8_t slave_addr;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (NULL == fdt) {
		err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
		if (TEGRABL_NO_ERROR != err) {
			goto fail;
		}
	}

	/* Set fdt offset */
	internal_node_offset = 0;
	if (TEGRABL_NO_ERROR != tegrabl_dt_get_node_with_path(
									fdt, "/bpmp_i2c/spmic",
									&internal_node_offset)) {
		if (TEGRABL_NO_ERROR != tegrabl_dt_get_node_with_path(
									fdt, "/bpmp/i2c/spmic",
									&internal_node_offset)) {
			pr_error("Cannot find DT node for 'bpmp_i2c' (pmic max77620)\n");
			goto fail;
		}
	}


	phandle = fdt_get_phandle(fdt, internal_node_offset);

	/* register only if any references of max77620 are found */
	if (!phandle) {
		pr_info("max77620 not used. Hence not registering\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* fill pmic properties */
	pmic_p = (tegrabl_pmic_t *)tegrabl_calloc(1, sizeof(tegrabl_pmic_t));
	if (pmic_p) {
		pmic_p->phandle = phandle;
		pmic_p->i2c_instance = i2c_instance;
		ptr = &pmic_p->name[0];

		strcpy(ptr, MAXPMIC_COMPATIBLE);

		/* register poweroff only if system-pmic-power-off
		   property is present */
		if (tegrabl_dt_get_prop_u32(fdt, internal_node_offset,
									"maxim,system-pmic-power-off", NULL)) {
			pr_info("register 'maxim' power off handle\n");
			pmic_p->poweroff = max77620_poweroff;
			pmic_p->get_reset_reason = max77620_get_reset_reason;
		}

		/* get slave address */
		if (tegrabl_dt_read_reg_by_index(fdt, internal_node_offset,
										 0, &reg, NULL) != TEGRABL_NO_ERROR) {
			pr_error("unable to find slave address\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			goto fail;
		}

		/* 8 bit addressing */
		slave_addr = reg << 1;
		pr_debug("max77620 8-bit slave address 0x%x\n", slave_addr);

		/* obtain i2c_dev handle */
		hi2c_dev = tegrabl_i2c_dev_open(pmic_p->i2c_instance, slave_addr,
							 sizeof(uint8_t), sizeof(uint8_t));
		if (!hi2c_dev) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}


		/* TODO:
		 * irq and event handlers to be added here
		 */

		/* register driver */
		if (tegrabl_pmic_register(pmic_p) != TEGRABL_NO_ERROR) {
			pr_error("failed to register %s pmic\n", MAXPMIC_COMPATIBLE);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
			goto fail;
		}

		/* probe regulators */
		err = max77620_probe();
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* set active-discharge for sd/ldo */
		err = max77620_set_active_discharge();
		if (err != TEGRABL_NO_ERROR)
			goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("tegrabl_max77620_init failed!!\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
	}
	return err;
}

/**
 * @brief - Un-registers max77620 driver routines with the pmic interface
 * @return - Error code
 */
tegrabl_error_t tegrabl_max77620_un_init(void)
{
	tegrabl_error_t err;

	if (hi2c_dev)
		err = tegrabl_i2c_dev_close(hi2c_dev);
	else
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}
	return err;
}
