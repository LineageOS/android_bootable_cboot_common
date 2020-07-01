/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_QSPI_FLASH_PARAM_H
#define INCLUDED_TEGRABL_QSPI_FLASH_PARAM_H

/**
 * @brief Platform specific QSPI flash parameters to access QSPI flash.
 *
 * @clk_src: Clock Source. Select clock source from multiple clock input.
 *			 The valid values are (defined in tegrabl_clock.h):
 *					TEGRABL_CLK_SRC_PLLP_OUT0,
 *					TEGRABL_CLK_SRC_PLLC2_OUT0,
 *					TEGRABL_CLK_SRC_PLLC_OUT0,
 *					TEGRABL_CLK_PLL_ID_PLLC3,
 *					TEGRABL_CLK_SRC_PLLC4_MUXED,
 *					TEGRABL_CLK_SRC_CLK_M,
 *			Note that the valid input from above list may again depends on
 *			SOC to SoC so please refer the SoC data sheet for valid clock
 *			source from above list for given SoCs.
 * @clk_div: Clock divisor.
 * @clk_src_freq: Clock source frequency in KHz.
 * @interface_freq: Interface frequency in KHz for QSPI bus.
 * @max_bus_width: Maximum bus width
 *					0 for X1,
 *					2 for X4,
 * @enable_ddr_read: Enable DDR mode of QSPI flash access.
 *					0 for only SDR
 *					1 for SDR and DDR access.
 * @dma_type: Dma type whether BPMP or GPCDMA.
 *					0 for DMA_GPC
 *					1 for DMA_BPMP
 * @fifo_access_mode: QSPI Fifo access method whether PIO or PIO and DMA.
 *					0 for PIO only.
 *					1 for PIO and DMA.
 * @trimmer1_val: Tx clock tap delay.
 * @trimmer2_val: Rx clock tap delay.
 */
struct tegrabl_qspi_flash_platform_params {
	uint32_t clk_src;
	uint32_t clk_div;
	uint32_t clk_src_freq;
	uint32_t interface_freq;
	uint32_t max_bus_width;
	bool enable_ddr_read;
	uint32_t dma_type;
	uint32_t fifo_access_mode;
	uint32_t read_dummy_cycles;
	uint32_t trimmer1_val;
	uint32_t trimmer2_val;
};

#endif /* #ifndef INCLUDED_TEGRABL_QSPI_FLASH_PARAM_H */
