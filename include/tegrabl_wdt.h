/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_WDT_H
#define TEGRABL_WDT_H

#include <stdint.h>
#include <tegrabl_error.h>

/**
 * @brief WDT instance
 *
 * TEGRABL_WDT_BPMP corresponds to BPMP-WDT
 * TEGRABL_WDT_LCCPLEX corresponds to LCCPLEX_WDT
 */
typedef uint32_t tegrabl_wdt_instance_t;
#define TEGRABL_WDT_BPMP 0
#define TEGRABL_WDT_LCCPLEX 1

/* macro tegrabl wdt expiry */
#define TEGRABL_WDT_EXPIRY_1 0x1
#define TEGRABL_WDT_EXPIRY_2 0x2
#define TEGRABL_WDT_EXPIRY_3 0x4
#define TEGRABL_WDT_EXPIRY_4 0x8
#define TEGRABL_WDT_EXPIRY_5 0x10

/**
 * @brief enum specifying the wdt clock sources
 */
/* macro tegrabl wdt src */
typedef uint32_t tegrabl_wdt_clk_src_t;
#define TEGRABL_WDT_SRC_USECCNT 0U
#define TEGRABL_WDT_SRC_OSCCNT 1U
#define TEGRABL_WDT_SRC_TSCCNT_29_0 2U
#define TEGRABL_WDT_SRC_TSCCNT_41_12 3U
#define TEGRABL_WDT_SRC_MAX 4U

/**
 * @brief disable wdt
 *
 * @param instance wdt instance to be disabled
 */
void tegrabl_wdt_disable(tegrabl_wdt_instance_t instance);

/**
 * @brief program watchdog timer based on odm data bit and HALT_IN_FIQ bit
 *
 * @param instance wdt instance to be configured
 * @param expiry enable/disable corresponding expiries
 *		  bit 0 indicates first_expiry, bit1 indicates second_expiry and so on
 * @param period timer expiry period in seconds
 * @param clk_src specifies clk_src from enum tegrabl_wdt_clk_src
 *
 * @return TEGRABL_NO_ERROR if successful, else proper error code
 *
 */
tegrabl_error_t tegrabl_wdt_enable(tegrabl_wdt_instance_t instance,
								   uint8_t expiry, uint8_t period,
								   tegrabl_wdt_clk_src_t clk_src);

/**
 * @brief load wdt peroid and start counter / disable wdt
 * only if WDT configure conditions are met
 *
 * @param instance wdt instance to be disabled
 * @param period timer expiry period in seconds
 */
void tegrabl_wdt_load_or_disable(tegrabl_wdt_instance_t instance,
	uint8_t period);


#endif
