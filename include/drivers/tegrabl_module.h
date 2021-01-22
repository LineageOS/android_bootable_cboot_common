/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */
#include <inttypes.h>
#ifndef INCLUDED_TEGRABL_MODULE_H
#define INCLUDED_TEGRABL_MODULE_H

/**
 * @brief Type for module
 */
/* macro tegrabl module */
#define TEGRABL_MODULE_CLKRST 0U			/* 0x0 */
#define TEGRABL_MODULE_UART 1U				/* 0x1 */
#define TEGRABL_MODULE_SDMMC 2U				/* 0x2 */
#define TEGRABL_MODULE_QSPI 3U				/* 0x3 */
#define TEGRABL_MODULE_SE 4U				/* 0x4 */
#define TEGRABL_MODULE_XUSB_HOST 5U			/* 0x5 */
#define TEGRABL_MODULE_XUSB_DEV 6U			/* 0x6 */
#define TEGRABL_MODULE_XUSB_PADCTL 7U			/* 0x7 */
#define TEGRABL_MODULE_XUSB_SS 8U			/* 0x8 */
#define TEGRABL_MODULE_XUSBF 9U				/* 0x9 */
#define TEGRABL_MODULE_DPAUX1 10U			/* 0xA */
#define TEGRABL_MODULE_HOST1X 11U			/* 0xB */
#define TEGRABL_MODULE_CLDVFS 12U			/* 0xC */
#define TEGRABL_MODULE_I2C 13U				/* 0xD */
#define TEGRABL_MODULE_SOR_SAFE 14U			/* 0xE */
#define TEGRABL_MODULE_MEM 15U				/* 0xF */
#define TEGRABL_MODULE_KFUSE 16U			/* 0x10 */
#define TEGRABL_MODULE_NVDEC 17U			/* 0x11 */
#define TEGRABL_MODULE_GPCDMA 18U			/* 0x12 */
#define TEGRABL_MODULE_BPMPDMA 19U			/* 0x13 */
#define TEGRABL_MODULE_SPEDMA 20U			/* 0x14 */
#define TEGRABL_MODULE_SOC_THERM 21U			/* 0x15 */
#define TEGRABL_MODULE_APE 22U				/* 0x16 */
#define TEGRABL_MODULE_ADSP 23U				/* 0x17 */
#define TEGRABL_MODULE_APB2APE 24U			/* 0x18 */
#define TEGRABL_MODULE_SATA 25U				/* 0x19 */
#define TEGRABL_MODULE_PWM 26U				/* 0x1A */
#define TEGRABL_MODULE_DSI 27U				/* 0x1B */
#define TEGRABL_MODULE_SOR 28U				/* 0x1C */
#define TEGRABL_MODULE_SOR_OUT 29U			/* 0x1D */
#define TEGRABL_MODULE_SOR_PAD_CLKOUT 30U		/* 0x1E */
#define TEGRABL_MODULE_DPAUX 31U			/* 0x1F */
#define TEGRABL_MODULE_NVDISPLAYHUB 32U			/* 0x20 */
#define TEGRABL_MODULE_NVDISPLAY_DSC 33U		/* 0x21 */
#define TEGRABL_MODULE_NVDISPLAY_DISP 34U		/* 0x22 */
#define TEGRABL_MODULE_NVDISPLAY_P 35U			/* 0x23 */
#define TEGRABL_MODULE_NVDISPLAY0_HEAD 36U		/* 0x24 */
#define TEGRABL_MODULE_NVDISPLAY0_WGRP 37U		/* 0x25 */
#define TEGRABL_MODULE_NVDISPLAY0_MISC 38U		/* 0x26 */
#define TEGRABL_MODULE_SPI 39U				/* 0x27 */
#define TEGRABL_MODULE_AUD_MCLK 40U			/* 0x28 */
#define TEGRABL_MODULE_CPUINIT 41U			/* 0x29 */
#define TEGRABL_MODULE_SATA_OOB 42U			/* 0x2A */
#define TEGRABL_MODULE_SATACOLD 43U			/* 0x2B */
#define TEGRABL_MODULE_MPHY 44U				/* 0x2C */
#define TEGRABL_MODULE_UFS 45U				/* 0x2D */
#define TEGRABL_MODULE_UFSDEV_REF 46U			/* 0x2E */
#define TEGRABL_MODULE_UFSHC_CG_SYS 47U			/* 0x2F */
#define TEGRABL_MODULE_UPHY 48U				/* 0x30 */
#define TEGRABL_MODULE_PEX_USB_UPHY 49U			/* 0x31 */
#define TEGRABL_MODULE_PEX_USB_UPHY_PLL_MGMNT 50U	/* 0x32*/
#define TEGRABL_MODULE_PCIE 51U				/* 0x33 */
#define TEGRABL_MODULE_PCIEXCLK 52U			/* 0x34 */
#define TEGRABL_MODULE_AFI 53U				/* 0x35 */
#define TEGRABL_MODULE_DPAUX2 54U			/* 0x36 */
#define TEGRABL_MODULE_DPAUX3 55U			/* 0x37 */
#define TEGRABL_MODULE_PEX_SATA_USB_RX_BYP 56U		/*0x38*/
#define TEGRABL_MODULE_VIC 57U				/*0x39*/
#define TEGRABL_MODULE_AXI_CBB 58U				/* 0x3A */
#define TEGRABL_MODULE_EQOS 59U				/* 0x3B */
#define TEGRABL_MODULE_PCIE_APB 60U			/* 0x3C */
#define TEGRABL_MODULE_PCIE_CORE 61U				/* 0x3D */
#define TEGRABL_MODULE_NUM 62U				/* 0x3E Total modules in the list */

typedef uint32_t tegrabl_module_t;

#endif /* INCLUDED_TEGRABL_MODULE_H */
