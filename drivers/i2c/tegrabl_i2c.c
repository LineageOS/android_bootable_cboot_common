/*
 * Copyright (c) 2015 - 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_I2C

#include "build_config.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <list.h>
#include <tegrabl_malloc.h>
#include <tegrabl_debug.h>
#include <tegrabl_clock.h>
#include <tegrabl_i2c_local.h>
#include <tegrabl_error.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_io.h>
#include <tegrabl_timer.h>
#include <tegrabl_compiler.h>
#include <tegrabl_dpaux.h>
#include <tegrabl_i2c_bpmpfw.h>

/**
 * @brief List of controllers
 */
static struct list_node i2c_list;

#define MAX_I2C_MODES 4

struct tegrabl_i2c_prod_setting {
	uint32_t num_settings;
	uint32_t *settings;
};

static struct tegrabl_i2c_prod_setting
	i2c_prod_settings[TEGRABL_INSTANCE_I2C_MAX + 2][MAX_I2C_MODES + 1];

/**
 * @brief Defines the controller instance number and clock id
 * of particular i2c controller based on base address
 */
static uint32_t i2c_base[] = {
	NV_ADDRESS_MAP_I2C1_BASE,
	NV_ADDRESS_MAP_I2C2_BASE,
	NV_ADDRESS_MAP_I2C3_BASE,
	NV_ADDRESS_MAP_I2C4_BASE,
	NV_ADDRESS_MAP_I2C5_BASE,
	NV_ADDRESS_MAP_I2C6_BASE,
	NV_ADDRESS_MAP_I2C7_BASE,
	NV_ADDRESS_MAP_I2C8_BASE,
	NV_ADDRESS_MAP_I2C9_BASE
};


/**
 * @brief Defines the frequencies with which each controller
 * to be initialized.
 */
static uint32_t i2c_freq[TEGRABL_INSTANCE_I2C_MAX + 1] = {
	STD_SPEED,
	STD_SPEED,
	STD_SPEED,
	STD_SPEED,
	STD_SPEED,
	STD_SPEED,
	STD_SPEED,
	STD_SPEED,
	STD_SPEED
};

static TEGRABL_INLINE void i2c_writel(struct tegrabl_i2c *hi2c, uint32_t reg,
	uint32_t val)
{
	NV_WRITE32(hi2c->base + reg, val);
}

static TEGRABL_INLINE uint32_t i2c_readl(struct tegrabl_i2c *hi2c,
	uint32_t reg)
{
	return NV_READ32(hi2c->base + reg);
}

tegrabl_error_t tegrabl_i2c_register_prod_settings(uint32_t instance,
		uint32_t mode, uint32_t *settings, uint32_t num_settings)
{
	if ((instance > TEGRABL_INSTANCE_I2C_MAX) || (mode > MAX_I2C_MODES)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if ((num_settings != 0U) && (settings == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	i2c_prod_settings[instance][mode].num_settings = num_settings;
	i2c_prod_settings[instance][mode].settings = settings;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_i2c_set_bus_freq_info(uint32_t *freq, uint32_t num)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!num || !freq) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (num > ARRAY_SIZE(i2c_freq)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	while (num > 0) {
		num--;
		i2c_freq[num] = (freq[num] ? (freq[num] * KHZ) : STD_SPEED);
	}
fail:
	return error;
}

tegrabl_error_t tegrabl_i2c_register(void)
{
	list_initialize(&i2c_list);
	return TEGRABL_NO_ERROR;
}

void tegrabl_i2c_unregister_instance(enum tegrabl_instance_i2c instance)
{
	struct tegrabl_i2c *hi2c, *temp;

	list_for_every_entry_safe(&i2c_list, hi2c, temp, struct tegrabl_i2c, node) {
		if (hi2c->instance == instance) {
			list_delete(&hi2c->node);
			tegrabl_free(hi2c);
		}
	}
}

static tegrabl_error_t i2c_reset_controller(struct tegrabl_i2c *hi2c)
{
	uint32_t i2c_source_freq = 0;
	uint32_t rate_set = 0;
	uint32_t i2c_clk_divisor;
	uint32_t reg;
	uint32_t err = TEGRABL_NO_ERROR;

	pr_debug("%s: entry %d\n", __func__, hi2c->instance);

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_bpmpfw_controlled == true) {
		return TEGRABL_NO_ERROR;
	}
#endif

	if (hi2c->clk_freq < HS_SPEED) {
		if (hi2c->clk_freq > FM_SPEED) {
			i2c_clk_divisor = 0x10;
		} else if (hi2c->clk_freq <= STD_SPEED) {
			i2c_clk_divisor = 0x16;
		} else {
			i2c_clk_divisor = 0x19;
		}

		i2c_source_freq = hi2c->clk_freq * (i2c_clk_divisor + 1);
		i2c_source_freq /= 1000;
		/* if i2c_clk_divisor > 3 then Tlow + Thigh = 8
		 * else Tlow + Thigh = 9. Currently i2c_clk_divisor > 3.
		 */
		i2c_source_freq = i2c_source_freq << 3;
	} else {
		i2c_clk_divisor = 0x02;
		/* Tlow + Thigh = 13 */
		i2c_source_freq = hi2c->clk_freq * (i2c_clk_divisor + 1) * 13;
		i2c_source_freq /= 1000;
	}

	/* Assert reset to i2c */
	err = tegrabl_car_rst_set(hi2c->module_id, hi2c->instance);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_debug("Unable to assert reset to i2c instance %u\n",
			 hi2c->instance);
		goto fail;
	}

	/* Set the i2c controller clock source */
	err = tegrabl_car_set_clk_src(hi2c->module_id, hi2c->instance,
				      TEGRABL_CLK_SRC_PLLP_OUT0);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_debug("Unable to change clk source to i2c instance %u\n",
			 hi2c->instance);
		goto fail;
	}

	/* Set the i2c controller frequency */
	err = tegrabl_car_set_clk_rate(hi2c->module_id, hi2c->instance,
				       i2c_source_freq, &rate_set);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_debug("Unable to set clk rate to i2c instance %u\n",
			 hi2c->instance);
		goto fail;
	}

	/* Deassert reset to i2c */
	err = tegrabl_car_rst_clear(hi2c->module_id, hi2c->instance);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_debug("Unable to clear reset to i2c instance %u\n",
			 hi2c->instance);
		goto fail;
	}

	/* Wait for 5us delay after reset enable */
	tegrabl_udelay(5);

	reg = i2c_readl(hi2c, I2C_I2C_CLK_DIVISOR_REGISTER);
	if (hi2c->clk_freq < HS_SPEED) {
		BITFIELD_SET(reg, i2c_clk_divisor, I2C_CLK_DIVISOR_STD_FAST_MODE_WIDTH,
				I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT);
	} else {
		BITFIELD_SET(reg, i2c_clk_divisor, I2C_CLK_DIVISOR_HS_MODE_WIDTH,
				I2C_CLK_DIVISOR_HS_MODE_SHIFT);
	}
	i2c_writel(hi2c, I2C_I2C_CLK_DIVISOR_REGISTER, reg);

	pr_debug("%s: exit, i2c_source_freq = %d\n", __func__, rate_set);

fail:
	return err;
}

/**
 * @brief Transfers the register settings from shadow registers to actual
 * controller registers.
 *
 * @param hi2c i2c controller hi2c.
 *
 * @return TEGRABL_NO_ERROR if successful.
 */
static tegrabl_error_t i2c_load_config(struct tegrabl_i2c *hi2c)
{
	uint32_t reg_value = 0;
	uint32_t timeout = CNFG_LOAD_TIMEOUT_US;

	if (hi2c == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	pr_debug("Load from shadow registers to controller registers\n");

	i2c_writel(hi2c, I2C_I2C_CONFIG_LOAD_REGISTER, I2C_I2C_MSTR_CONFIG_LOAD);

	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0) {
			pr_debug("Load config timeout\n");
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
		}
		reg_value = i2c_readl(hi2c, I2C_I2C_CONFIG_LOAD_REGISTER);
	} while ((reg_value & I2C_I2C_MSTR_CONFIG_LOAD) != 0U);

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Configures the packet header and writes to
 * Tx Fifo.
 *
 * @param hi2c i2c controller hi2c
 * @param repeat_start do not send stop after sending packet
 * @param is_write prepare headers for write operation
 * @param slave_address address of slave for communication
 * @param length number of bytes in payload after header
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t i2c_send_header(struct tegrabl_i2c *hi2c,
	bool repeat_start, bool is_write, uint8_t slave_address, uint32_t length)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg_value = 0;

	pr_debug("%s: entry\n", __func__);

	i2c_writel(hi2c, I2C_I2C_CNFG_REGISTER, ENABLE_PACKET_MODE);

	reg_value = i2c_readl(hi2c, I2C_INTERRUPT_STATUS_REGISTER);
	i2c_writel(hi2c, I2C_INTERRUPT_STATUS_REGISTER, reg_value);

	reg_value = 0;
	reg_value |= I2C_IO_PACKET_HEADER_I2C_PROTOCOL;
	reg_value |= ((hi2c->instance) <<
				I2C_IO_PACKET_HEADER_CONTROLLER_ID_SHIFT);
	i2c_writel(hi2c, I2C_TX_PACKET_FIFO_REGISTER, reg_value);
	pr_debug("header 1 %08x\n", reg_value);

	if (length > 0) {
		length--;
	}

	reg_value = 0;
	reg_value = length & 0xFFF;
	i2c_writel(hi2c, I2C_TX_PACKET_FIFO_REGISTER, reg_value);
	pr_debug("header 2 %08x\n", reg_value);

	reg_value = slave_address & I2C_IO_PACKET_HEADER_SLAVE_ADDRESS_MASK;
	if (is_write == false) {
		reg_value |= I2C_IO_PACKET_HEADER_READ_MODE;
	}

	if (repeat_start) {
		reg_value |= I2C_IO_PACKET_HEADER_REPEAT_START;
	}

	if (hi2c->clk_freq >= HS_SPEED) {
		reg_value |= I2C_ENABLE_HS_MODE;
	}

	i2c_writel(hi2c, I2C_TX_PACKET_FIFO_REGISTER, reg_value);
	pr_debug("header 3 %08x\n", reg_value);

	error = i2c_load_config(hi2c);

	return error;
}

/**
 * @brief Waits till there is room in Tx Fifo or timeout.
 *
 * @param hi2c i2c controller hi2c.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t i2c_wait_for_tx_fifo_empty(struct tegrabl_i2c *hi2c)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	time_t timeout;
	uint32_t empty_slots = 0;

	pr_debug("%s: entry\n", __func__);
	timeout = hi2c->fifo_timeout;
	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0) {
			pr_debug("I2C Tx fifo empty timeout\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}
		empty_slots = i2c_readl(hi2c, I2C_FIFO_STATUS_REGISTER);
		empty_slots = empty_slots >> I2C_FIFO_STATUS_TX_FIFO_EXPTY_CNT_SHIFT;
		empty_slots = empty_slots & 0xF;
	} while (empty_slots == 0);

fail:
	return error;
}

/**
 * @brief Waits till some bytes are present in Rx Fifo or timeout.
 *
 * @param hi2c i2c controller hi2c
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t i2c_wait_for_rx_fifo_filled(struct tegrabl_i2c *hi2c)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t filled_slots = 0;
	time_t timeout;

	pr_debug("%s: entry\n", __func__);
	timeout = hi2c->fifo_timeout;
	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0) {
			pr_debug("I2C Rx fifo filled timeout\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}
		filled_slots = i2c_readl(hi2c, I2C_FIFO_STATUS_REGISTER);
		filled_slots = filled_slots >> I2C_FIFO_STATUS_RX_FIFO_FULL_CNT_SHIFT;
		filled_slots = filled_slots & 0xF;
	} while (filled_slots == 0);

fail:
	return error;
}

/**
 * @brief Waits till single packet is completely transferred or timeout.
 *
 * @param hi2c i2c controller hi2c.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t i2c_wait_for_transfer_complete(struct tegrabl_i2c *hi2c)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	time_t timeout;
	uint32_t interrupt_status = 0;

	pr_debug("%s: entry\n", __func__);
	timeout = hi2c->xfer_timeout;
	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0) {
			pr_debug("I2C transfer timeout\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}
		interrupt_status = i2c_readl(hi2c, I2C_INTERRUPT_STATUS_REGISTER);

		if ((interrupt_status & I2C_INTERRUPT_STATUS_PACKET_XFER_COMPLETE) != 0U) {
			break;
		} else if ((interrupt_status & I2C_INTERRUPT_STATUS_ARB_LOST) != 0U) {
			pr_debug("I2C arb lost\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_ACCESS, 0);
			goto fail;
		} else if ((interrupt_status & I2C_INTERRUPT_STATUS_NOACK) != 0U) {
			pr_debug("I2C slave not started\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			goto fail;
		} else {
			/* No Action Required */
		}
	} while (true);

fail:
	return error;
}

static void i2c_set_prod_settings(struct tegrabl_i2c *hi2c)
{
	uint32_t i = 0;
	uint32_t reg = 0;
	uint32_t mode = 0;
	struct tegrabl_i2c_prod_setting *setting = NULL;

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_bpmpfw_controlled == true) {
		return;
	}
#endif

	if (hi2c->clk_freq > FM_PLUS_SPEED) {
		mode = 3;
	} else if (hi2c->clk_freq > FM_SPEED) {
		mode = 2;
	} else if (hi2c->clk_freq > STD_SPEED) {
		mode = 1;
	} else {
		mode = 0;
	}

	setting = &i2c_prod_settings[hi2c->instance][mode];

	if (setting->num_settings != 0U) {
		/* Apply prod settings using <addr, mask, value> tuple */
		for (i = 0; i < (setting->num_settings * 3U); i += 3U) {
			reg = NV_READ32(setting->settings[i]);
			reg &= (~setting->settings[i + 1]);
			reg |= (setting->settings[i + 2] &
					setting->settings[i + 1]);
			NV_WRITE32(setting->settings[i], reg);
		}
	}
	return;
}

struct tegrabl_i2c *tegrabl_i2c_open(enum tegrabl_instance_i2c instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_i2c *hi2c = NULL;

	pr_debug("%s: entry\n", __func__);

	if (instance >= TEGRABL_INSTANCE_I2C_INVALID) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}
	list_for_every_entry(&i2c_list, hi2c, struct tegrabl_i2c, node) {
		if (hi2c->instance == instance) {
			return hi2c;
		}
	}

	hi2c = tegrabl_malloc(sizeof(*hi2c));
	if (hi2c == NULL) {
		pr_debug("Failed to allocate memory for i2c struct\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(hi2c, 0x0, sizeof(*hi2c));
	hi2c->instance = instance;
	hi2c->clk_freq = i2c_freq[instance];
	hi2c->requires_cldvfs = true;
	hi2c->is_initialized = false;
	hi2c->module_id = TEGRABL_MODULE_I2C;
	hi2c->base = i2c_base[hi2c->instance];
	hi2c->single_fifo_timeout = I2C_TIMEOUT;
	hi2c->byte_xfer_timeout = I2C_TIMEOUT;

	pr_debug("Instance=%2u, ", hi2c->instance);
	pr_debug("base=0x%08X, ", (uint32_t)hi2c->base);
	pr_debug("frequency=%u, ", hi2c->clk_freq);
	pr_debug("requires cldvfs=%u\n", hi2c->requires_cldvfs);

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (instance == TEGRABL_INSTANCE_POWER_I2C_BPMPFW) {
		hi2c->is_bpmpfw_controlled = true;
		pr_info("virtual i2c enabled\n");
	}
#endif
	error = i2c_reset_controller(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

#if !defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->requires_cldvfs) {
		error = tegrabl_car_rst_set(TEGRABL_MODULE_DVFS, 0);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		error = tegrabl_car_clk_enable(TEGRABL_MODULE_DVFS, 0, NULL);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		error = tegrabl_car_rst_clear(TEGRABL_MODULE_DVFS, 0);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
#endif

	/* I2C4 for HDMI on SOR1 */
	if (instance == TEGRABL_INSTANCE_I2C4) {
#if defined(CONFIG_ENABLE_DPAUX)
		/* dp aux pad control settings for hdmi */
		error = tegrabl_dpaux_init_ddc_i2c(DPAUX_INSTANCE_1);
#else
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
#endif
		if (error != TEGRABL_NO_ERROR)
			goto fail;
	}
	/* I2C6 for HDMI on SOR */
	else if (instance == TEGRABL_INSTANCE_I2C6) {
#if defined(CONFIG_ENABLE_DPAUX)
		/* dp aux pad control settings for hdmi */
		error = tegrabl_dpaux_init_ddc_i2c(DPAUX_INSTANCE_0);
#else
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
#endif
		if (error != TEGRABL_NO_ERROR)
			goto fail;
	}

	i2c_set_prod_settings(hi2c);

	error = tegrabl_i2c_bus_clear(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = i2c_reset_controller(hi2c);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	i2c_set_prod_settings(hi2c);

	hi2c->is_initialized = true;
	list_add_tail(&i2c_list, &hi2c->node);
	pr_debug("%s: exit\n", __func__);

	return hi2c;

fail:
	TEGRABL_SET_HIGHEST_MODULE(error);
	pr_error("%s: failed error = %x\n", __func__, error);
	if (hi2c != NULL) {
		tegrabl_free(hi2c);
	}

	return NULL;
}

tegrabl_error_t tegrabl_i2c_read(struct tegrabl_i2c *hi2c, uint16_t slave_addr,
	bool repeat_start, void *buf, uint32_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t bytes = 0;
	uint32_t data = 0;
	uint8_t *buffer = buf;
	uint32_t i;

	if ((hi2c == NULL) || (buf == NULL) || (len == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return error;
	}

#if defined(CONFIG_POWER_I2C_BPMPFW)
	pr_debug("In func >> %s, line:>> %d\n", __func__, __LINE__);
	if (hi2c->is_bpmpfw_controlled == true) {
		error = tegrabl_virtual_i2c_xfer(hi2c, slave_addr, repeat_start, buf,
										len, true); /* is_read = true */
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		return error; /* TEGRABL_NO_ERROR */
	}
#endif

	pr_debug(
		"%s: instance = %d, slave addr = %x, repeat start = %d, len = %d\n",
		__func__, hi2c->instance, slave_addr, repeat_start, len);

	error = i2c_send_header(hi2c, repeat_start, false, slave_addr, len);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* start bit, slave addr, r/w bit approx takes 2 bytes times and data */
	hi2c->xfer_timeout = hi2c->byte_xfer_timeout * (2 + len);
	hi2c->fifo_timeout = hi2c->single_fifo_timeout * len;

	i = 0;
	while (i < len) {
		error = i2c_wait_for_rx_fifo_filled(hi2c);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		data = 0;
		bytes = MIN(len, sizeof(uint32_t));
		data = i2c_readl(hi2c, I2C_RX_FIFO);
		memcpy(&buffer[i], &data, bytes);
		i += bytes;
	}

	error = i2c_wait_for_transfer_complete(hi2c);

	for (i = 0; i < len; i++) {
		pr_debug("byte[%d] = %02x\n", i, buffer[i]);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: error = %08x\n", __func__, error);
		i2c_reset_controller(hi2c);
		tegrabl_i2c_bus_clear(hi2c);
	}

	return error;
}

tegrabl_error_t tegrabl_i2c_write(struct tegrabl_i2c *hi2c, uint16_t slave_addr,
	bool repeat_start, void *buf, uint32_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t bytes = 0;
	uint32_t data = 0;
	uint8_t *buffer = buf;
	uint32_t i;

	if ((hi2c == NULL) || (buf == NULL) || (len == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return error;
	}
#if defined(CONFIG_POWER_I2C_BPMPFW)
	pr_debug("In func >> %s, line:>> %d\n", __func__, __LINE__);
	if (hi2c->is_bpmpfw_controlled == true) {
		error = tegrabl_virtual_i2c_xfer(hi2c, slave_addr, repeat_start, buf,
										len, false); /* is_read = false */
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		return error; /* TEGRABL_NO_ERROR */
	}
#endif

	pr_debug(
		"%s: instance = %d, slave addr = %x, repeat start = %d, len = %d\n",
		__func__, hi2c->instance, slave_addr, repeat_start, len);

	for (i = 0; i < len; i++) {
		pr_debug("byte[%d] = %02x\n", i, buffer[i]);
	}

	error = i2c_send_header(hi2c, repeat_start, true, slave_addr, len);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* start bit, slave addr, r/w bit approx takes 2 bytes times and data */
	hi2c->xfer_timeout = hi2c->byte_xfer_timeout * (2 + len);
	hi2c->fifo_timeout = hi2c->single_fifo_timeout * len;

	while (len != 0U) {
		error = i2c_wait_for_tx_fifo_empty(hi2c);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		data = 0;
		bytes = MIN(len, sizeof(data));
		memcpy(&data, buffer, bytes);
		i2c_writel(hi2c, I2C_TX_PACKET_FIFO_REGISTER, data);
		len -= bytes;
		buffer += bytes;
	}

	error = i2c_wait_for_transfer_complete(hi2c);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: error = %08x\n", __func__, error);
		i2c_reset_controller(hi2c);
		tegrabl_i2c_bus_clear(hi2c);
	}

	return error;
}

tegrabl_error_t tegrabl_i2c_transaction(struct tegrabl_i2c *hi2c,
	struct tegrabl_i2c_transaction *trans, uint32_t num_trans)
{
	uint32_t i;
	struct tegrabl_i2c_transaction *ptrans;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);

	if ((hi2c == NULL) || (trans == NULL) || (num_trans == 0)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return error;
	}

#if defined(CONFIG_POWER_I2C_BPMPFW)
		if (hi2c->is_bpmpfw_controlled == true) {
			return TEGRABL_NO_ERROR;
		}
#endif


	for (i = 0; i < num_trans; i++) {
		ptrans = trans;
		if (ptrans->is_write)
			error = tegrabl_i2c_write(hi2c, ptrans->slave_addr,
					 ptrans->is_repeat_start, ptrans->buf, ptrans->len);
		else
			error = tegrabl_i2c_read(hi2c, ptrans->slave_addr,
					 ptrans->is_repeat_start, ptrans->buf, ptrans->len);
		if (error != TEGRABL_NO_ERROR) {
			break;
		}
		tegrabl_udelay((time_t)ptrans->wait_time);
		ptrans++;
	}

	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: error = %08x\n", __func__, error);
		i2c_reset_controller(hi2c);
		tegrabl_i2c_bus_clear(hi2c);
	}

	return error;
}

tegrabl_error_t tegrabl_i2c_bus_clear(struct tegrabl_i2c *hi2c)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg_value = 0;
	uint32_t timeout = I2C_TIMEOUT;

	if (hi2c == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return error;
	}

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_bpmpfw_controlled == true) {
		return TEGRABL_NO_ERROR;
	}
#endif

	pr_debug("Bus clear for %d\n", hi2c->instance);
	reg_value |= I2C_BUS_CLEAR_TERMINATE_IMMEDIATE;
	reg_value |= I2C_BUS_CLEAR_STOP_COND_NO_STOP;
	reg_value |= 0x9 << I2C_BUS_CLEAR_SCLK_THRESHOLD_SHIFT;

	i2c_writel(hi2c, I2C_BUS_CLEAR_CONFIG_REGISTER, reg_value);
	error = i2c_load_config(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	reg_value = i2c_readl(hi2c, I2C_BUS_CLEAR_CONFIG_REGISTER);
	reg_value |= I2C_BUS_CLEAR_ENABLE;
	i2c_writel(hi2c, I2C_BUS_CLEAR_CONFIG_REGISTER, reg_value);

	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0) {
			pr_debug("Bus clear timeout\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}
		reg_value = i2c_readl(hi2c, I2C_INTERRUPT_STATUS_REGISTER);
	} while (!(reg_value & I2C_BUS_CLEAR_DONE));

	/* clear interrupt status register */
	i2c_writel(hi2c, I2C_INTERRUPT_STATUS_REGISTER, reg_value);

	reg_value = i2c_readl(hi2c, I2C_BUS_CLEAR_STATUS_REGISTER);
	if (!(reg_value & I2C_BUS_CLEAR_STATUS_BUS_CLEARED)) {
		pr_debug("Bus clear failed\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: error = %08x\n", __func__, error);
		i2c_reset_controller(hi2c);
	}
	return error;
}
