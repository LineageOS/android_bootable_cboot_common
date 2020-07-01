/*
 * TCA9539 16-bit I2C I/O Expander Driver
 *
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_GPIO

#include <tegrabl_utils.h>
#include <tegrabl_gpio.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_io.h>
#include <tegrabl_malloc.h>
#include <tegrabl_i2c.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_gpio.h>
#include <tegrabl_tca9539_gpio.h>
#include <tegrabl_tca9539_plat_config.h>
#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
#include <tegrabl_devicetree.h>
#endif

#define TCA9539_GPIO_DRIVER_NAME	"tca9539_gpio_driver"

#define TCA9539_BANK_NUM		2
#define TCA9539_BANK_SIZE		8

#define TCA9539_REG_ADDR_SIZE	1
#define TCA9539_REG_BYTES		1

/*
 * GPIO Number Bit Mapping(Bit0...Bit7)
 * -----------------------------
 *          | Port#0  | Port#1
 * ---------|---------|---------
 * GPIO Num | 0 ... 7 | 8 ... 15
 * -----------------------------
 *
 *
 * Control Register and Command Byte
 * ---------------------------------------
 * Command | Register        |  Protocol
 * --------|-----------------|------------
 * 0x00    | Input Port#0    |  Read
 * --------|-----------------|------------
 * 0x01    | Input Port#1    |  Read
 * --------|-----------------|------------
 * 0x02    | Output Port#0   |  Read/Write
 * --------|-----------------|------------
 * 0x03    | Output Port#1   |  Read/Write
 * --------|-----------------|------------
 * 0x04    | Polarity Port#0 |  Read/Write
 * --------|-----------------|------------
 * 0x05    | Polarity Port#1 |  Read/Write
 * --------|-----------------|------------
 * 0x06    | Config Port#0   |  Read/Write
 * --------|-----------------|------------
 * 0x07    | Config Port#1   |  Read/Write
 * ---------------------------------------
 */

/* command register index */
#define TCA9539_INPUT	0
#define TCA9539_OUTPUT	1
#define TCA9539_POLAR	2
#define TCA9539_CONFIG	3

struct tca9539_cmd {
	uint8_t input[TCA9539_BANK_NUM];
	uint8_t output[TCA9539_BANK_NUM];
	uint8_t polar[TCA9539_BANK_NUM];
	uint8_t config[TCA9539_BANK_NUM];
};

struct tca9539_cache {
	/* cache output and config register */
	uint8_t output[TCA9539_BANK_NUM];
	uint8_t config[TCA9539_BANK_NUM];
};

struct tca9539_chip {
	/* TCA9539 i2c handler */
	struct tegrabl_i2c_dev *hi2cdev;
	/* i2c bus instance */
	int i2c_inst;
	/* i2c instance name */
	char *i2c_name;
	/* slave address */
	uint32_t i2c_addr;
	/* command register */
	struct tca9539_cmd cmd;
	/* cache of register setting */
	struct tca9539_cache cache;
};

static bool is_gpio_valid(uint32_t gpio_num)
{
	return (gpio_num < TCA9539_GPIO_MAX);
}

/**
 * @brief TCA9539 GPIO driver API for reading a GPIO pin state
 *
 * @param gpio_num The GPIO pin number
 * @param state The variable to read back the current GPIO pin state
 * @param drv_data The private driver data
 *
 * @return TEGRABL_NO_ERROR on success with 'state' argument having GPIO pin
 *         state otherwise error
 */
static tegrabl_error_t tca9539_gpio_read(uint32_t gpio_num,
				gpio_pin_state_t *state, void *drv_data)
{
	struct tca9539_chip *dev = (struct tca9539_chip *)drv_data;
	uint8_t reg, shift, val;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (dev == NULL) {
		pr_error("%s: driver data is not available!\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (!is_gpio_valid(gpio_num)) {
		pr_error("%s: gpio[%d] is not valid!\n", __func__, gpio_num);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	reg = dev->cmd.input[gpio_num / TCA9539_BANK_SIZE];
	shift = gpio_num % TCA9539_BANK_SIZE;

	ret = tegrabl_i2c_dev_read(dev->hi2cdev, &val, reg, sizeof(val));
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("%s: failed to read 0x%02x\n", __func__, reg);
		return ret;
	}

	if ((val >> shift) & 0x01)
		*state = GPIO_PIN_STATE_HIGH;
	else
		*state = GPIO_PIN_STATE_LOW;

	return ret;
}

/**
 * @brief TCA9539 GPIO driver API for setting a GPIO pin state
 *
 * @param gpio_num The GPIO pin number
 * @param state The GPIO pin state (LOW/HIGH)
 * @param drv_data The private driver data
 *
 * @return TEGRABL_NO_ERROR on success otherwise error
 */
static tegrabl_error_t tca9539_gpio_write(uint32_t gpio_num,
										  gpio_pin_state_t state,
										  void *drv_data)
{
	struct tca9539_chip *dev = (struct tca9539_chip *)drv_data;
	uint8_t reg, shift, val;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (dev == NULL) {
		pr_error("%s: driver data is not available!\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (!is_gpio_valid(gpio_num)) {
		pr_error("%s: gpio[%d] is not valid!\n", __func__, gpio_num);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	reg = dev->cmd.output[gpio_num / TCA9539_BANK_SIZE];
	shift = gpio_num % TCA9539_BANK_SIZE;
	val = dev->cache.output[gpio_num / TCA9539_BANK_SIZE];

	if (state == GPIO_PIN_STATE_HIGH)
		val |= (1 << shift);
	else
		val &= ~(1 << shift);

	ret = tegrabl_i2c_dev_write(dev->hi2cdev, &val, reg, sizeof(val));
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("%s: failed to write 0x%02x\n", __func__, reg);
	}

	return ret;
}

/**
 * @brief TCA9539 GPIO driver API for configuring a GPIO pin
 *
 * @param gpio_num The GPIO pin number
 * @param mode The GPIO mode(INPUT/OUTPUT)
 * @param drv_data The private driver data
 *
 * @return TEGRABL_NO_ERROR on success otherwise error
 */
static tegrabl_error_t tca9539_gpio_config(uint32_t gpio_num,
										   gpio_pin_mode_t mode,
										   void *drv_data)
{
	struct tca9539_chip *dev = (struct tca9539_chip *)drv_data;
	uint8_t reg, shift, val;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (dev == NULL) {
		pr_error("%s: driver data is not available!\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (!is_gpio_valid(gpio_num)) {
		pr_error("%s: gpio[%d] is not valid!\n", __func__, gpio_num);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	reg = dev->cmd.config[gpio_num / TCA9539_BANK_SIZE];
	shift = gpio_num % TCA9539_BANK_SIZE;
	val = dev->cache.config[gpio_num / TCA9539_BANK_SIZE];

	if (mode == GPIO_PINMODE_INPUT)
		val |= (1 << shift);
	else if (mode == GPIO_PINMODE_OUTPUT)
		val &= ~(1 << shift);
	else {
		pr_error("%s: input mode(%d) is invalid!\n", __func__, mode);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	ret = tegrabl_i2c_dev_write(dev->hi2cdev, &val, reg, sizeof(val));
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("%s: failed to write 0x%02x\n", __func__, reg);
	}

	return ret;
}

static struct gpio_driver_ops ops = {
	.read = &tca9539_gpio_read,
	.write = &tca9539_gpio_write,
	.config = &tca9539_gpio_config
};

static tegrabl_error_t tca9539_device_init(struct tca9539_chip *dev)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint8_t val;
	int i;

	/* command register initialization */
	for (i = 0; i < TCA9539_BANK_NUM; i++) {
		dev->cmd.input[i] = TCA9539_INPUT * TCA9539_BANK_NUM + i;
		dev->cmd.output[i] = TCA9539_OUTPUT * TCA9539_BANK_NUM + i;
		dev->cmd.polar[i] = TCA9539_POLAR * TCA9539_BANK_NUM + i;
		dev->cmd.config[i] = TCA9539_CONFIG * TCA9539_BANK_NUM + i;
	}

	dev->hi2cdev = tegrabl_i2c_dev_open(dev->i2c_inst, dev->i2c_addr,
										TCA9539_REG_ADDR_SIZE,
										TCA9539_REG_BYTES);
	if (NULL == dev->hi2cdev) {
		pr_error("%s: failed to open i2c device!\n", __func__);
		goto fail;
	}

	for (i = 0; i < TCA9539_BANK_NUM; i++) {
		/* disable polarity inversion */
		val = 0x00;
		ret = tegrabl_i2c_dev_write(dev->hi2cdev, &val, dev->cmd.polar[i],
									sizeof(val));
		if (ret != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to write polar reg\n", __func__);
			goto fail;
		}
		/* read output register */
		ret = tegrabl_i2c_dev_read(dev->hi2cdev, &dev->cache.output[i],
								   dev->cmd.output[i], 1);
		if (ret != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to read output reg\n", __func__);
			goto fail;
		}
		/* read configuration register */
		ret = tegrabl_i2c_dev_read(dev->hi2cdev, &dev->cache.config[i],
								   dev->cmd.config[i], 1);
		if (ret != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to read config reg\n", __func__);
			goto fail;
		}
	}

fail:
	return ret;
}

#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
static char *compatible_chips[] = {
	"ti,tca9539",
	"nxp,tca9539",
};

static tegrabl_error_t fetch_driver_phandle_from_dt(struct tca9539_chip *device,
													struct gpio_driver *drv)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *fdt = NULL;
	int node_itr, i2c_node;
	uint16_t i;
	uintptr_t slave_addr;

	TEGRABL_ASSERT(device != NULL);
	TEGRABL_ASSERT(drv != NULL);

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		goto done;
	}

	for (i = 0; i < ARRAY_SIZE(compatible_chips); i++) {
		err = tegrabl_dt_get_node_with_path(fdt, device->i2c_name, &i2c_node);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to get i2c node for tca9539\n", __func__);
			continue;
		}

		while (1) {
			err = tegrabl_dt_get_node_with_compatible(fdt, i2c_node, compatible_chips[i],
													  &node_itr);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s: failed to get node with compatible %s\n", __func__,
						 compatible_chips[i]);
				break;
			}
			err = tegrabl_dt_read_reg_by_index(fdt, node_itr, 0, &slave_addr, NULL);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s: failed to get i2c address\n", __func__);
				break;
			}
			/* start from next node offset */
			i2c_node = node_itr;
			pr_debug("tca9539 i2c addr in dt: 0x%x\n", (uint32_t)slave_addr);
			/* i2c_addr is shifted by 1 earlier */
			if (slave_addr == (device->i2c_addr >> 1)) {
				drv->phandle = fdt_get_phandle(fdt, node_itr);
				goto done;
			}
		}
	}

done:
	return err;
}
#endif

tegrabl_error_t tegrabl_tca9539_init(void)
{
	struct tca9539_chip *device = NULL;
	struct gpio_driver *driver = NULL;
	uint32_t reg;
	uint32_t i;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	for (i = 0; i < ARRAY_SIZE(tca9539_chips); i++) {
		device = tegrabl_calloc(1, sizeof(struct tca9539_chip));
		if (device == NULL) {
			pr_error("%s: failed to alloc memory!\n", __func__);
			return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		}

		device->i2c_inst = tca9539_chips[i].i2c_inst;
		device->i2c_name = tca9539_chips[i].i2c_name;

		reg = (uintptr_t)tca9539_chips[i].i2c_addr;
		/* 8 bit address */
		device->i2c_addr = reg << 1;

		pr_info("%s: i2c bus: %d, slave addr: 0x%02x\n",
				__func__, device->i2c_inst, device->i2c_addr);

		driver = tegrabl_calloc(1, sizeof(struct gpio_driver));
		if (driver == NULL) {
			pr_error("%s: failed to alloc memory!\n", __func__);
			ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}

		driver->chip_id = tca9539_chips[i].chip_id;
		driver->driver_data = (void *)device;
		driver->name = TCA9539_GPIO_DRIVER_NAME;
		driver->phandle = -1;
		driver->ops = &ops;

#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
		/* fetch driver phandle from dt, error in this is not fatal */
		ret = fetch_driver_phandle_from_dt(device, driver);
		if (ret != TEGRABL_NO_ERROR) {
			pr_warn("%s: failed to fetch phandle from dt\n", __func__);
			continue;
		}
#endif

		/* initialize device */
		ret = tca9539_device_init(device);
		if (ret != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to init device!\n", __func__);
			continue;
		}

		/* register gpio driver to the driver list */
		ret = tegrabl_gpio_driver_register(driver);
		if (ret != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to register gpio driver!\n", __func__);
			goto fail;
		}
	}

	return TEGRABL_NO_ERROR;

fail:
	if (driver) {
		tegrabl_free(driver);
	}
	if (device) {
		tegrabl_free(device);
	}

	return ret;
}
