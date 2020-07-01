/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_EEPROM

#include <tegrabl_eeprom.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_utils.h>
#include <tegrabl_error.h>

#define TEGRABL_I2C_DEFAULT_RETRY_COUNT	1
#define TEGRABL_I2C2_RETRY_COUNT		2

static tegrabl_error_t verify_eeprom_data(struct tegrabl_eeprom *eeprom)
{
	uint8_t calculated_crc = 0, stored_crc = 1;

	calculated_crc = tegrabl_utils_crc8(eeprom->data, eeprom->size - 1);
	stored_crc = eeprom->data[eeprom->size - 1];

	if (calculated_crc != stored_crc) {
		pr_error("eeprom: CRC8 check failed. Stored:0x%02x, Calc:0x%02x\n",
				 stored_crc, calculated_crc);
		return TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_eeprom_read(struct tegrabl_eeprom *eeprom)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_i2c_dev *hi2c_dev = NULL;
	uint32_t retry_count;

	if (!eeprom) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	hi2c_dev = tegrabl_i2c_dev_open(eeprom->instance, eeprom->slave_addr,
									sizeof(eeprom->slave_addr),
									sizeof(*(eeprom->data)));
	if (hi2c_dev == NULL) {
		pr_error("eeprom: Can't get handle to eeprom device @%d\n",
				 eeprom->slave_addr);
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	switch (hi2c_dev->instance) {
	case TEGRABL_INSTANCE_I2C2:
		retry_count = TEGRABL_I2C2_RETRY_COUNT;
		break;
	default:
		retry_count = TEGRABL_I2C_DEFAULT_RETRY_COUNT;
		break;
	}

	while (retry_count != 0) {
		error = tegrabl_i2c_dev_read(hi2c_dev, eeprom->data, 0, eeprom->size);
		if (error == TEGRABL_NO_ERROR) {
			break;
		}

		retry_count--;
		if (retry_count != 0) {
			pr_error("eeprom: Retry to read I2C slave device.\n");
		}
	}

	if (error != TEGRABL_NO_ERROR) {
		pr_error("eeprom: Failed to read I2C slave device\n");
		goto fail;
	}

	if (eeprom->crc_valid)
		error = verify_eeprom_data(eeprom);

	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	return TEGRABL_NO_ERROR;

fail:
	TEGRABL_SET_HIGHEST_MODULE(error);
	return error;
}

tegrabl_error_t tegrabl_eeprom_dump(struct tegrabl_eeprom *eeprom)
{
	char *eeprom_data_str, *ptr;
	unsigned int i, row_size = 16;

	pr_info("Dumping EEPROM data\n");
	pr_info("name = %s\n", eeprom->name);
	pr_info("instance = %u\n",  eeprom->instance);
	pr_info("slave_addr = %u\n", eeprom->slave_addr);
	pr_info("size = %u\n", eeprom->size);
	pr_info("crc_valid = %u\n", eeprom->crc_valid);
	pr_info("data_valid = %u\n", eeprom->data_valid);

	/* 3 bytes/character - 2 for the character itself, and 1 for whitespace
	 * I am over allocating for programming convenience of newline cases
	 */
	eeprom_data_str = tegrabl_calloc(1, eeprom->size * (3 + 1));
	if (eeprom_data_str == NULL) {
		pr_info("Insufficient memory. Can't dump eeprom data\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
	}

	/* Allocate size to hold the characters and the newlines */
	ptr = eeprom_data_str;

	for (i = 0; i < eeprom->size; i++)
		if (i % row_size) {
			tegrabl_snprintf(ptr, 4, "%02x ", eeprom->data[i]);
			ptr += 3;
		} else {
			tegrabl_snprintf(ptr, 5, "\n%02x ", eeprom->data[i]);
			ptr += 4;
		}

	pr_info("### EEPROM DATA DUMP ###\n\n%s\n\n", eeprom_data_str);

	return TEGRABL_NO_ERROR;
}
