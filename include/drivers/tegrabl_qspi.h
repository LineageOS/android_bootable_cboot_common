/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_QSPI_H
#define INCLUDED_TEGRABL_QSPI_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_gpcdma.h>
#include <tegrabl_mb1_bct.h>
#include <tegrabl_clock.h>

/* QSPI controller and Flash chip operating mode.
 * SDR_MODE: 0 [IO on single edge of clock]
 * DDR_MODE: 1 [IO on rising and falling edge of clock]
 */
enum qspi_op_mode {
	SDR_MODE = 0,
	DDR_MODE
};

/* Qspi Transfer Structure
 *    Message consists of multiple transfers
 */
struct tegrabl_qspi_transfer {
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint16_t mode;
	uint32_t write_len;
	uint32_t read_len;
	uint32_t speed_hz;
	uint32_t bus_width;
	uint32_t dummy_cycles;
	enum qspi_op_mode op_mode;
};

/*
 * @brief enum values are based on the clk_source_qspi register fields
 */
enum tegrabl_qspi_clk_src {
	QSPI_CLK_SRC_PLLP_OUT0 = 0x0,
	QSPI_CLK_SRC_PLLC4_MUXED = 0x4,
	QSPI_CLK_SRC_CLK_M = 0x6,
};

/*
 * @brief enum values specifies the mode being used to transfer
 */
enum tegrabl_qspi_xfer_mode {
	QSPI_MODE_PIO,
	QSPI_MODE_DMA,
};

/**
 * @brief QSPI Bus Width Enumeration
 */
enum qspi_bus_width {
	QSPI_BUS_WIDTH_X1,
	QSPI_BUS_WIDTH_X2,
	QSPI_BUS_WIDTH_X4
};

/**
 * @brief QSPI clk structure specfies source and frequency to be configured
 */
struct qspi_clk_data {
	enum tegrabl_clk_src_id_t clk_src;
	uint32_t clk_divisor;
};

/**
 * @brief Initializes the given Qspi controller
 *
 * @param params a pointer to struct tegrabl_mb1bct_qspi_params
 *
 * @retval TEGRABL_NO_ERROR Initialization is successful.
 */
tegrabl_error_t tegrabl_qspi_open(
				struct tegrabl_mb1bct_qspi_params *params);

/**
 * @brief Performs Qspi transaction write and read
 *
 * @param p_transfers address of array of qspi transfers
 *        Each message consists of multiple transfers
 *        For QSPI Flash - <CMD><ADDRESS><DATA>
 * @param no_of_transfers Number of transfers
 *
 * @retval TEGRABL_NO_ERROR No err
 */
tegrabl_error_t
tegrabl_qspi_transaction(
	struct tegrabl_qspi_transfer *p_transfers,
	uint8_t no_of_transfers);

/**
 * @brief Re-initializes the given Qspi controller
 *
 * @param params a pointer to struct tegrabl_mb1bct_qspi_params
 *
 * @retval TEGRABL_NO_ERROR Initialization is successful.
 */
tegrabl_error_t tegrabl_qspi_reinit(struct tegrabl_mb1bct_qspi_params *params);

/**
 * @brief Set Qspi_clk divisor mode for read operation
 *
 * @param val 0:SDR mode
 *            1:DDR mode
 *
 * @retval TEGRABL_NO_ERROR No err
 */
tegrabl_error_t tegrabl_qspi_clk_div_mode(uint32_t val);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_TEGRABL_QSPI_H */
