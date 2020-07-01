/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_clock.h>

struct tegrabl_qspi_handle;

/**
 * @brief QSPI controller and Flash chip operating mode.
 * SDR_MODE: 0 [IO on single edge of clock]
 * DDR_MODE: 1 [IO on rising and falling edge of clock]
 */
/* macro qspi op mode */
typedef uint32_t qspi_op_mode_t;
#define SDR_MODE 0U
#define DDR_MODE 1U

#define QSPI_MAX_INSTANCE       2U

#define QSPI_OP_MODE_SDR	0x0
#define QSPI_OP_MODE_DDR	0x4

/**
 * @brief QSPI signal modes for clock polarity and data edge.
 */
#define QSPI_SIGNAL_MODE_0	0x0
#define QSPI_SIGNAL_MODE_3	0x3

/**
 * @brief: QSPI FIFO access methods:
 *		ONLY_PIO:	 Only use the PIO mode for FIFO access
 *					 in given transfer list.
 *		BEST_EFFORT: Based on data transfer length and fifo depth,
 *					 use the PIO or DMA Transfers.
 */
#define QSPI_FIFO_ACCESS_MODE_ONLY_PIO			0x0
#define QSPI_FIFO_ACCESS_MODE_BEST_EFFORT		0x1

/**
 * @brief QSPI Bus Width Enumeration
 * QSPI_BUS_WIDTH_X1: Single IO mode.
 * QSPI_BUS_WIDTH_X2: Dual IO mode.
 * QSPI_BUS_WIDTH_X4: Quad IO mode.
 */
typedef uint32_t qspi_bus_width_t;
#define QSPI_BUS_WIDTH_X1 0U
#define QSPI_BUS_WIDTH_X2 1U
#define QSPI_BUS_WIDTH_X4 2U

#define QSPI_PACKET_BIT_LENGTH(n)				(((n) - 1) & 0x1FU)

/** Defines to make combined sequence address/data
 *@_x: Indicates interface bus width of CMD
 *			X1: 00 = Single bit mode (x1 mode)
 *			X2: 01 = Dual mode (x2 mode)
 *			X3: 10 = Quad mode (x4 mode) 11 = RSVD
 *			Valid values are X1, X2 and X4.
 * @_ddr: SDR or DRR mode. Valid values are SDR or DDR.
 * @_bits: Packet bit length. Valid values are  8, 16, 32.
*/
#define QSPI_COMBINED_SEQ(_x, _ddr, _bits)					\
			((((QSPI_BUS_WIDTH_##_x) & 0x3U) << 13)		|	\
			 (((QSPI_OP_MODE_##_ddr) & 0x1U) << 12)		|	\
			  ((QSPI_PACKET_BIT_LENGTH(_bits) << 0)))

/**
 * @brief struct tegrabl_qspi_transfer - QSPI Transfer Structure which
 *										 contains the information for
 *										 One transfer. Message consists
 *										 of multiple transfers.
 *
 * @tx_buff: Pointer to Transmit buffer. Must be word aligned so that
 *			same buffer can be used for DMA transfer also to have zero
 *			copy DMA.
 * @rx_buff: Pointer to Receive buffer. Must be word aligned so that
 *			same buffer can be used for DMA transfer also to have zero
 *			copy DMA.
 * @enable_combined_sequence: Tell whether current QSPI transfer is
 *			combined sequence or not. Value "true" is for combined
 *			sequence transfer.
 * @comb_address_seq: Combined address sequence information. This
 *			must be created using the macro QSPI_COMBINED_SEQ_ADDR().
 * @comb_data_seq: Combined data sequence information. This must
 *			be initialized using the macro QSPI_COMBINED_SEQ_DATA().
 * @comb_cmd_data: Combined sequence Data command.
 * @comb_cmd_address: Combined sequence Address.
 * @write_len: Number of bytes need to transmit. It can be non-multiple
 * 			of 4 also.
 * @read_len: Number of bytes need to receive. It can be non-multiple
 *			of 4 also.
 * @speed_hz: Clock rate of current data transfer in Hz. If it is zero
 *			then device specific speed will be configured.
 * @bus_width: QSPI IO mode like single, dual or quad. Must be one of
 *				QSPI_BUS_WIDTH_X1,
 *				QSPI_BUS_WIDTH_X2,
 *				QSPI_BUS_WIDTH_X4
 * @dummy_cycles: Sometimes, dummy clock cycles needed for some of
 *				serial flash commands. This adds the number of clocks
 *				after command.
 * @packet_bit_length: Number of bits per packet. Must be 8 or 16 or 32.
 *				Other packet bit length is not selected.
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
	qspi_op_mode_t op_mode;
	bool enable_combined_sequence;
	uint32_t comb_address_seq;
	uint32_t comb_data_seq;
	uint32_t comb_cmd_data;
	uint32_t comb_cmd_address;
	uint32_t packet_bit_length;
};

/**
 * @brief struct tegrabl_qspi_device_property- QSPI device specific
 *							property. This is filled by client based
 *							on what device is connected and passed to
 *							QSPI APIs when it opens the QSPI port.
 *
 * @spi_mode: SPI signal mode. Only MODE 0 and MODE 3 are supported.
 * 			Must be one of
 *					QSPI_SIGNAL_MODE_0 or
 *					QSPI_SIGNAL_MODE_3.
 * @cs_active_low: Boolean type, to tell whether CS is active low or
 *			HIGH. It is "true" when device is selected when CS
 *			line is LOW. Else it is "false".
 * @chip_select: Chip select ID where device is connected. The valid entry
 *			is 0 as of now as QSPI only support 1 CS. Still client drive
 *			need to pass 0 explicitly for any future extension.
 * @speed_hz: Normal interface clock rate of the data transfer in Hz. This
 *			is used when speed is not passed from the QSPI transfer.
 */
struct tegrabl_qspi_device_property {
	uint32_t spi_mode;
	bool cs_active_low;
	uint32_t chip_select;
	uint32_t speed_hz;
};

/**
 * @brief struct tegrabl_qspi_platform_params- QSPI platform specific
 *				parameters which have the platform specific properties
 *				for device interface, data transfer and internal
 *				configuration of the QSPI controller.
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
 * @clk_src_freq: Clock source frequency.
 * @interface_freq: Interface frequency.
 * @clk_div: Clock divisor value.
 * @max_bus_width: IO maximum bus width mode whether single, dual or quad
 *				   IO mode.
 *						QSPI_BUS_WIDTH_X1,
 *						QSPI_BUS_WIDTH_X2,
 *						QSPI_BUS_WIDTH_X4
 * @dma_type: DMA type which is used for FIFO access.
 *						0 for GPCDMA, 1 for BPMP DMA and 2 for SPE DMA.
 * @fifo_access_mode: Fifo Access mode,
 *						0 for PIO only.
 *						1 for DMA and PIO, Best effort.
 * @trimmer1_val: Trimmer 1 value for internal configuration of QSPI
 *				controller.
 * @trimmer2_val: Trimmer2 value for internal configurations of QSPI
 *				controller.
 */
struct tegrabl_qspi_platform_params {
	uint32_t clk_src;
	uint32_t clk_src_freq;
	uint32_t interface_freq;
	uint32_t clk_div;
	uint32_t max_bus_width;
	uint32_t dma_type;
	uint32_t fifo_access_mode;
	uint32_t trimmer1_val;
	uint32_t trimmer2_val;
};

/*
 * @brief enum values are based on the clk_source_qspi register fields
 */
/*macro tegrabl qspi clk src */
typedef uint32_t tegrabl_qspi_clk_src_t;
#define QSPI_CLK_SRC_PLLP_OUT0 0x0
#define QSPI_CLK_SRC_PLLC2_OUT0 0x1
#define QSPI_CLK_SRC_PLLC_OUT0 0x2
#define QSPI_CLK_SRC_PLLC3_OUT0 0x3
#define QSPI_CLK_SRC_PLLC4_MUXED 0x4
#define QSPI_CLK_SRC_CLK_M 0x6

/*
 * @brief enum values specifies the mode being used to transfer
 */
/* macro tegrabl qspi xfer mode */
typedef uint32_t tegrabl_qspi_xfer_mode_t;
#define QSPI_MODE_PIO 0
#define QSPI_MODE_DMA 1

/**
 * @brief QSPI clk structure specifies source and frequency to be configured
 */
struct qspi_clk_data {
	tegrabl_clk_src_id_t clk_src;
	uint32_t clk_divisor;
};

/**
 * @brief: Initialize the QSPI controller for data transfer like setting
 * pinmux, clock rates etc, allocate the qspi handle to caller so that
 * caller can pass this to rest of  QSPI calls.
 *
 * Caller also passes the device specific information which is used
 * by QSPI internal driver for housekeeping and data transfers.
 *
 * QSPI driver keep the copy of the device parameter inside the driver
 * and so caller can remove the content of passed device property.
 *
 * @instance: Instance ID of QSPI. It starts from 0. Hence it is 0
 *			for QSPI0 and 1 for QSPI1. The valid values depends on
 *			SoC.
 *
 * @qspi_device: Pointer to the device specific property structure.
 *			Caller need to fill the information and pass the pointer of this
 *			structure to QSPI driver. QSPI driver gets the generic property of
 *			the device from this structure for future reference. QSPI driver
 *			keeps the copy of this information internally for future usage.
 *
 * @qspi_params: QSPI platform specific parameters.
 *
 * @qspi_handle: QSPI driver returns the QSPI handle to the caller. Caller
 *			need to pass this handle for rest of QSPI APIs. The content
 *			of this handle is not accessed by the caller. The information
 *			contained by this handle is internal to QSPI driver. Whenever
 * QSPI client driver calls the QSPI APIs, it passes this handle and QSPI
 * driver gets the information about the QSPI instance, device properties,
 * transfer status etc.
 *
 * This function returns following values:
 * NO_ERROR: Operation is success and there is no error found on QSPI
 *			channel allocation.
 * BUSY: Channel is already allocated.
 * INVALID_PARAMS: The passed information are not correct like invalid
 *			instance ID or device property.
 */
tegrabl_error_t tegrabl_qspi_open(uint32_t instance,
						  struct tegrabl_qspi_device_property *qspi_device,
						  struct tegrabl_qspi_platform_params *params,
						  struct tegrabl_qspi_handle **qspi_handle);

/**
 * @brief: Close QSPI handle and releases all allocated resources.
 *			After this call, client driver can again get the QSPI channel
 *			for same device property.
 *
 * @qspi_handle: Pointer to the QSPI handle which is allocated by call
 *			tegrabl_qspi_get().
 *
 * This function returns following values:
 * NO_ERROR: Operation is success
 * BUSY: Transfer is in progress.
 */
tegrabl_error_t tegrabl_qspi_close(struct tegrabl_qspi_handle *qspi_handle);

/**
 * @brief Performs Qspi transaction write and read
 *		  This API starts QSPI transfers. QSPI Driver activates chip select
 *			before transfer and keeps in active state till all transfer is
 *			done. Client can pass multiple list of transfers to have multiple
 *			transfer in one call under one CS state change.
 *
 *			If CS needs to toggle between transfers then client need to split
 *			in different list and call this APIs multiple times.
 *
 *			The transfer status is checked using polling method.
 *			This API is synchronous and asynchronous in nature. The behavior
 *			depends on the value of parameter timeout_us.
 *				- If it is zero then it is asynchronous in nature. The APIs
 *				  configure the QSPI for transfer and return to the caller
 *				  without waiting for the transfer completes. In this case,
 *				  it is caller responsibility to check transfer status
 *				  frequently. As there is no other context available to
 *				  reconfigure QSPI controller for next transfer, it is only
 *				  done when caller calls the other asynchronous related APIs.
 *				  When caller calls the API then transfer status is checked
 *				  and if controller is idle then reconfigure for next remaining
 *				  transfers.
 *			   -  If timeout is non-zero then QSPI will start time counting
 *				  and do polling for transfer completes for all requested
 *				  transfers.
 *					* If timeout happen before all transfer completes then it
 *					  abort transfer and return error to the caller.
 *				    * If all transfers completes before timeout then it return
 *					  success to the caller.
 *					* If any error occurs during transfer then abort transfer
 *					  and return error to the caller.
 *
 *			QSPI driver does not keep copy of the transfer in case of
 *			asynchronous and so caller need to preserve the information till
 *			all transfer completes.
 *
 * @qspi_handle: Pointer to the QSPI handle which is allocated by call
 *			tegrabl_qspi_open().
 * @p_transfer: Array of transfer list to do the QSPI transfer. Each transfer
 *			structure has information about the type of data transfers like
 *			direction, command sequence etc.
 *			QSPI transfer is done in sequence the way it is passed in the
 *			transfer list. All transfers are done in single chip-select active
 *			state.
 * @no_of_transfers: Number of transfers list in the transfer parameters.
 * @timeout_us: Timeout in microseconds. If zero then behavior is asynchronous
 *			type.
 *
 * This function returns following values:
 *		NO_ERROR: Operation is success and there is no error found on
 *				QSPI transfer.
 *				For Asynchronous call, NO_ERROR means transfer started
 *				successfully. For Synchronous call, NO_ERROR means all
 *				transfer completed successfully.
 *		BUSY: Transfer is already in progress and no more transfer can
 *			  be accepted.
 *		INVALID_PARAMS: The passed information is not like non-aligned
 *				transfer buffer or invalid speed, bit packet length,
 *				command sequence information etc.
 */
tegrabl_error_t
tegrabl_qspi_transaction(
	struct tegrabl_qspi_handle *qspi_handle,
	struct tegrabl_qspi_transfer *p_transfers,
	uint8_t no_of_transfers, uint32_t timeout_us);

/**
 * @brief: tegrabl_qspi_check_transfer_status() - This API waits for the
 *				ongoing transfer to be completed. The timeout in microseconds
 *				is provided by the caller to define the wait time. Value of
 *				parameter timeout_us and is_abort decide the behavior of the
 *				API.
 *
 *	If timeout_us =0 and is_abort = false:
 *			  - If controller is idle after transfer and waiting for next
 *				configuration then it configures for next transfer and return
 *				to caller as BUSY.
 *			  -	If controller is still busy for transfer then it returns BUSY.
 *			  - If all transfer is completed and controller is idle then return
 *				TRANSFER_DONE. In this case, new transfer can be started.
 * If timeout_us = 0 and is_abort = true;
 *			  - If controller is idle after transfer and waiting for next
 *				configuration then it aborts the transfer, make QSPI in idle
 *				state for next transfer and return the ABORT to the caller.
 *			  - If controller is still busy for transfer then it stops
 *				transfer, make controller status to idle so that new
 *				transfer can be started and return ABORT to the caller.
 *			  -	If all transfer is completed and controller is idle then
 *				return TRANSFER_DONE. New transfer can be started after this.
 * If timeout_us = non-zero and is_abort = false
 *			 -	If controller is idle after transfer and waiting for next
 *				configuration for remaining transfer then it configures
 *				for next transfer and waits for the timeout for completion.
 *			 -  If all transfer completed before timeout then it returns
 *				TRANSFR_DONE.
 *			 -  If transfer does not complete in given timeout then returns
 *				TIMEOUT. It will not abort the transfer.
 *			 -  If controller is still busy in current transfer then it waits
 *				for the timeout for completion.
 *			 		  - If all transfer completed before timeout then it returns
 *						TRANSFR_DONE.
 *					  - If transfer does not complete in given timeout then
 *						returns TIMEOUT. It will not abort the transfer.
 *					  - If all transfer is completed and controller is idle
 *						then return TRANSFER_DONE. In this case, new transfer
 *						can be started.
 *
 * If timeout_us = non-zero and is_abort = true
 * 			  - If controller is idle after transfer and waiting for next
 *				configuration for remaining transfer then it configures for
 *				next transfer and waits for the timeout for completion.
 *			  - If all transfer completed before timeout then it returns
 *				TRANSFR_DONE.
 *			  - If transfer does not complete in given timeout then it
 *				aborts transfer, make QSPI controller to idle state for
 *				next transfer start and returns TIMEOUT. New transfer
 *				can be started after this.
 *			  - If controller is still busy in current transfer then it waits
 *				for the timeout for completion.
 *					  - If all transfer completed before timeout then it
 *						returns TRANSFR_DONE.
 *					  - If transfer does not complete in given timeout then
 *						it aborts transfer, make QSPI controller to idle state
 *						for next transfer start and returns TIMEOUT. New
 *						transfer can be started after this.
 *					  - If all transfer is completed and controller is idle
 *						then return TRANSFER_DONE. In this case, new transfer
 *						can be started.
 *
 * Pre-requisites: API tegrabl_qspi_start_transfer() has to be called with
 *				   zero timeout.
 * @qspi_handle: Pointer to the QSPI handle which is allocated by call
 *			tegrabl_qspi_get().
 * @timeout_us: Timeout in microseconds.
 * @is_abort: Transfer need to be aborted or not before returning from
 *			APIs if transfer is in progress. If true then it aborts the
 *			transfer.
 *
 * This function returns following values:
 *		NO_ERROR: Operation is success and there is no error found on QSPI
 *		TRANSFER_DONE: When all transfer completed.
 */
tegrabl_error_t tegrabl_qspi_check_transfer_status(
						struct tegrabl_qspi_handle *qspi_handle,
						uint32_t timeout_us, bool is_abort);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_TEGRABL_QSPI_H */
