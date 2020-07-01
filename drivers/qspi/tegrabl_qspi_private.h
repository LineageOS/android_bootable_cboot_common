/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_QSPI_PRIVATE_H
#define INCLUDED_TEGRABL_QSPI_PRIVATE_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <tegrabl_qspi.h>
#include <tegrabl_gpcdma.h>

/* Wrapper macros for reading/writing from/to QSPI */
#define qspi_readl(reg) \
		NV_READ32(NV_ADDRESS_MAP_QSPI_BASE + (QSPI_##reg##_0))

#define qspi_writel(reg, val) \
		NV_WRITE32((NV_ADDRESS_MAP_QSPI_BASE + (QSPI_##reg##_0)), (val))

/* Disable compiler optimization locally to ensure read after write */
#define qspi_writel_flush(reg, val) \
do { \
	uint32_t reg32 = 0; \
	NV_WRITE32((NV_ADDRESS_MAP_QSPI_BASE + (QSPI_##reg##_0)), (val)); \
	reg32 = NV_READ32(NV_ADDRESS_MAP_QSPI_BASE + (QSPI_##reg##_0)); \
	reg32 = reg32; \
} while (0)

#define QSPI_MAX_BIT_LENGTH		31
#define QSPI_8Bit_BIT_LENGTH	7
#define QSPI_FIFO_DEPTH			64
#define BYTES_PER_WORD			4

/* Read time out of 1 second. */
#define QSPI_HW_TIMEOUT			1000000

/*
 * QSPI context structure
 */
struct tegrabl_qspi_ctxt {
	enum tegrabl_dmatype dma_type;
	tegrabl_gpcdma_handle_t dma_handle;
};

void qspi_dump_registers(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_TEGRABL_QSPI_PRIVATE_H */
