/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_I2C_DEV

#include <string.h>
#include <tegrabl_malloc.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_debug.h>
#include <stddef.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_utils.h>
#include <tegrabl_i2c.h>
#include <tegrabl_i2c_dev_err_aux.h>

struct tegrabl_i2c_dev *tegrabl_i2c_dev_open(tegrabl_instance_i2c_t instance,
	uint32_t slave_addr, uint32_t reg_addr_size, uint32_t bytes_per_reg)
{
	struct tegrabl_i2c_dev *hi2cdev;
	struct tegrabl_i2c *hi2c;

	pr_trace("%s: entry\n", __func__);

	hi2cdev = tegrabl_malloc(sizeof(*hi2cdev));
	if (hi2cdev == NULL) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_NO_MEMORY, "%d", "i2c dev for instnace %d",
			(uint32_t)sizeof(*hi2cdev), instance);
		return NULL;
	}

	hi2c = tegrabl_i2c_open(instance);
	if (hi2c == NULL) {
		tegrabl_free(hi2cdev);
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED, "instance %d", instance);
		return NULL;
	}

	hi2cdev->hi2c = hi2c;
	hi2cdev->instance = instance;
	hi2cdev->slave_addr = (uint16_t)slave_addr;
	hi2cdev->reg_addr_size = reg_addr_size;
	hi2cdev->bytes_per_reg = bytes_per_reg;
	hi2cdev->wait_time_for_write_us = 0;

	pr_trace("%s: exit\n", __func__);
	return hi2cdev;
}

tegrabl_error_t tegrabl_i2c_dev_read(struct tegrabl_i2c_dev *hi2cdev, void *buf,
	uint32_t reg_addr, uint32_t reg_count)
{
	uint8_t *buffer = NULL;
	uint8_t *pbuf = buf;
	bool repeat_start;
	struct tegrabl_i2c *hi2c = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t curr_reg_addr;
	uint16_t slave_addr;
	uint32_t bytes_per_reg;
	uint32_t reg_addr_size;
	uint32_t regs_remaining;
	uint32_t regs_to_transfer;
	uint32_t i;
	uint32_t j = 0;

	pr_trace("%s: entry\n", __func__);

	if ((hi2cdev == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_DEV_READ);
		TEGRABL_SET_ERROR_STRING(error, "hi2cdev %p, buf %p", hi2cdev, buf);
		goto invalid_param;
	}

	hi2c = hi2cdev->hi2c;
	slave_addr = hi2cdev->slave_addr;
	bytes_per_reg = hi2cdev->bytes_per_reg;
	reg_addr_size = hi2cdev->reg_addr_size;

	if (reg_addr_size > sizeof(reg_addr)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE, TEGRABL_I2C_DEV_READ);
		TEGRABL_SET_ERROR_STRING(error, "register address size %d", "%d",
				reg_addr_size, (uint32_t)sizeof(reg_addr));
		goto fail;
	}

	buffer = tegrabl_malloc(reg_addr_size);
	if (buffer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_I2C_DEV_READ);
		TEGRABL_SET_ERROR_STRING(error, "%d", "register address computation", reg_addr_size);
		goto fail;
	}

	pr_trace("i2c read slave: 0x%x, register 0x%x\n", slave_addr, reg_addr);
	pr_trace("reg_addr_size %d, instance %d\n", reg_addr_size,
		hi2cdev->instance);
	pr_trace("bytes_per_reg %d\n", bytes_per_reg);

	curr_reg_addr = reg_addr;

	regs_remaining = reg_count;
	do {
		if ((regs_remaining * bytes_per_reg) < MAX_I2C_TRANSFER_SIZE) {
			regs_to_transfer = regs_remaining;
		} else {
			regs_to_transfer = DIV_FLOOR(MAX_I2C_TRANSFER_SIZE, bytes_per_reg);
		}
		regs_remaining -= regs_to_transfer;
		repeat_start = regs_remaining ? true : false;

		/* copy current slave register address */
		i = 0;
		do  {
			buffer[i] = (uint8_t)((curr_reg_addr >> (8U * i)) & 0xFFU);
		} while (++i < reg_addr_size);

		error = tegrabl_i2c_write(hi2c, slave_addr, true, buffer, i);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_SEND_FAILED, "register address 0x%08x", reg_addr);
			goto fail;
		}

		i = regs_to_transfer * bytes_per_reg;

		error = tegrabl_i2c_read(hi2c, slave_addr, repeat_start, &pbuf[j], i);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
					"data of size %d at register address 0x%08x", i, reg_addr);
			goto fail;
		}

		j += i;

		pr_trace("curr reg = %x, regs transferred = %u, regs remain = %u\n",
			 curr_reg_addr, regs_to_transfer, regs_remaining);
		curr_reg_addr += regs_to_transfer;
	} while (regs_remaining != 0U);

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
				"%d registers of size %d from slave 0x%02x at 0x%08x via instance %d",
				reg_count, bytes_per_reg, slave_addr, reg_addr, hi2cdev->instance);
	}

	if (buffer != NULL) {
		tegrabl_free(buffer);
	}

invalid_param:
	return error;
}

tegrabl_error_t tegrabl_i2c_dev_write(struct tegrabl_i2c_dev *hi2cdev,
	const void *buf, uint32_t reg_addr, uint32_t reg_count)
{
	uint8_t *buffer = NULL;
	uint8_t *pbuf =  (uint8_t *)buf;
	bool repeat_start;
	struct tegrabl_i2c *hi2c = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t curr_reg_addr;
	uint16_t slave_addr;
	uint32_t bytes_per_reg;
	uint32_t reg_addr_size;
	uint32_t regs_remaining;
	uint32_t regs_to_transfer;
	uint32_t i;
	uint32_t j = 0;

	pr_trace("%s: entry\n", __func__);

	if ((hi2cdev == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_DEV_WRITE);
		TEGRABL_SET_ERROR_STRING(error, "hi2cdev %p, buf %p", hi2cdev, buf);
		goto invalid_param;
	}

	hi2c = hi2cdev->hi2c;
	slave_addr = hi2cdev->slave_addr;
	bytes_per_reg = hi2cdev->bytes_per_reg;
	reg_addr_size = hi2cdev->reg_addr_size;

	if (reg_addr_size > sizeof(reg_addr)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE, TEGRABL_I2C_DEV_WRITE);
		TEGRABL_SET_ERROR_STRING(error, "register address size %d", "%d",
				reg_addr_size, (uint32_t)sizeof(reg_addr));
		goto fail;
	}

	pr_trace("i2c write slave: 0x%x, register 0x%x\n", slave_addr, reg_addr);
	pr_trace("reg_addr_size %d, instance %d\n", reg_addr_size,
		hi2cdev->instance);
	pr_trace("bytes_per_reg %d\n", bytes_per_reg);

	curr_reg_addr = reg_addr;

	buffer = tegrabl_malloc(MIN((reg_addr_size + (reg_count * bytes_per_reg)),
		MAX_I2C_TRANSFER_SIZE));
	if (buffer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_I2C_DEV_WRITE);
		TEGRABL_SET_ERROR_STRING(error, "%d", "register address computation", reg_addr_size);
		goto fail;
	}

	regs_remaining = reg_count;

	do {
		if (((regs_remaining * bytes_per_reg) + reg_addr_size) <
			MAX_I2C_TRANSFER_SIZE) {
			regs_to_transfer = regs_remaining;
		} else {
			regs_to_transfer = DIV_FLOOR(
				(MAX_I2C_TRANSFER_SIZE - reg_addr_size), bytes_per_reg);
		}
		regs_remaining -= regs_to_transfer;
		repeat_start = regs_remaining ? true : false;

		/* copy current slave register address */
		i = 0;
		do  {
			buffer[i] = (uint8_t)((curr_reg_addr >> (8U * i)) & 0xFFU);
		} while (++i < reg_addr_size);

		memcpy(&buffer[i], &pbuf[j], regs_to_transfer * bytes_per_reg);
		i += regs_to_transfer * bytes_per_reg;
		j += regs_to_transfer * bytes_per_reg;

		error = tegrabl_i2c_write(hi2c, slave_addr, repeat_start, buffer, i);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error)
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_SEND_FAILED, "register address 0x%08x", reg_addr);
			goto fail;
		}
		pr_trace("curr reg = %x, regs transferred = %u, regs remain = %u\n",
			 curr_reg_addr, regs_to_transfer, regs_remaining);
		curr_reg_addr += regs_to_transfer;
	} while (regs_remaining != 0U);

	/* some slaves requires wait time after write*/
	tegrabl_udelay((time_t)hi2cdev->wait_time_for_write_us);

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_WRITE_FAILED,
				"%d registers of size %d to slave 0x%02x at 0x%08x via instance %d",
				reg_count, bytes_per_reg, slave_addr, reg_addr, hi2cdev->instance);
	}

	if (buffer != NULL) {
		tegrabl_free(buffer);
	}

invalid_param:
	return error;
}

tegrabl_error_t tegrabl_i2c_dev_ioctl(struct tegrabl_i2c_dev *hi2cdev,
	tegrabl_i2c_dev_ioctl_t ioctl, void *args)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_trace("%s: entry\n", __func__);

	if ((hi2cdev == NULL) || (args == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_DEV_IOCTL);
		TEGRABL_SET_ERROR_STRING(error, "hi2cdev %p, args %p", hi2cdev, args);
		goto invalid_param;
	}

	switch (ioctl) {
	case TEGRABL_I2C_DEV_IOCTL_WAIT_TIME_WRITE:
		hi2cdev->wait_time_for_write_us = *((time_t *)args);
		break;
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, TEGRABL_I2C_DEV_IOCTL);
		TEGRABL_SET_ERROR_STRING(error, "ioctl %d", ioctl);
		break;
	}

	/* Keep label of fail but just goto it for passing MISRA checking */
	goto fail;

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_COMMAND_FAILED, "ioctl %d on instance %d",
				ioctl, hi2cdev->instance);
	}

invalid_param:
	return error;
}

tegrabl_error_t tegrabl_i2c_dev_close(struct tegrabl_i2c_dev *hi2cdev)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hi2cdev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_I2C_DEV_CLOSE);
		TEGRABL_SET_ERROR_STRING(error, "hi2cdev %p", hi2cdev);
		return error;
	}

	tegrabl_free(hi2cdev);
	return TEGRABL_NO_ERROR;
}
