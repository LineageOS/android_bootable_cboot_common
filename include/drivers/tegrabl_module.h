/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_MODULE_H
#define INCLUDED_TEGRABL_MODULE_H

/**
 * @brief Type for module
 */
typedef enum {
	TEGRABL_MODULE_CLKRST = 0,				/* 0x0 */
	TEGRABL_MODULE_UART = 1,				/* 0x1 */
	TEGRABL_MODULE_SDMMC = 2,				/* 0x2 */
	TEGRABL_MODULE_QSPI = 3,				/* 0x3 */
	TEGRABL_MODULE_SE = 4,					/* 0x4 */
	TEGRABL_MODULE_XUSB_HOST = 5,			/* 0x5 */
	TEGRABL_MODULE_XUSB_DEV = 6,			/* 0x6 */
	TEGRABL_MODULE_XUSB_PADCTL = 7,			/* 0x7 */
	TEGRABL_MODULE_XUSB_SS = 8,				/* 0x8 */
	TEGRABL_MODULE_XUSBF = 9,				/* 0x9 */
	TEGRABL_MODULE_DPAUX1 = 10,				/* 0xA */
	TEGRABL_MODULE_HOST1X = 11,				/* 0xB */
	TEGRABL_MODULE_DVFS = 12,				/* 0xC */
	TEGRABL_MODULE_I2C = 13,				/* 0xD */
	TEGRABL_MODULE_SOR_SAFE = 14,			/* 0xE */
	TEGRABL_MODULE_MEM = 15,				/* 0xF */
	TEGRABL_MODULE_KFUSE = 16,				/* 0x10 */
	TEGRABL_MODULE_NVDEC = 17,				/* 0x11 */
	TEGRABL_MODULE_GPCDMA = 18,				/* 0x12 */
	TEGRABL_MODULE_BPMPDMA = 19,			/* 0x13 */
	TEGRABL_MODULE_SPEDMA = 20,				/* 0x14 */
	TEGRABL_MODULE_SOC_THERM = 21,			/* 0x15 */
	TEGRABL_MODULE_APE = 22,				/* 0x16 */
	TEGRABL_MODULE_ADSP = 23,				/* 0x17 */
	TEGRABL_MODULE_APB2APE = 24,			/* 0x18 */
	TEGRABL_MODULE_SATA = 25,				/* 0x19 */
	TEGRABL_MODULE_PWM = 26,				/* 0x1A */
	TEGRABL_MODULE_DSI = 27,				/* 0x1B */
	TEGRABL_MODULE_SOR = 28,				/* 0x1C */
	TEGRABL_MODULE_SOR_OUT = 29,			/* 0x1D */
	TEGRABL_MODULE_SOR_PAD_CLKOUT = 30,		/* 0x1E */
	TEGRABL_MODULE_DPAUX = 31,				/* 0x1F */
	TEGRABL_MODULE_NVDISPLAYHUB = 32,		/* 0x20 */
	TEGRABL_MODULE_NVDISPLAY_DSC = 33,		/* 0x21 */
	TEGRABL_MODULE_NVDISPLAY_DISP = 34,		/* 0x22 */
	TEGRABL_MODULE_NVDISPLAY_P = 35,		/* 0x23 */
	TEGRABL_MODULE_NVDISPLAY0_HEAD = 36,	/* 0x24 */
	TEGRABL_MODULE_NVDISPLAY0_WGRP = 37,	/* 0x25 */
	TEGRABL_MODULE_NVDISPLAY0_MISC = 38,	/* 0x26 */
	TEGRABL_MODULE_SPI = 39,				/* 0x27 */
	TEGRABL_MODULE_AUD_MCLK = 40,			/* 0x28 */
	TEGRABL_MODULE_UFS = 41,				/* 0x29 */
	TEGRABL_MODULE_SATA_OOB = 42,			/* 0x2A */
	TEGRABL_MODULE_SATACOLD = 43,			/* 0x2B */
	TEGRABL_MODULE_PCIE = 44,				/* 0x2C */
	TEGRABL_MODULE_PCIEXCLK = 45,				/* 0x2D */
	TEGRABL_MODULE_AFI = 46,				/* 0x2E */
	TEGRABL_MODULE_XUSBH = 47,			/* 0x2F */
	TEGRABL_MODULE_NUM = 48,				/* 0x30 Total modules in the list */
} tegrabl_module_t;

#endif /* INCLUDED_TEGRABL_MODULE_H */
