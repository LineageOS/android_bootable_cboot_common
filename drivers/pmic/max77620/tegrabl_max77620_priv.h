/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef INCLUDED_TEGRABL_MAX77620_PRIV_H
#define INCLUDED_TEGRABL_MAX77620_PRIV_H

#include <tegrabl_pmic_max77620.h>

#define MAX77620_COMPATIBLE "maxim,max77620"
#define MAX77620_GPIO_DRIVER "max77620-gpio"

#define GPIO_ST                 1

#define MAX77620_REG_ONOFF_CFG1 0x41
#define PWR_OFF_BIT             1
#define SW_RST_BIT              7

/* Clear after every 30 secs */
#define WDT_TIMEOUT_MS      30000

/* RTC block slave address */
#define MAX77620_RTC_I2C_SLAVE_ADDRESS 0xD0

/* gpio registers */
#define MAX77620_GPIO0_REG  0x36
#define MAX77620_GPIO1_REG  0x37
#define MAX77620_GPIO2_REG  0x38
#define MAX77620_GPIO3_REG  0x39
#define MAX77620_GPIO4_REG  0x3A
#define MAX77620_GPIO5_REG  0x3B
#define MAX77620_GPIO6_REG  0x3C
#define MAX77620_GPIO7_REG  0x3D
#define MAX77620_AME_GPIO    0x40

#define MAX77620_REG_SD0    0x16
#define MAX77620_REG_SD1    0x17
#define MAX77620_REG_SD2    0x18
#define MAX77620_REG_SD3    0x19
#define MAX77620_REG_CNFG_SD0   0x1D
#define MAX77620_REG_CNFG_SD1   0x1E
#define MAX77620_REG_CNFG_SD2   0x1F
#define MAX77620_REG_CNFG_SD3   0x20

#define MAX77620_REG_NVERC  0x0C

#define MAX77620_REG_LDO0   0x23
#define MAX77620_REG_LDO1   0x25
#define MAX77620_REG_LDO2   0x27
#define MAX77620_REG_LDO3   0x29
#define MAX77620_REG_LDO4   0x2B
#define MAX77620_REG_LDO5   0x2D
#define MAX77620_REG_LDO6   0x2F
#define MAX77620_REG_LDO7   0x31
#define MAX77620_REG_LDO8   0x33

#define MAX77620_REG_CNFG_LDO0  0x24
#define MAX77620_REG_CNFG_LDO1  0x26
#define MAX77620_REG_CNFG_LDO2  0x28
#define MAX77620_REG_CNFG_LDO3  0x2A
#define MAX77620_REG_CNFG_LDO4  0x2C
#define MAX77620_REG_CNFG_LDO5  0x2E
#define MAX77620_REG_CNFG_LDO6  0x30
#define MAX77620_REG_CNFG_LDO7  0x32
#define MAX77620_REG_CNFG_LDO8  0x34

/* Interrupt and mask registers */
#define MAX77620_REG_RTCINT     0x0
#define MAX77620_REG_RTCINTM    0x01
#define MAX77620_REG_CNFGGLBL1  0x00
#define MAX77620_REG_GLBLCNFG3  0x02
#define MAX77620_REG_IRQTOPM    0x0D
#define MAX77620_REG_INTENLBT   0x0E
#define MAX77620_REG_ONOFFIRQM  0x12
#define MAX77620_REG_IRQTOPM_IRQ_GLBLM_MASK (1 << 7)
#define MAX77620_REG_IRQTOP 0x05
#define MAX77620_REG_IRQTOP_IRQONOFF_MASK   0x02
#define MAX77620_REG_ONOFFIRQ   0x0B
#define MAX77620_REG_ONOFFIRQ_EN0_R_MASK    0x08

#define MAX77620_REG_SD_NADE_MASK   (1 << 3)
#define MAX77620_REG_LDO_ADE_MASK   (1 << 1)
#define MAX77620_REG_CNFGGLBL1_MBLPD_MASK   (1 << 6)

#define REG_DATA(_name, _reg, _rtype, _step_uv, _min_uv,        \
				 _max_uv, _ops)                                 \
{                                                               \
	.phandle = 0,                                               \
	.name = _name,                                              \
	.reg = MAX77620_REG_##_reg,                                 \
	.cfg_reg = MAX77620_REG_CNFG_##_reg,                        \
	.regulator_type = MAX77620_REGULATOR_TYPE_##_rtype,         \
	.act_dischrg_en = false,                                    \
	.act_dischrg_dis = false,                                   \
	.step_volts = _step_uv,                                     \
	.min_volts = _min_uv,                                       \
	.max_volts = _max_uv,                                       \
	.ops = _ops,                                                \
}

#endif /*INCLUDED_TEGRABL_MAX77620_PRIV_H*/
