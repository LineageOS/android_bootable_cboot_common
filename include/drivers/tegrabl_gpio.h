/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __GPIO_DRIVER_H
#define __GPIO_DRIVER_H

#include <tegrabl_error.h>
#include <list.h>

#define TEGRA_GPIO(bank, offset) \
		((TEGRA_GPIO_BANK_ID_##bank * 8) + offset)

/* GPIO controllor handle hard-coded here */
#define TEGRA_GPIO_MAIN_CHIPID     0
#define TEGRA_GPIO_AON_CHIPID      1
#define TEGRA_GPIO_TCA9539_CHIPID_BASE  2

typedef uint32_t gpio_pin_state_t;
#define GPIO_PIN_STATE_LOW 0 /* GPIO pin in LOW state. */
#define GPIO_PIN_STATE_HIGH 1 /* GPIO pin in HIGH state. */

typedef uint32_t gpio_pin_mode_t;
#define GPIO_PINMODE_INPUT 0 /* GPIO pin in INPUT mode */
#define GPIO_PINMODE_OUTPUT 1 /* GPIO pin in OUTPUT mode */
#define GPIO_PINMODE_SPIO 2/* Configure pin to SPIO mode */

#define GPIO_OUT_CONTROL_DRIVEN 0 /* GPIO pin out control driven */
#define GPIO_OUT_CONTROL_FLOATED 1 /* GPIO pin out control floated */

/* It is mandatory for any GPIO driver to register these APIs.
 * Else the driver will not be registered to the GPIO core
 */
struct gpio_driver_ops {
	tegrabl_error_t (*read)(uint32_t gpio_num, gpio_pin_state_t *state,
							void *drv_data);
	tegrabl_error_t (*write)(uint32_t gpio_num, gpio_pin_state_t state,
							 void *drv_data);
	tegrabl_error_t (*config)(uint32_t gpio_num, gpio_pin_mode_t mode,
							  void *drv_data);
};

/* Every gpio driver has to fill the below fields before calling
 * gpio_driver_register()
 */
struct gpio_driver {
	int32_t phandle; /* phandle of the GPIO controller. */
	uint32_t chip_id; /* chip id of GPIO controller */
	char *name; /* GPIO driver name. like 'tegra_gpio_driver' etc. */
	void *driver_data; /* private driver data for each device */
	struct list_node node; /* will be part of GPIO drivers list. */
	struct gpio_driver_ops *ops; /* Handle the consumers are interested in */
};

/**
 * @brief The GPIO API for gpio driver registration.
 *
 * @param drv Pointer to the GPIO driver to be registered.
 *
 * @return TEGRABL_NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_gpio_driver_register(struct gpio_driver *drv);

/**
 * @brief Retrieve handle to a gpio driver from its chip_id number
 *
 * @param chip id of gpio controller
 * @param out Callee filled. Points to the driver structure on success
 *
 * @return TEGRABL_NO_ERROR on success and *out points to the required gpio
 * driver handle. On Failure, *out shall be NULL
 */
tegrabl_error_t tegrabl_gpio_driver_get(uint32_t chip_id,
										struct gpio_driver **out);

#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
/**
 * @brief Retrieve chip_id/phandle map
 *
 * @param phandle gpio driver phandle
 * @param chip_id gpio chip id (output param)
 *
 * @return TEGRABL_NO_ERROR on success
 */
tegrabl_error_t tegrabl_gpio_get_chipid_with_phandle(int phandle, uint32_t *chip_id);
#endif

/**
 * @brief The GPIO API for reading a GPIO pin state
 *
 * @param GPIO driver handle
 * @param gpio_num GPIO pin number
 * @param mode GPIO pin state (LOW/HIGH)
 *
 * @return TEGRABL_NO_ERROR on succes with GPIO state in 'state' argument
 *		   otherwise error
 */
static inline tegrabl_error_t gpio_read(struct gpio_driver *drv,
										uint32_t gpio_num,
										gpio_pin_state_t *state)
{
	return drv->ops->read(gpio_num, state, drv->driver_data);
}

/**
 * @brief The GPIO API for setting the state of a GPIO pin
 *
 * @param GPIO driver handle
 * @param gpio_num GPIO pin number
 * @param mode GPIO pin state (LOW/HIGH)
 *
 * @return TEGRABL_NO_ERROR on succes otherwise error
 */
static inline tegrabl_error_t gpio_write(struct gpio_driver *drv,
										 uint32_t gpio_num,
										 gpio_pin_state_t state)
{
	return drv->ops->write(gpio_num, state, drv->driver_data);
}

/**
 * @brief The GPIO API for configuring a GPIO pin
 *
 * @param GPIO driver handle
 * @param gpio_num GPIO pin number.
 * @param mode GPIO pin mode (INPUT/OUTPUT)
 *
 * @return TEGRABL_NO_ERROR on success otherwise error
 */
static inline tegrabl_error_t gpio_config(struct gpio_driver *drv,
										  uint32_t gpio_num,
										  gpio_pin_mode_t mode)
{
	return drv->ops->config(gpio_num, mode, drv->driver_data);
}

/**
 * @brief Initialize gpio driver framework
 */
void gpio_framework_init(void);

/**
 * @brief tegra gpio driver initialisation
 *
 * @return TEGRABL_NO_ERROR on success otherwise error
 */
tegrabl_error_t tegrabl_gpio_driver_init(void);

#endif
