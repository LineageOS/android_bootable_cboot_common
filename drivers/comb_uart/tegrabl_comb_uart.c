/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_COMBINED_UART

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_drf.h>
#include <tegrabl_module.h>
#include <tegrabl_timer.h>
#include <tegrabl_comb_uart.h>
#include <tegrabl_io.h>
#include <tegrabl_debug.h>
#include <tegrabl_comb_uart_soc_local.h>

#define BIT(n) (1UL << (n))
#define MAX_BYTES_IN_PCKT 3U

static struct comb_uart_mailbox comb_uart;

/* Shared mailbox protocol bits */
#define NUM_BYTES_FIELD_BIT	(24U)
#define FLUSH_BIT		(26U)
#define HW_FLUSH_BIT		(27U)
#define INTR_TRIGGER_BIT	(31U)

/* RX/TX timeout */
#define UART_TIMEOUT_MS		(500U)

/*Return true if TX mailbox does not have tag bit set*/
static inline bool comb_uart_is_tx_empty(void)
{
	return (NV_READ32(comb_uart.tx_addr) & BIT(INTR_TRIGGER_BIT)) == 0U;
}

/* Wait for TX to clear, or until time out*/
static tegrabl_error_t comb_uart_wait_tx_empty(void)
{
	time_t start_time = tegrabl_get_timestamp_ms();
	time_t elapsed_time;
	tegrabl_error_t err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);

	do {
		if (comb_uart_is_tx_empty()) {
			err = TEGRABL_NO_ERROR;
			break;
		}

		elapsed_time = tegrabl_get_timestamp_ms() - start_time;
	} while (elapsed_time < UART_TIMEOUT_MS);

	return err;
}

/* polling 31st bit to check if SPE consumed the data */
static tegrabl_error_t comb_uart_wait_tx_ack(void)
{
	uint32_t reg_val;
	time_t start_time = tegrabl_get_timestamp_ms();
	time_t elapsed_time;
	tegrabl_error_t err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 1);

	do {
		reg_val = NV_READ32(comb_uart.tx_addr);
		if ((reg_val & BIT(INTR_TRIGGER_BIT)) == 0U) {
			err = TEGRABL_NO_ERROR;
			break;
		}

		elapsed_time = tegrabl_get_timestamp_ms() - start_time;
	} while (elapsed_time < UART_TIMEOUT_MS);

	return err;
}

static void comb_uart_rx_ack(uint32_t reg_val)
{
	/* ACK the RX by clearing 31st bit of Rx mailbox*/
	NV_WRITE32(comb_uart.rx_addr, reg_val & ~(BIT(INTR_TRIGGER_BIT)));
}

tegrabl_error_t tegrabl_comb_uart_rx(uint8_t *rx_buf, uint32_t len, uint32_t *bytes_received, time_t timeout)
{
	uint32_t reg_val;
	uint32_t num_chars;
	uint8_t i;
	uint32_t j = 0;
	time_t start_time = tegrabl_get_timestamp_ms();
	time_t elapsed_time;
	tegrabl_error_t err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 2);

	if ((rx_buf == NULL) || (bytes_received == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	*bytes_received = 0;

	do {
		/* read the mailbox register till trigger bit gets active */
		do {
			reg_val = NV_READ32(comb_uart.rx_addr);

			elapsed_time = tegrabl_get_timestamp_ms() - start_time;
			if (elapsed_time > timeout) {
				goto fail;
			}
		} while ((reg_val & BIT(INTR_TRIGGER_BIT)) == 0U);

		/* read how many characters are sent by reading 24th and 25th bit */
		num_chars = (reg_val >> NUM_BYTES_FIELD_BIT) & 0x3U;
		if (num_chars == 0U) { /*received no data*/
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
			goto fail;
		}

		/* read each byte and store in the buffer */
		for (i = 0; i < num_chars; i++) {
			rx_buf[j++] = (uint8_t)((reg_val >> (i * 8U)) & 0xFFU);
			/* check if requested bytes are received, break if yes */
			if (j == len) {
				comb_uart_rx_ack(reg_val);
				*bytes_received = j;
				err = TEGRABL_NO_ERROR;
				goto done;
			}
		}

		comb_uart_rx_ack(reg_val);

		elapsed_time = tegrabl_get_timestamp_ms() - start_time;

	} while (elapsed_time < timeout);

fail:
	if (j > 0U) { /* partial data is received */
		*bytes_received = j;
	}
done:
	return err;
}

tegrabl_error_t tegrabl_comb_uart_tx(const void *tx_buf, uint32_t len, time_t timeout)
{
	uint32_t num_packets, curr_packet_bytes, last_packet_bytes;
	uint32_t reg_val;
	uint32_t i, j;
	uint8_t *s = (uint8_t *)tx_buf;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(timeout);
	if (tx_buf == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	if (len == 0U) {
		goto fail; /*return TEGRABL_NO_ERROR*/
	}

	/* get number of packets and mark first or last packet */
	num_packets = len / MAX_BYTES_IN_PCKT;
	if ((len % MAX_BYTES_IN_PCKT) != 0U) {
		last_packet_bytes = len % MAX_BYTES_IN_PCKT;
		num_packets += 1U;
	} else {
		last_packet_bytes = MAX_BYTES_IN_PCKT;
	}

	/* Loop for processing each 3 char packet */
	for (i = 0; i < num_packets; i++) {
		reg_val = BIT(INTR_TRIGGER_BIT);

		if (i == (num_packets - 1U)) {
			reg_val |= BIT(FLUSH_BIT);
			curr_packet_bytes = last_packet_bytes;
		} else {
			curr_packet_bytes = MAX_BYTES_IN_PCKT;
		}

		/*
		 * Extract the current 3 chars from the string buffer (const char *s) and store them in the mailbox
		 * register value.
		 */
		reg_val |= curr_packet_bytes << NUM_BYTES_FIELD_BIT;
		for (j = 0; j < curr_packet_bytes; j++) {
			reg_val |= (uint32_t)(s[(i * MAX_BYTES_IN_PCKT) + j]) << (j * 8U);
		}

		/* Wait for TX mailbox to clear */
		err = comb_uart_wait_tx_empty();
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		/* Write packet to TX */
		NV_WRITE32(comb_uart.tx_addr, reg_val);

		/* Wait for ACK that the current packet is received at other end.*/
		err = comb_uart_wait_tx_ack();
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_comb_uart_hw_flush(void)
{
	tegrabl_error_t err;

	/* Initialize TX value */
	uint32_t tx = BIT(INTR_TRIGGER_BIT) | BIT(HW_FLUSH_BIT);

	/* Wait for SPE mailbox to clear */
	err = comb_uart_wait_tx_empty();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	if (err == TEGRABL_NO_ERROR) {
		/* Write to SPE mailbox */
		NV_WRITE32(comb_uart.tx_addr, tx);
		/* Wait for SPE mailbox to clear */
		err = comb_uart_wait_tx_empty();
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	return err;
}

/* combined uart driver init/probe */
tegrabl_error_t tegrabl_comb_uart_init(uint32_t mailbox_type)
{
	uint32_t tmp = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	comb_uart = comb_uart_get_mailbox_info(mailbox_type);

	pr_trace("comb_uart: mailbox: tx addr: 0x%08x, rx addr: 0x%08x\n", comb_uart.tx_addr, comb_uart.rx_addr);

	/* Clear mailbox registers */
	NV_WRITE32(comb_uart.tx_addr, 0);
	NV_WRITE32(comb_uart.rx_addr, 0);

	/* Write just a single newline as a probe test */
	tmp = 0x0AUL | BIT(NUM_BYTES_FIELD_BIT) | BIT(FLUSH_BIT) | BIT(INTR_TRIGGER_BIT);

	NV_WRITE32(comb_uart.tx_addr, tmp);

	err = comb_uart_wait_tx_ack();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Ready, clear the mailbox registers */
	NV_WRITE32(comb_uart.tx_addr, 0);

fail:
	return err;
}

tegrabl_error_t tegrabl_comb_uart_get_mailbox_port_addr(tegrabl_comb_uart_mailbox_port_t port,
														uint32_t *const addr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (addr == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (port) {

	case COMB_UART_MAILBOX_TX_PORT:
		*addr = comb_uart.tx_addr;
		break;

	case COMB_UART_MAILBOX_RX_PORT:
		*addr = comb_uart.rx_addr;
		break;

	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		break;
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_comb_uart_close(void)
{
	/*Dummy func, returning error msg type as expected by upper layer*/
	return TEGRABL_NO_ERROR;
}
