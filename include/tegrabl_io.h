/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_IO_H
#define INCLUDED_TEGRABL_IO_H

#include <stdint.h>
#include <tegrabl_compiler.h>

/* For GCC, result of composite expression of two values of uint8_t OR uint16_t is of type int.
 * So don't typecast d, which can be composite expression, using typeof for NV_WRITE8 and NV_WRITE16.
 */

#undef NV_WRITE8
#define NV_WRITE8(a, d)      *((volatile uint8_t *)(uintptr_t)(typeof (a))(a)) = (uint8_t)(d)

#undef NV_WRITE16
#define NV_WRITE16(a, d)     *((volatile uint16_t *)(uintptr_t)(typeof (a))(a)) = (uint16_t)(d)

#undef NV_WRITE32
#define NV_WRITE32(a, d)     *((volatile uint32_t *)(uintptr_t)(typeof (a))(a)) = (uint32_t)(typeof (d))(d)

#undef NV_WRITE64
#define NV_WRITE64(a, d)     *((volatile uint64_t *)(uintptr_t)(typeof (a))(a)) = (uint64_t)(typeof (d))(d)

#undef NV_READ8
#define NV_READ8(a)         *((const volatile uint8_t *)(uintptr_t)(typeof (a))(a))

#undef NV_READ16
#define NV_READ16(a)        *((const volatile uint16_t *)(uintptr_t)(typeof (a))(a))

#undef NV_READ32
#define NV_READ32(a)        *((const volatile uint32_t *)(uintptr_t)(typeof (a))(a))

#undef NV_READ64
#define NV_READ64(a)        *((const volatile uint64_t *)(uintptr_t)(typeof (a))(a))

#define NV_WRITE32_FENCE(a, d)	\
	do {						\
		uint32_t reg;			\
		NV_WRITE32(a, d);		\
		reg = NV_READ32(a);		\
		reg = reg;				\
	} while (false)

#define REG_ADDR(block, reg)		((uint32_t)NV_ADDRESS_MAP_##block##_BASE + (uint32_t)reg##_0)
#define REG_READ(block, reg)			NV_READ32(REG_ADDR(block, reg))
#define REG_WRITE(block, reg, value)	NV_WRITE32(REG_ADDR(block, reg), value)

/**
 * @brief Debug function that acts as wrapper on top of NV_WRITE32 and allows
 * conditionally logging (i.e. if TEGRABL_TRACE_REG_RW is defined in including
 * source file) the address/value of the register being written to.
 *
 * @param addr address of the register being written
 * @param val 32bit data to be written to the register
 */
static TEGRABL_INLINE void tegrabl_trace_write32(volatile uint32_t addr,
		uint32_t val)
{
#if defined(TEGRABL_TRACE_REG_RW)
	pr_debug("%s: [0x%08"PRIx32"] <= 0x%08"PRIx32"\n", __func__, addr, val);
#endif
	NV_WRITE32(addr, val);
}

/**
 * @brief Debug function that acts as wrapper on top of NV_READ32 and allows
 * conditionally logging (i.e. if TEGRABL_TRACE_REG_RW is defined in including
 * source file) the address/value of the register being read from.
 *
 * @param addr address of the register being read
 *
 * @return 32bit data read from the register
 */
static TEGRABL_INLINE uint32_t tegrabl_trace_read32(volatile uint32_t addr)
{
	uint32_t val = NV_READ32(addr);
#if defined(TEGRABL_TRACE_REG_RW)
	pr_debug("%s: [0x%08"PRIx32"] <= 0x%08"PRIx32"\n", __func__, addr, val);
#endif
	return val;
}

#endif // INCLUDED_TEGRABL_IO_H
