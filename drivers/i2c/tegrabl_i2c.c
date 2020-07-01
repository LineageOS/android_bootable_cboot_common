/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_ar_macro.h>
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
#include <tegrabl_i2c_soc_common.h>
#include <ari2c.h>
#include <tegrabl_i2c_err_aux.h>

/**
 * @brief List of controllers
 */
static struct list_node i2c_list;

static inline void i2c_writel(struct tegrabl_i2c *hi2c, uint32_t reg,
	uint32_t val)
{
	NV_WRITE32(hi2c->base_addr + reg, val);
}

static inline uint32_t i2c_readl(struct tegrabl_i2c *hi2c,
	uint32_t reg)
{
	return NV_READ32(hi2c->base_addr + reg);
}

tegrabl_error_t tegrabl_i2c_set_bus_freq_info(uint32_t *freq, uint32_t num)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct i2c_soc_info *hi2c_info;
	uint32_t num_of_instances;

	if ((num == 0U) || (freq == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				TEGRABL_I2C_SET_BUS_FREQ_INFO);
		TEGRABL_SET_ERROR_STRING(error, "instance : %d, freq: %p", num, freq);
		goto fail;
	}

	i2c_get_soc_info(&hi2c_info, &num_of_instances);

	if (num > num_of_instances) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW,
				TEGRABL_I2C_SET_BUS_FREQ_INFO);
		TEGRABL_SET_ERROR_STRING(error, "instance %d", "instance %d", num, num_of_instances);
		goto fail;
	}

	while (num > 0UL) {
		num--;
		hi2c_info[num].clk_freq = (freq[num] != 0UL) ? freq[num] : STD_SPEED;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_i2c_register(void)
{
	list_initialize(&i2c_list);
	return TEGRABL_NO_ERROR;
}

void tegrabl_i2c_unregister_instance(tegrabl_instance_i2c_t instance)
{
	struct tegrabl_i2c *hi2c, *temp;

	list_for_every_entry_safe(&i2c_list, hi2c, temp, struct tegrabl_i2c, node) {
		if (hi2c->instance == instance) {
			list_delete(&hi2c->node);
			tegrabl_free(hi2c);
		}
	}
}

static uint32_t get_i2c_clock_divisor(uint32_t freq_in, uint32_t tlow, uint32_t thigh, uint32_t freq_out)
{
	uint32_t divisor;
	uint32_t clk_divisor;

	divisor = (tlow + thigh + 2UL) * freq_out;
	clk_divisor = freq_in / divisor;

	if (clk_divisor > 1UL) {
		clk_divisor--;
	}

	return clk_divisor;
}

static tegrabl_error_t i2c_reset_controller(struct tegrabl_i2c *hi2c)
{
	uint32_t i2c_clk_divisor;
	uint32_t reg;
	uint32_t err = TEGRABL_NO_ERROR;
	uint32_t tlow, thigh, source_rate;

	TEGRABL_ASSERT(hi2c != NULL);

	pr_trace("%s: entry %d\n", __func__, hi2c->instance);

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_enable_bpmpfw_i2c == true) {
		return TEGRABL_NO_ERROR;
	}
#endif

	/* Assert reset to i2c */
	err = tegrabl_car_rst_set(hi2c->module_id, (uint8_t)hi2c->instance);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Set the i2c controller clock source */
	err = tegrabl_car_set_clk_src(hi2c->module_id, (uint8_t)hi2c->instance, TEGRABL_CLK_SRC_PLLP_OUT0);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	source_rate = tegrabl_i2c_get_clk_source_rate(hi2c);

	err = tegrabl_car_set_clk_rate(hi2c->module_id, (uint8_t)hi2c->instance, source_rate, &source_rate);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	if (source_rate == 0UL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, I2C_RESET_CONTROLLER);
		TEGRABL_PRINT_ERROR_STRING(err, "source rate %d", source_rate);
		goto fail;
	}

	/* Deassert reset to i2c */
	err = tegrabl_car_rst_clear(hi2c->module_id, (uint8_t)hi2c->instance);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Wait for 5us delay after reset enable */
	tegrabl_udelay(5);

	i2c_set_prod_settings(hi2c);

	if (hi2c->clk_freq < HS_SPEED) {
		reg = i2c_readl(hi2c, I2C_I2C_INTERFACE_TIMING_0_0);

		tlow = NV_DRF_VAL(I2C, I2C_INTERFACE_TIMING_0, TLOW, reg);
		thigh = NV_DRF_VAL(I2C, I2C_INTERFACE_TIMING_0, THIGH, reg);

		i2c_clk_divisor = get_i2c_clock_divisor(source_rate, tlow, thigh, hi2c->clk_freq);

		reg = i2c_readl(hi2c, I2C_I2C_CLK_DIVISOR_REGISTER_0);
		reg = NV_FLD_SET_DRF_NUM(I2C, I2C_CLK_DIVISOR_REGISTER,
								 I2C_CLK_DIVISOR_STD_FAST_MODE, i2c_clk_divisor, reg);
	} else {
		reg = i2c_readl(hi2c, I2C_I2C_HS_INTERFACE_TIMING_0_0);

		tlow = NV_DRF_VAL(I2C, I2C_HS_INTERFACE_TIMING_0, HS_TLOW, reg);
		thigh = NV_DRF_VAL(I2C, I2C_HS_INTERFACE_TIMING_0, HS_THIGH, reg);

		i2c_clk_divisor = get_i2c_clock_divisor(source_rate, tlow, thigh, hi2c->clk_freq);

		reg = i2c_readl(hi2c, I2C_I2C_CLK_DIVISOR_REGISTER_0);
		reg = NV_FLD_SET_DRF_NUM(I2C, I2C_CLK_DIVISOR_REGISTER, I2C_CLK_DIVISOR_HSMODE, i2c_clk_divisor, reg);
	}

	i2c_writel(hi2c, I2C_I2C_CLK_DIVISOR_REGISTER_0, reg);

	pr_trace("%s: exit, i2c_source_freq = %d\n", __func__, rate_set);

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
	uint32_t val = 0;
	uint32_t config_clear;
	uint32_t timeout = CNFG_LOAD_TIMEOUT_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_ASSERT(hi2c != NULL);

	pr_trace("Load from shadow registers to controller registers\n");

	val = NV_DRF_DEF(I2C, I2C_CONFIG_LOAD, MSTR_CONFIG_LOAD, ENABLE);
	i2c_writel(hi2c, I2C_I2C_CONFIG_LOAD_0, val);

	config_clear = NV_DRF_DEF(I2C, I2C_CONFIG_LOAD, MSTR_CONFIG_LOAD, DISABLE);

	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0UL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, I2C_LOAD_CONFIG);
			TEGRABL_SET_ERROR_STRING(error, "load config", "0x%08x", val);
			goto fail;
		}
		val = i2c_readl(hi2c, I2C_I2C_CONFIG_LOAD_0);
		val = NV_DRF_VAL(I2C, I2C_CONFIG_LOAD, MSTR_CONFIG_LOAD, val);
	} while (val != config_clear);

fail:
	return error;
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
	bool repeat_start, bool is_write, uint16_t slave_address, uint32_t length)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t val = 0;

	TEGRABL_ASSERT(hi2c != NULL);

	pr_trace("%s: entry\n", __func__);

	val = NV_DRF_DEF(I2C, I2C_CNFG, PACKET_MODE_EN, GO);
	i2c_writel(hi2c, I2C_I2C_CNFG_0, val);

	val = i2c_readl(hi2c, I2C_INTERRUPT_STATUS_REGISTER_0);
	i2c_writel(hi2c, I2C_INTERRUPT_STATUS_REGISTER_0, val);

	val = 0;
	val |= PACKET_HEADER_I2C_PROTOCOL;
	val |= ((hi2c->instance) << PACKET_HEADER_CONTROLLER_ID_SHIFT);
	i2c_writel(hi2c, I2C_I2C_TX_PACKET_FIFO_0, val);
	pr_trace("header 1 %08x\n", val);

	if (length > 0UL) {
		length--;
	}

	val = 0;
	val = length & 0xFFFUL;
	i2c_writel(hi2c, I2C_I2C_TX_PACKET_FIFO_0, val);
	pr_trace("header 2 %08x\n", val);

	val = slave_address & PACKET_HEADER_SLAVE_ADDRESS_MASK;
	if (is_write == false) {
		val |= PACKET_HEADER_READ_MODE;
	}

	if (repeat_start) {
		val |= PACKET_HEADER_REPEAT_START;
	}

	if (hi2c->clk_freq >= HS_SPEED) {
		val |= PACKET_HEADER_HS_MODE;
	}

	i2c_writel(hi2c, I2C_I2C_TX_PACKET_FIFO_0, val);
	pr_trace("header 3 %08x\n", val);

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
	uint32_t val = 0;

	pr_trace("%s: entry\n", __func__);

	TEGRABL_ASSERT(hi2c != NULL);

	timeout = hi2c->fifo_timeout;
	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0ULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, I2C_WAIT_FOR_TX_FIFO_EMPTY);
			TEGRABL_SET_ERROR_STRING(error, "TX Fifo empty", "0x%08x", val);
			goto fail;
		}

#if defined(I2C_MST_FIFO_STATUS_0)
		val = i2c_readl(hi2c, I2C_MST_FIFO_STATUS_0);
		empty_slots = NV_DRF_VAL(I2C, MST_FIFO_STATUS, TX_FIFO_EMPTY_CNT, val);
#else
		val = i2c_readl(hi2c, I2C_FIFO_STATUS_0);
		empty_slots = NV_DRF_VAL(I2C, FIFO_STATUS, TX_FIFO_EMPTY_CNT, val);
#endif

	} while (empty_slots == 0UL);

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
	uint32_t val = 0;

	pr_trace("%s: entry\n", __func__);

	TEGRABL_ASSERT(hi2c != NULL);

	timeout = hi2c->fifo_timeout;
	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0ULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, I2C_WAIT_FOR_RX_FIFO_FILLED);
			TEGRABL_SET_ERROR_STRING(error, "RX Fifo full", "0x%08x", val);
			goto fail;
		}
#if defined(I2C_MST_FIFO_STATUS_0)
		val = i2c_readl(hi2c, I2C_MST_FIFO_STATUS_0);
		filled_slots = NV_DRF_VAL(I2C, MST_FIFO_STATUS,	RX_FIFO_FULL_CNT, val);
#else
		val = i2c_readl(hi2c, I2C_FIFO_STATUS_0);
		filled_slots = NV_DRF_VAL(I2C, FIFO_STATUS, RX_FIFO_FULL_CNT, val);
#endif
	} while (filled_slots == 0UL);

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
	uint32_t val = 0;

	pr_trace("%s: entry\n", __func__);

	TEGRABL_ASSERT(hi2c != NULL);

	timeout = hi2c->xfer_timeout;
	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0ULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, I2C_WAIT_FOR_TRANSFER_COMPLETE);
			TEGRABL_SET_ERROR_STRING(error, "transfer complete", "0x%08x", val);
			goto fail;
		}
		val = i2c_readl(hi2c, I2C_INTERRUPT_STATUS_REGISTER_0);

		if (NV_DRF_VAL(I2C, INTERRUPT_STATUS_REGISTER, PACKET_XFER_COMPLETE,
				val) != 0U) {
			break;
		} else if (NV_DRF_VAL(I2C, INTERRUPT_STATUS_REGISTER, ARB_LOST, val)
				!= 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_ACCESS,
					I2C_WAIT_FOR_TRANSFER_COMPLETE);
			TEGRABL_SET_ERROR_STRING(error, "bus");
			goto fail;
		} else if (NV_DRF_VAL(I2C, INTERRUPT_STATUS_REGISTER, NOACK, val)
					!= 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND,
					I2C_WAIT_FOR_TRANSFER_COMPLETE);
			TEGRABL_SET_ERROR_STRING(error, "slave", "slaves");
			goto fail;
		} else {
			/* No Action Required */
		}
	} while (true);

fail:
	return error;
}

struct tegrabl_i2c *tegrabl_i2c_open(tegrabl_instance_i2c_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_i2c *hi2c = NULL;
	struct i2c_soc_info *hi2c_info;
	uint32_t num_of_instances;

	pr_trace("%s: entry\n", __func__);

	i2c_get_soc_info(&hi2c_info, &num_of_instances);

	if (instance > num_of_instances - 1UL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_I2C_OPEN_1);
		TEGRABL_SET_ERROR_STRING(error, "%d for instance", instance);
		goto fail;
	}
	list_for_every_entry(&i2c_list, hi2c, struct tegrabl_i2c, node) {
		if (hi2c->instance == instance) {
			return hi2c;
		}
	}

	hi2c = tegrabl_malloc(sizeof(*hi2c));
	if (hi2c == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_I2C_OPEN_1);
		TEGRABL_SET_ERROR_STRING(error, "%d", "struct of instance %d", (uint32_t)sizeof(*hi2c), instance);
		goto fail;
	}
	memset(hi2c, 0x0, sizeof(*hi2c));


	hi2c->instance = instance;
	hi2c->clk_freq = hi2c_info[instance].clk_freq;
	hi2c->base_addr = hi2c_info[instance].base_addr;
	hi2c->is_cldvfs_required = hi2c_info[instance].is_cldvfs_required;
	hi2c->is_bpmpfw_controlled = hi2c_info[instance].is_bpmpfw_controlled;
	hi2c->is_initialized = false;
	hi2c->module_id = TEGRABL_MODULE_I2C;
	hi2c->single_fifo_timeout = I2C_TIMEOUT;
	hi2c->byte_xfer_timeout = I2C_TIMEOUT;

	pr_trace("I2C: Instance=%2u, base=0x%08X, frequency=%u, requires cldvfs=%u\n", hi2c->instance,
			 (uint32_t)hi2c->base_addr, hi2c->clk_freq, hi2c->is_cldvfs_required);

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_bpmpfw_controlled == true) {
		hi2c->is_enable_bpmpfw_i2c = true;
		pr_info("virtual i2c enabled\n");
	}
#endif

	error = i2c_reset_controller(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_RESET_FAILED, "controller %d", instance);
		goto fail;
	}

#if !defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_cldvfs_required == true) {
		error = tegrabl_car_rst_set(TEGRABL_MODULE_CLDVFS, 0);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_SET_FAILED, "assert reset", "cldvfs");
			goto fail;
		}
		error = tegrabl_car_clk_enable(TEGRABL_MODULE_CLDVFS, 0, NULL);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_SET_FAILED, "enable", "cldvfs");
			goto fail;
		}
		error = tegrabl_car_rst_clear(TEGRABL_MODULE_CLDVFS, 0);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_CLEAR_FAILED, "assert reset", "cldvfs");
			goto fail;
		}
	}
#endif

#if defined(CONFIG_ENABLE_DPAUX)
	if (hi2c_info[instance].is_muxed_dpaux) {
		error = tegrabl_dpaux_init_ddc_i2c(hi2c_info[instance].dpaux_instance);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED, "DPAUX-%d for i2c-instance %d",
									   hi2c_info[instance].dpaux_instance, hi2c->instance);
			goto fail;
		}
	}
#endif

	error = tegrabl_i2c_bus_clear(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_CLEAR_FAILED, "bus", "instance %d", hi2c->instance);
		goto fail;
	}

	error = i2c_reset_controller(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_RESET_FAILED, "instance %d", hi2c->instance);
		goto fail;
	}

	hi2c->is_initialized = true;
	list_add_tail(&i2c_list, &hi2c->node);
	pr_trace("%s: exit\n", __func__);

	return hi2c;

fail:
	TEGRABL_SET_HIGHEST_MODULE(error);
	if (hi2c != NULL) {
		tegrabl_free(hi2c);
	}

	return NULL;
}

tegrabl_error_t tegrabl_i2c_read(struct tegrabl_i2c *hi2c, uint16_t slave_addr,
	bool repeat_start, void *buf, uint32_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bytes = 0;
	uint32_t data = 0;
	uint8_t *buffer = buf;
	uint32_t i;

	if ((hi2c == NULL) || (buf == NULL) || (len == 0UL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_READ);
		TEGRABL_SET_ERROR_STRING(error, "hi2c: %p, buf: %p, len: %d", hi2c, buf, len);
		return error;
	}

#if defined(CONFIG_POWER_I2C_BPMPFW)
	pr_trace("In func >> %s, line:>> %d\n", __func__, __LINE__);
	if (hi2c->is_enable_bpmpfw_i2c == true) {
		error = tegrabl_virtual_i2c_xfer(hi2c, slave_addr, repeat_start, buf,
										len, true); /* is_read = true */
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		return error; /* TEGRABL_NO_ERROR */
	}
#endif

	pr_trace(
		"%s: instance = %d, slave addr = %x, repeat start = %d, len = %d\n",
		__func__, hi2c->instance, slave_addr, repeat_start, len);

	error = i2c_send_header(hi2c, repeat_start, false, slave_addr, len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_SEND_FAILED, "header");
		goto fail;
	}

	/* start bit, slave addr, r/w bit approx takes 2 bytes times and data */
	hi2c->xfer_timeout = hi2c->byte_xfer_timeout * (2ULL + len);
	hi2c->fifo_timeout = hi2c->single_fifo_timeout * len;

	i = 0;
	while (i < len) {
		error = i2c_wait_for_rx_fifo_filled(hi2c);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		data = 0;
		bytes = MIN(len, sizeof(uint32_t));
		data = i2c_readl(hi2c, I2C_I2C_RX_FIFO_0);
		memcpy(&buffer[i], &data, bytes);
		i += bytes;
	}

	error = i2c_wait_for_transfer_complete(hi2c);

	for (i = 0; i < len; i++) {
		pr_trace("byte[%d] = %02x\n", i, buffer[i]);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
				"%d bytes from slave: 0x%04x with repeat start %s", len, slave_addr,
				repeat_start ? "true" : "false");
		(void)i2c_reset_controller(hi2c);
		(void)tegrabl_i2c_bus_clear(hi2c);
	}

	return error;
}

tegrabl_error_t tegrabl_i2c_write(struct tegrabl_i2c *hi2c, uint16_t slave_addr,
	bool repeat_start, void *buf, uint32_t len)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bytes = 0;
	uint32_t data = 0;
	uint8_t *buffer = buf;
	uint32_t i;

	if ((hi2c == NULL) || (buf == NULL) || (len == 0UL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_WRITE);
		TEGRABL_SET_ERROR_STRING(error, "hi2c: %p, buf: %p, len: %d", hi2c, buf, len);
		return error;
	}
#if defined(CONFIG_POWER_I2C_BPMPFW)
	pr_trace("In func >> %s, line:>> %d\n", __func__, __LINE__);
	if (hi2c->is_enable_bpmpfw_i2c == true) {
		error = tegrabl_virtual_i2c_xfer(hi2c, slave_addr, repeat_start, buf,
										len, false); /* is_read = false */
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		return error; /* TEGRABL_NO_ERROR */
	}
#endif

	pr_trace("%s: instance = %d, slave addr = %x, repeat start = %d, len = %d\n",
		__func__, hi2c->instance, slave_addr, repeat_start, len);

	for (i = 0; i < len; i++) {
		pr_trace("byte[%d] = %02x\n", i, buffer[i]);
	}

	error = i2c_send_header(hi2c, repeat_start, true, slave_addr, len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_SEND_FAILED, "header");
		goto fail;
	}

	/* start bit, slave addr, r/w bit approx takes 2 bytes times and data */
	hi2c->xfer_timeout = hi2c->byte_xfer_timeout * (2ULL + len);
	hi2c->fifo_timeout = hi2c->single_fifo_timeout * len;

	while (len != 0U) {
		error = i2c_wait_for_tx_fifo_empty(hi2c);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		data = 0;
		bytes = MIN(len, sizeof(data));
		memcpy(&data, buffer, bytes);
		i2c_writel(hi2c, I2C_I2C_TX_PACKET_FIFO_0, data);
		len -= bytes;
		buffer += bytes;
	}

	error = i2c_wait_for_transfer_complete(hi2c);

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_WRITE_FAILED, "%d bytes to slave: 0x%04x with repeat start %s",
				len, slave_addr, repeat_start ? "true" : "false");
		(void)i2c_reset_controller(hi2c);
		(void)tegrabl_i2c_bus_clear(hi2c);
	}

	return error;
}

tegrabl_error_t tegrabl_i2c_transaction(struct tegrabl_i2c *hi2c,
	struct tegrabl_i2c_transaction *trans, uint32_t num_trans)
{
	uint32_t i;
	struct tegrabl_i2c_transaction *ptrans;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_trace("%s: entry\n", __func__);

	if ((hi2c == NULL) || (trans == NULL) || (num_trans == 0UL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_TRANSACTION);
		TEGRABL_SET_ERROR_STRING(error, "hi2c: %p, trans: %p, num_trans: %d", hi2c, trans, num_trans);
		return error;
	}

#if defined(CONFIG_POWER_I2C_BPMPFW)
		if (hi2c->is_enable_bpmpfw_i2c == true) {
			return TEGRABL_NO_ERROR;
		}
#endif


	for (i = 0; i < num_trans; i++) {
		ptrans = trans;
		if (ptrans->is_write == true) {
			error = tegrabl_i2c_write(hi2c, ptrans->slave_addr,
					 ptrans->is_repeat_start, ptrans->buf, ptrans->len);
		} else {
			error = tegrabl_i2c_read(hi2c, ptrans->slave_addr,
					 ptrans->is_repeat_start, ptrans->buf, ptrans->len);
		}
		if (error != TEGRABL_NO_ERROR) {
			break;
		}
		tegrabl_udelay((time_t)ptrans->wait_time);
		ptrans++;
	}

	if (error != TEGRABL_NO_ERROR) {
		pr_trace("%s: error = %08x\n", __func__, error);
		(void)i2c_reset_controller(hi2c);
		(void)tegrabl_i2c_bus_clear(hi2c);
	}

	return error;
}

tegrabl_error_t tegrabl_i2c_bus_clear(struct tegrabl_i2c *hi2c)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t val = 0;
	uint32_t timeout = I2C_TIMEOUT;

	if (hi2c == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_BUS_CLEAR);
		TEGRABL_SET_ERROR_STRING(error, "hi2c: %p", hi2c);
		return error;
	}

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_bpmpfw_controlled == true) {
		return TEGRABL_NO_ERROR;
	}
#endif

	pr_trace("Bus clear for %d\n", hi2c->instance);
	val = NV_DRF_DEF(I2C, I2C_BUS_CLEAR_CONFIG, BC_TERMINATE, IMMEDIATE);
	val = NV_FLD_SET_DRF_DEF(I2C, I2C_BUS_CLEAR_CONFIG, BC_STOP_COND,
			NO_STOP, val);
	val = NV_FLD_SET_DRF_NUM(I2C, I2C_BUS_CLEAR_CONFIG, BC_SCLK_THRESHOLD,
			9, val);
	i2c_writel(hi2c, I2C_I2C_BUS_CLEAR_CONFIG_0, val);

	error = i2c_load_config(hi2c);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED, "config");
		goto fail;
	}

	val = i2c_readl(hi2c, I2C_I2C_BUS_CLEAR_CONFIG_0);
	val = NV_FLD_SET_DRF_NUM(I2C, I2C_BUS_CLEAR_CONFIG, BC_ENABLE, 1, val);
	i2c_writel(hi2c, I2C_I2C_BUS_CLEAR_CONFIG_0, val);

	do {
		tegrabl_udelay((time_t)1);
		timeout--;
		if (timeout == 0UL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, TEGRABL_I2C_BUS_CLEAR);
			TEGRABL_SET_ERROR_STRING(error, "bus clear", "0x%08x", val);
			goto fail;
		}
		val = i2c_readl(hi2c, I2C_INTERRUPT_STATUS_REGISTER_0);
	} while (NV_DRF_VAL(I2C, INTERRUPT_STATUS_REGISTER, BUS_CLEAR_DONE, val)
				!= 1U);

	/* clear interrupt status register */
	i2c_writel(hi2c, I2C_INTERRUPT_STATUS_REGISTER_0, val);

	val = i2c_readl(hi2c, I2C_I2C_BUS_CLEAR_STATUS_0);

	if (NV_DRF_VAL(I2C, I2C_BUS_CLEAR_STATUS, BC_STATUS, val) != 1U) {
		error = TEGRABL_ERROR(TEGRABL_ERR_CONDITION, TEGRABL_I2C_BUS_CLEAR);
		TEGRABL_SET_ERROR_STRING(error, "bus clear");
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_CLEAR_FAILED, "bus", "instance %d", hi2c->instance);
		(void)i2c_reset_controller(hi2c);
	}
	return error;
}
