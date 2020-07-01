/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _TEGRABL_GPIO_LOCAL_H_
#define _TEGRABL_GPIO_LOCAL_H_

#include <stdint.h>

struct tegrabl_gpio_id {
	const char *devname;
	uint32_t base_addr;
	uint32_t bank_count;
	const uint32_t *bank_bases;
};

struct tegrabl_pingroup {
	const char *name;
	const uint32_t pin;
	const uint32_t reg_offset;
};

extern void tegrabl_pinconfig_set(uint32_t pin_num, uint32_t pinconfig);

/* Main GPIO controller */
extern struct tegrabl_gpio_id tegra_gpio_id_main;
#define tegra_gpio_ops_main tegrabl_gpio_ops

/* AON GPIO controller */
extern struct tegrabl_gpio_id tegra_gpio_id_aon;
#define tegra_gpio_ops_aon tegrabl_gpio_ops

#endif
