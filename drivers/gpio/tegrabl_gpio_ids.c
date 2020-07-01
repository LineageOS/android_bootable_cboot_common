/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <address_map_new.h>
#include <tegrabl_gpio_hw.h>
#include <tegrabl_utils.h>
#include <tegrabl_gpio_local.h>

/* main controllor addr base list */
static const uint32_t tegra_gpio_main_bases[] = {
	GPIO_A_ENABLE_CONFIG_00_0,
	GPIO_B_ENABLE_CONFIG_00_0,
	GPIO_C_ENABLE_CONFIG_00_0,
	GPIO_D_ENABLE_CONFIG_00_0,
	GPIO_E_ENABLE_CONFIG_00_0,
	GPIO_F_ENABLE_CONFIG_00_0,
	GPIO_G_ENABLE_CONFIG_00_0,
	GPIO_H_ENABLE_CONFIG_00_0,
	GPIO_I_ENABLE_CONFIG_00_0,
	GPIO_J_ENABLE_CONFIG_00_0,
	GPIO_K_ENABLE_CONFIG_00_0,
	GPIO_L_ENABLE_CONFIG_00_0,
	GPIO_M_ENABLE_CONFIG_00_0,
	GPIO_N_ENABLE_CONFIG_00_0,
	GPIO_O_ENABLE_CONFIG_00_0,
	GPIO_P_ENABLE_CONFIG_00_0,
	GPIO_Q_ENABLE_CONFIG_00_0,
	GPIO_R_ENABLE_CONFIG_00_0,
	GPIO_T_ENABLE_CONFIG_00_0,
	GPIO_X_ENABLE_CONFIG_00_0,
	GPIO_Y_ENABLE_CONFIG_00_0,
	GPIO_BB_ENABLE_CONFIG_00_0,
	GPIO_CC_ENABLE_CONFIG_00_0,
};

struct tegrabl_gpio_id tegra_gpio_id_main = {
	.devname = "gpio-main",
	.base_addr = NV_ADDRESS_MAP_GPIO_CTL0_BASE,
	.bank_count = ARRAY_SIZE(tegra_gpio_main_bases),
	.bank_bases = tegra_gpio_main_bases,
};

/* aon controllor addr base list */
static const uint32_t tegra_gpio_aon_bases[] = {
	GPIO_S_ENABLE_CONFIG_00_0,
	GPIO_U_ENABLE_CONFIG_00_0,
	GPIO_V_ENABLE_CONFIG_00_0,
	GPIO_W_ENABLE_CONFIG_00_0,
	GPIO_Z_ENABLE_CONFIG_00_0,
	GPIO_AA_ENABLE_CONFIG_00_0,
	GPIO_EE_ENABLE_CONFIG_00_0,
	GPIO_FF_ENABLE_CONFIG_00_0,
};

struct tegrabl_gpio_id tegra_gpio_id_aon = {
	.devname = "gpio-aon",
	.base_addr = NV_ADDRESS_MAP_AON_GPIO_0_BASE,
	.bank_count = ARRAY_SIZE(tegra_gpio_aon_bases),
	.bank_bases = tegra_gpio_aon_bases,
};
