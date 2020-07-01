/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _TEGRABL_GPIO_HW_H_
#define _TEGRABL_GPIO_HW_H_

#include <tegrabl_gpio.h>

/* Main GPIO controller */
extern struct tegrabl_gpio_id tegra_gpio_id_main;
#define tegra_gpio_ops_main tegrabl_gpio_ops

/* AON GPIO controller */
extern struct tegrabl_gpio_id tegra_gpio_id_aon;
#define tegra_gpio_ops_aon tegrabl_gpio_ops

/**
 * TODO:
 * Remove these macro hard coding when the redefinition in argpio.h and
 * argpio_aon.h are fixed.
 * Tracked in Bug 200216217
 */

/* GPIOs implemented by main GPIO controller */
#define TEGRA_GPIO_BANK_ID_A 0
#define TEGRA_GPIO_BANK_ID_B 1
#define TEGRA_GPIO_BANK_ID_C 2
#define TEGRA_GPIO_BANK_ID_D 3
#define TEGRA_GPIO_BANK_ID_E 4
#define TEGRA_GPIO_BANK_ID_F 5
#define TEGRA_GPIO_BANK_ID_G 6
#define TEGRA_GPIO_BANK_ID_H 7
#define TEGRA_GPIO_BANK_ID_I 8
#define TEGRA_GPIO_BANK_ID_J 9
#define TEGRA_GPIO_BANK_ID_K 10
#define TEGRA_GPIO_BANK_ID_L 11
#define TEGRA_GPIO_BANK_ID_M 12
#define TEGRA_GPIO_BANK_ID_N 13
#define TEGRA_GPIO_BANK_ID_O 14
#define TEGRA_GPIO_BANK_ID_P 15
#define TEGRA_GPIO_BANK_ID_Q 16
#define TEGRA_GPIO_BANK_ID_R 17
#define TEGRA_GPIO_BANK_ID_T 18
#define TEGRA_GPIO_BANK_ID_X 19
#define TEGRA_GPIO_BANK_ID_Y 20
#define TEGRA_GPIO_BANK_ID_BB 21
#define TEGRA_GPIO_BANK_ID_CC 22

/* GPIOs implemented by AON GPIO controller */
#define TEGRA_GPIO_BANK_ID_S 0
#define TEGRA_GPIO_BANK_ID_U 1
#define TEGRA_GPIO_BANK_ID_V 2
#define TEGRA_GPIO_BANK_ID_W 3
#define TEGRA_GPIO_BANK_ID_Z 4
#define TEGRA_GPIO_BANK_ID_AA 5
#define TEGRA_GPIO_BANK_ID_EE 6
#define TEGRA_GPIO_BANK_ID_FF 7

#ifndef _MK_SHIFT_CONST
#define _MK_SHIFT_CONST(_constant_) _constant_
#endif
#ifndef _MK_MASK_CONST
#define _MK_MASK_CONST(_constant_) _constant_
#endif
#ifndef _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_) (_constant_ ## UL)
#endif
#ifndef _MK_ADDR_CONST
#define _MK_ADDR_CONST(_constant_) _constant_
#endif
#ifndef _MK_FIELD_CONST
#define _MK_FIELD_CONST(_mask_, _shift_) (_MK_MASK_CONST(_mask_) << _MK_SHIFT_CONST(_shift_))
#endif

/* GPIO main controller bases */
#define GPIO_A_ENABLE_CONFIG_00_0 0x12000
#define GPIO_B_ENABLE_CONFIG_00_0 0x13000
#define GPIO_C_ENABLE_CONFIG_00_0 0x13200
#define GPIO_D_ENABLE_CONFIG_00_0 0x13400
#define GPIO_E_ENABLE_CONFIG_00_0 0x12200
#define GPIO_F_ENABLE_CONFIG_00_0 0x12400
#define GPIO_G_ENABLE_CONFIG_00_0 0x14200
#define GPIO_H_ENABLE_CONFIG_00_0 0x11000
#define GPIO_I_ENABLE_CONFIG_00_0 0x10800
#define GPIO_J_ENABLE_CONFIG_00_0 0x15000
#define GPIO_K_ENABLE_CONFIG_00_0 0x15200
#define GPIO_L_ENABLE_CONFIG_00_0 0x11200
#define GPIO_M_ENABLE_CONFIG_00_0 0x15600
#define GPIO_N_ENABLE_CONFIG_00_0 0x10000
#define GPIO_O_ENABLE_CONFIG_00_0 0x10200
#define GPIO_P_ENABLE_CONFIG_00_0 0x14000
#define GPIO_Q_ENABLE_CONFIG_00_0 0x10400
#define GPIO_R_ENABLE_CONFIG_00_0 0x10a00
#define GPIO_T_ENABLE_CONFIG_00_0 0x10600
#define GPIO_X_ENABLE_CONFIG_00_0 0x11400
#define GPIO_Y_ENABLE_CONFIG_00_0 0x11600
#define GPIO_BB_ENABLE_CONFIG_00_0 0x12600
#define GPIO_CC_ENABLE_CONFIG_00_0 0x15400

#define GPIO_N_ENABLE_CONFIG_01_0 0x10020

/* GPIO aon controller bases */
#define GPIO_S_ENABLE_CONFIG_00_0 0x1200
#define GPIO_U_ENABLE_CONFIG_00_0 0x1400
#define GPIO_V_ENABLE_CONFIG_00_0 0x1800
#define GPIO_W_ENABLE_CONFIG_00_0 0x1a00
#define GPIO_Z_ENABLE_CONFIG_00_0 0x1e00
#define GPIO_AA_ENABLE_CONFIG_00_0 0x1c00
#define GPIO_EE_ENABLE_CONFIG_00_0 0x1600
#define GPIO_FF_ENABLE_CONFIG_00_0 0x1000

/* Register GPIO_N_INPUT_00_0 */
#define GPIO_N_INPUT_00_0                       _MK_ADDR_CONST(0x10008)
#define GPIO_N_INPUT_00_0_SECURE                        0x0
#define GPIO_N_INPUT_00_0_SCR                   GPIO_N_SCR_00_0
#define GPIO_N_INPUT_00_0_WORD_COUNT                    0x1
#define GPIO_N_INPUT_00_0_RESET_VAL                     _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_RESET_MASK                    _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_READ_MASK                     _MK_MASK_CONST(0x1)
#define GPIO_N_INPUT_00_0_WRITE_MASK                    _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_GPIO_IN_SHIFT                 _MK_SHIFT_CONST(0)
#define GPIO_N_INPUT_00_0_GPIO_IN_FIELD                 _MK_FIELD_CONST(0x1, GPIO_N_INPUT_00_0_GPIO_IN_SHIFT)
#define GPIO_N_INPUT_00_0_GPIO_IN_RANGE                 0:0
#define GPIO_N_INPUT_00_0_GPIO_IN_WOFFSET                       0x0
#define GPIO_N_INPUT_00_0_GPIO_IN_DEFAULT                       _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_GPIO_IN_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_GPIO_IN_SW_DEFAULT                    _MK_MASK_CONST(0x0)
#define GPIO_N_INPUT_00_0_GPIO_IN_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)

#define GPIO_N_SCR_00_0                 _MK_ADDR_CONST(0x4)

/* Register GPIO_N_ENABLE_CONFIG_00_0 */
#define GPIO_N_ENABLE_CONFIG_00_0_SECURE                        0x0
#define GPIO_N_ENABLE_CONFIG_00_0_SCR                   GPIO_N_SCR_00_0
#define GPIO_N_ENABLE_CONFIG_00_0_WORD_COUNT                    0x1
#define GPIO_N_ENABLE_CONFIG_00_0_RESET_VAL                     _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_RESET_MASK                    _MK_MASK_CONST(0xff)
#define GPIO_N_ENABLE_CONFIG_00_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_READ_MASK                     _MK_MASK_CONST(0xff)
#define GPIO_N_ENABLE_CONFIG_00_0_WRITE_MASK                    _MK_MASK_CONST(0xff)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_SHIFT                     _MK_SHIFT_CONST(0)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_FIELD                     _MK_FIELD_CONST(0x1, GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_SHIFT)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_RANGE                     0:0
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_WOFFSET                   0x0
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_DEFAULT                   _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_DISABLE                   _MK_ENUM_CONST(0)
#define GPIO_N_ENABLE_CONFIG_00_0_GPIO_ENABLE_ENABLE                    _MK_ENUM_CONST(1)

#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_SHIFT                  _MK_SHIFT_CONST(1)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_FIELD                  _MK_FIELD_CONST(0x1, GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_SHIFT)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_RANGE                  1:1
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_WOFFSET                        0x0
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_DEFAULT                        _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_IN                     _MK_ENUM_CONST(0)
#define GPIO_N_ENABLE_CONFIG_00_0_IN_OUT_OUT                    _MK_ENUM_CONST(1)

/* Register GPIO_N_OUTPUT_VALUE_00_0 */
#define GPIO_N_OUTPUT_VALUE_00_0                        _MK_ADDR_CONST(0x10010)
#define GPIO_N_OUTPUT_VALUE_00_0_SECURE                         0x0
#define GPIO_N_OUTPUT_VALUE_00_0_SCR                    GPIO_N_SCR_00_0
#define GPIO_N_OUTPUT_VALUE_00_0_WORD_COUNT                     0x1
#define GPIO_N_OUTPUT_VALUE_00_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_VALUE_00_0_RESET_MASK                     _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_VALUE_00_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_VALUE_00_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_VALUE_00_0_READ_MASK                      _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_VALUE_00_0_WRITE_MASK                     _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_FIELD                     _MK_FIELD_CONST(0x1, GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_SHIFT)
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_RANGE                     0:0
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_WOFFSET                   0x0
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_VALUE_00_0_GPIO_OUT_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)

/* Register GPIO_N_OUTPUT_CONTROL_00_0 */
#define GPIO_N_OUTPUT_CONTROL_00_0                      _MK_ADDR_CONST(0x1000c)
#define GPIO_N_OUTPUT_CONTROL_00_0_SECURE                       0x0
#define GPIO_N_OUTPUT_CONTROL_00_0_SCR                  GPIO_N_SCR_00_0
#define GPIO_N_OUTPUT_CONTROL_00_0_WORD_COUNT                   0x1
#define GPIO_N_OUTPUT_CONTROL_00_0_RESET_VAL                    _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_CONTROL_00_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_CONTROL_00_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_CONTROL_00_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_CONTROL_00_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_CONTROL_00_0_WRITE_MASK                   _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_SHIFT                       _MK_SHIFT_CONST(0)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_FIELD                       _MK_FIELD_CONST(0x1, GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_SHIFT)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_RANGE                       0:0
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_WOFFSET                     0x0
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_DEFAULT                     _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_DEFAULT_MASK                        _MK_MASK_CONST(0x1)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_DRIVEN                      _MK_ENUM_CONST(0)
#define GPIO_N_OUTPUT_CONTROL_00_0_GPIO_OUT_CONTROL_FLOATED                     _MK_ENUM_CONST(1)

#endif
