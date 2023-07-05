/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef _SMMU_HELPER_H
#define _SMMU_HELPER_H

#define BIT_FIELD(a, b)						\
         (((~((uint32_t)0)) - (((uint32_t)1) << (b)) + 1) &	\
         (~((uint32_t)0) >> (32 - 1 - (a))))

#define BIT_FIELD_64(a, b)					\
         (((~((uint64_t)0)) - (((uint64_t)1) << (b)) + 1) &	\
         (~((uint64_t)0) >> (64 - 1 - (a))))

#define BIT_RSH(a, b)						\
	({							\
		((a & b) >> ffs(b));				\
	})

static inline int fls(uint32_t x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static inline int ffs(uint32_t x)
{
        int r = 1;

        if (!x)
                return 0;
        if (!(x & 0xffff)) {
                x >>= 16;
                r += 16;
        }
        if (!(x & 0xff)) {
                x >>= 8;
                r += 8;
        }
        if (!(x & 0xf)) {
                x >>= 4;
                r += 4;
        }
        if (!(x & 3)) {
                x >>= 2;
                r += 2;
        }
        if (!(x & 1)) {
                x >>= 1;
                r += 1;
        }
        return r - 1;
}

static inline int ffs64(uint64_t x)
{
        int r = 1;

        if (!x)
                return 0;
        if (!(x & 0xffffffff)) {
                x >>= 32;
                r += 32;
        }
        if (!(x & 0xffff)) {
                x >>= 16;
                r += 16;
        }
        if (!(x & 0xff)) {
                x >>= 8;
                r += 8;
        }
        if (!(x & 0xf)) {
                x >>= 4;
                r += 4;
        }
        if (!(x & 3)) {
                x >>= 2;
                r += 2;
        }
        if (!(x & 1)) {
                x >>= 1;
                r += 1;
        }
        return r - 1;
}

static inline uint32_t set_bits(uint32_t bits, uint32_t val)
{
	uint32_t t = 0;

        t |= val << (ffs(bits));
        return t;
}

static inline uint64_t set_bits64(uint64_t bits, uint64_t val)
{
	uint64_t t = 0;

        t |= val << (ffs64(bits));
        return t;
}

#define dmb(opt)	asm volatile("dmb " #opt : : : "memory")
#define dma_wmb()	dmb(oshst)
#define wmb()		asm volatile("dmb ishst" ::: "memory")

#endif	/* _SMMU_HELPER_H */
