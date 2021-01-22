/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <err.h>
#include <ctype.h>
#include <printf.h>
#include <tegrabl_debug.h>
#include <tegrabl_spi.h>
#include <tegrabl_spi_test.h>

static struct tegrabl_spi *hspi;
static tegrabl_error_t
spi_init(void)
{
	struct tegrabl_spi_endianess endianness;
	uint32_t mode;
	uint32_t bus;
	uint32_t freq;
	tegrabl_error_t ret;

	mode = SPI_BUS_MODE;
	bus = SPI_BUS_NUM;
	freq = SPI_BUS_FREQ_KHZ;
	hspi = tegrabl_spi_open(bus, freq);
	if (hspi == NULL)
		return TEGRABL_ERR_INVALID;
	pr_debug("hspi<0x%x 0x%x 0x%x>\n", hspi->instance, hspi->base_addr,
		 hspi->freq_khz);
	/* set the mode via ioctl call */
	/* speed/frequencey is already set during open */
	/* set endianness  */
	endianness.is_lsbit_first = false;
	endianness.is_lsbyte_first = false;
	ret = tegrabl_spi_ioctl(hspi, SPI_IOCTL_SET_ENDIANESS, &endianness);
	if (ret != TEGRABL_NO_ERROR)
		return TEGRABL_ERR_INVALID;
	/* setting the mode */
	ret = tegrabl_spi_ioctl(hspi, SPI_IOCTL_SET_MODE, &mode);
	if (ret != TEGRABL_NO_ERROR)
		return TEGRABL_ERR_INVALID;
	return ret;
}

static tegrabl_error_t
spi_flash_scan(void)
{
	uint8_t din[SPI_BUF_LEN_RD];
	struct tegrabl_spi_transfer transfer;
	tegrabl_error_t ret;
	uint8_t dout[SPI_BUF_LEN_WR];

	/*command to read windbond flash ID */
	dout[0] = SPI_FLASH_ID_RD_CMD;
	transfer.tx_buf = &dout[0];
	transfer.rx_buf = &din[0];
	transfer.write_len = 1;
	transfer.read_len = 3;

	ret = tegrabl_spi_transaction(hspi, &transfer, 1);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("Error in tegrabl_spi_transaction!\n");
		return TEGRABL_ERR_INVALID;
	}
	printf("SF: Got idcodes,mID=0x%x,pID=0x%x,d[2]=0x%x\n", din[0], din[1],
	       din[2]);
	return ret;
}

tegrabl_error_t
do_spi_flash_probe(void)
{
	tegrabl_error_t ret;
	static bool initialized;
	if (initialized == false) {
		ret = spi_init();
		if (ret != TEGRABL_NO_ERROR) {
			pr_error("spi_init error\n");
			return TEGRABL_ERR_INVALID;
		}
		initialized = true;
	}
	ret = spi_flash_scan();
	if (ret != TEGRABL_NO_ERROR)
		return TEGRABL_ERR_INVALID;
	return ret;
}
