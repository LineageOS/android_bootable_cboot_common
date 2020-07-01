/*
 * Copyright (c) 2015 - 2017, NVIDIA CORPORATION.  All rights reserved.
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

struct tegrabl_i2c_dev *tegrabl_i2c_dev_open(enum tegrabl_instance_i2c instance,
	uint32_t slave_addr, uint32_t reg_addr_size, uint32_t bytes_per_reg)
{
	struct tegrabl_i2c_dev *hi2cdev;
	struct tegrabl_i2c *hi2c;

	pr_debug("%s: entry\n", __func__);

	hi2cdev = tegrabl_malloc(sizeof(*hi2cdev));
	if (hi2cdev == NULL) {
		pr_error("%s: i2c dev memory alloc failed\n", __func__);
		return NULL;
	}

	hi2c = tegrabl_i2c_open(instance);
	if (hi2c == NULL) {
		pr_error("%s: i2c open failed\n", __func__);
		return NULL;
	}

	hi2cdev->hi2c = hi2c;
	hi2cdev->instance = instance;
	hi2cdev->slave_addr = slave_addr;
	hi2cdev->reg_addr_size = reg_addr_size;
	hi2cdev->bytes_per_reg = bytes_per_reg;
	hi2cdev->wait_time_for_write_us = 0;

	pr_debug("%s: exit\n", __func__);
	return hi2cdev;
}

tegrabl_error_t tegrabl_i2c_dev_read(struct tegrabl_i2c_dev *hi2cdev, void *buf,
	uint32_t reg_addr, uint32_t reg_count)
{
	uint8_t *buffer = NULL;
	uint8_t *pbuf = buf;
	bool repeat_start = false;
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

	pr_debug("%s: entry\n", __func__);

	if ((hi2cdev == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}
	hi2c = hi2cdev->hi2c;
	slave_addr = hi2cdev->slave_addr;
	bytes_per_reg = hi2cdev->bytes_per_reg;
	reg_addr_size = hi2cdev->reg_addr_size;

	buffer = tegrabl_malloc(reg_addr_size);
	if (buffer == NULL) {
		pr_error("intermediate memory for i2c failed\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	pr_debug("i2c write slave: 0x%x, register 0x%x\n", slave_addr, reg_addr);
	pr_debug("reg_addr_size %d, instance %d\n", reg_addr_size,
		hi2cdev->instance);
	pr_debug("bytes_per_reg %d\n", bytes_per_reg);

	curr_reg_addr = reg_addr;

	regs_remaining = reg_count;
	do {
		if (regs_remaining * bytes_per_reg  < MAX_I2C_TRANSFER_SIZE)
			regs_to_transfer = regs_remaining;
		else
			regs_to_transfer = DIV_FLOOR(MAX_I2C_TRANSFER_SIZE, bytes_per_reg);
		repeat_start = 1;

		/* copy current slave register address */
		i = 0;
		do  {
			buffer[i] = (curr_reg_addr >> (8 * i)) & 0xFF;
		} while (++i < reg_addr_size);

		error = tegrabl_i2c_write(hi2c, slave_addr, 1, buffer, i);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		i = regs_to_transfer * bytes_per_reg;

		error = tegrabl_i2c_read(hi2c, slave_addr, repeat_start, &pbuf[j], i);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		j  += i;

		pr_debug("curr reg = %x, regs transferred = %d, regs remain = %d\n",
			 curr_reg_addr, regs_to_transfer, regs_remaining);
		regs_remaining -= regs_to_transfer;
		curr_reg_addr += regs_to_transfer;
	} while (regs_remaining != 0U);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("i2c dev read failed\n");
	}

	if (buffer != NULL) {
		tegrabl_free(buffer);
	}
	return error;
}

tegrabl_error_t tegrabl_i2c_dev_write(struct tegrabl_i2c_dev *hi2cdev,
	const void *buf, uint32_t reg_addr, uint32_t reg_count)
{
	uint8_t *buffer = NULL;
	uint8_t *pbuf =  (uint8_t *)buf;
	bool repeat_start = false;
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

	pr_debug("%s: entry\n", __func__);

	if ((hi2cdev == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	hi2c = hi2cdev->hi2c;
	slave_addr = hi2cdev->slave_addr;
	bytes_per_reg = hi2cdev->bytes_per_reg;
	reg_addr_size = hi2cdev->reg_addr_size;

	pr_debug("i2c write slave: 0x%x, register 0x%x\n", slave_addr, reg_addr);
	pr_debug("reg_addr_size %d, instance %d\n", reg_addr_size,
		hi2cdev->instance);
	pr_debug("bytes_per_reg %d\n", bytes_per_reg);

	curr_reg_addr = reg_addr;

	buffer = tegrabl_malloc(MIN(reg_addr_size + reg_count * bytes_per_reg,
		MAX_I2C_TRANSFER_SIZE));
	if (buffer == NULL) {
		pr_error("intermediate memory for i2c failed\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	regs_remaining = reg_count;
	do {
		if (regs_remaining * bytes_per_reg + reg_addr_size <
			MAX_I2C_TRANSFER_SIZE) {
			regs_to_transfer = regs_remaining;
		} else {
			regs_to_transfer = DIV_FLOOR(
				(MAX_I2C_TRANSFER_SIZE - reg_addr_size), bytes_per_reg);
			repeat_start = 1;
		}

		/* copy current slave register address */
		i = 0;
		do  {
			buffer[i] = (curr_reg_addr >> (8 * i)) & 0xFF;
		} while (++i < reg_addr_size);

		memcpy(&buffer[i], &pbuf[j], regs_to_transfer * bytes_per_reg);
		i += regs_to_transfer * bytes_per_reg;
		j += i;

		error = tegrabl_i2c_write(hi2c, slave_addr, repeat_start, buffer, i);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		pr_debug("curr reg = %x, regs transferred = %d, regs remain = %d\n",
			 curr_reg_addr, regs_to_transfer, regs_remaining);
		regs_remaining -= regs_to_transfer;
		curr_reg_addr += regs_to_transfer;
	} while (regs_remaining != 0U);

	/* some slaves requires wait time after write*/
	tegrabl_udelay((time_t)hi2cdev->wait_time_for_write_us);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("i2c dev write failed\n");
	}

	if (buffer != NULL) {
		tegrabl_free(buffer);
	}
	return error;
}

tegrabl_error_t tegrabl_i2c_dev_ioctl(struct tegrabl_i2c_dev *hi2cdev,
	enum tegrabl_i2c_dev_ioctl ioctl, void *args)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);

	if ((hi2cdev == NULL) || (args == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (ioctl) {
	case TEGRABL_I2C_DEV_IOCTL_WAIT_TIME_WRITE:
		hi2cdev->wait_time_for_write_us = *((time_t *)args);
		break;
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		break;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("i2c dev ioctl failed\n");
	}
	return error;
}

tegrabl_error_t tegrabl_i2c_dev_close(struct tegrabl_i2c_dev *hi2cdev)
{
	if (hi2cdev == NULL) {
		pr_error("%s: invliad handle\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	tegrabl_free(hi2cdev);
	return TEGRABL_NO_ERROR;
}
