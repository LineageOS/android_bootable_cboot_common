/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All Rights Reserved.
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

#undef NV_WRITE8
#define NV_WRITE8(a,d)      *((volatile uint8_t *)(uintptr_t)(a)) = (d)

#undef NV_WRITE16
#define NV_WRITE16(a,d)     *((volatile uint16_t *)(uintptr_t)(a)) = (d)

#undef NV_WRITE32
#define NV_WRITE32(a,d)     *((volatile uint32_t *)(uintptr_t)(a)) = (d)

#undef NV_WRITE64
#define NV_WRITE64(a,d)     *((volatile uint64_t *)(uintptr_t)(a)) = (d)

#undef NV_READ8
#define NV_READ8(a)         *((const volatile uint8_t *)(uintptr_t)(a))

#undef NV_READ16
#define NV_READ16(a)        *((const volatile uint16_t *)(uintptr_t)(a))

#undef NV_READ32
#define NV_READ32(a)        *((const volatile uint32_t *)(uintptr_t)(a))

#undef NV_READ64
#define NV_READ64(a)        *((const volatile uint64_t *)(uintptr_t)(a))

#define NV_WRITE32_FENCE(a, d)	\
	do {						\
		uint32_t reg;			\
		NV_WRITE32(a, d);		\
		reg = NV_READ32(a);		\
		reg = reg;				\
	} while (0)

#define REG_ADDR(block, reg)		(NV_ADDRESS_MAP_##block##_BASE + reg##_0)
#define REG_READ(block, reg)			NV_READ32(REG_ADDR(block, reg))
#define REG_WRITE(block, reg, value)	NV_WRITE32(REG_ADDR(block, reg), value)

#endif // INCLUDED_TEGRABL_IO_H
