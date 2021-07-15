/*
 * Copyright (c) 2015-2021, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_ARMV8A_H
#define INCLUDED_ARMV8A_H

/* CPSR/SPSR definitions */
#define DAIF_FIQ_BIT	(1 << 0)
#define DAIF_IRQ_BIT	(1 << 1)
#define DAIF_ABT_BIT	(1 << 2)
#define DAIF_DBG_BIT	(1 << 3)
#define SPSR_DAIF_SHIFT	6
#define SPSR_DAIF_MASK	0xf

#define SPSR_AIF_SHIFT	6
#define SPSR_AIF_MASK	0x7

#define SPSR_E_SHIFT	9
#define SPSR_E_MASK	0x1
#define SPSR_E_LITTLE	0x0
#define SPSR_E_BIG	0x1

#define SPSR_T_SHIFT	5
#define SPSR_T_MASK	0x1
#define SPSR_T_ARM	0x0
#define SPSR_T_THUMB	0x1

#define DISABLE_ALL_EXCEPTIONS \
	(DAIF_FIQ_BIT | DAIF_IRQ_BIT | DAIF_ABT_BIT | DAIF_DBG_BIT)

#define MODE_SP_SHIFT	0x0
#define MODE_SP_MASK	0x1
#define MODE_SP_EL0	0x0
#define MODE_SP_ELX	0x1

#define MODE_RW_SHIFT	0x4
#define MODE_RW_MASK	0x1
#define MODE_RW_64	0x0
#define MODE_RW_32	0x1

#define MODE_EL_SHIFT	0x2
#define MODE_EL_MASK	0x3
#define MODE_EL3	0x3
#define MODE_EL2	0x2
#define MODE_EL1	0x1
#define MODE_EL0	0x0

#define MODE32_SHIFT	0
#define MODE32_MASK	0xf
#define MODE32_usr	0x0
#define MODE32_fiq	0x1
#define MODE32_irq	0x2
#define MODE32_svc	0x3
#define MODE32_mon	0x6
#define MODE32_abt	0x7
#define MODE32_hyp	0xa
#define MODE32_und	0xb
#define MODE32_sys	0xf

#define A5X_L2ECTLR_ECC_PARITY_ENABLE_BIT	21

#define NV_ID_AFR0_NVCACHE_OPS_MASK			(0xf << 12)

#if !defined(_ASSEMBLY_)

static inline void tegrabl_dsb(void)
{
	asm volatile ("dsb sy" : : : "memory", "cc");
}

static inline void tegrabl_isb(void)
{
	asm volatile ("isb" : : : "memory", "cc");
}

static inline void tegrabl_nop(void)
{
	asm volatile ("nop" : : : "memory", "cc");
}

static inline void tegrabl_yield(void)
{
	asm volatile ("yield" : : : "memory", "cc");
}

static inline uint32_t tegrabl_read_midr(void)
{
	uint32_t reg;
	asm volatile ("mrs %0, midr_el1" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline uint64_t tegrabl_read_mpidr(void)
{
	uint64_t reg;
	asm volatile ("mrs %0, mpidr_el1" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline uint64_t tegrabl_read_par(void)
{
	uint64_t reg;
	asm volatile ("mrs %0, par_el1" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline uint64_t tegrabl_read_id_afr0(void)
{
	uint64_t reg;
	asm volatile ("mrs %0, id_afr0_el1" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline void tegrabl_enable_serror(void)
{
	asm volatile ("msr daifclr, #4" : : : "memory", "cc");
}

static inline void tegrabl_disable_serror(void)
{
	asm volatile ("msr daifset, #4" : : : "memory", "cc");
}

static inline uint32_t tegrabl_read_a5x_l2ctlr(void)
{
	uint32_t reg;
	asm volatile ("mrs %0, s3_1_c11_c0_2" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline void tegrabl_at_s1e2r(uint64_t va)
{
	asm volatile ("at s1e2r, %0" : : "r"(va) : "memory", "cc");
}

static inline uint32_t tegrabl_nv_dcache_clean(void)
{
	uint32_t reg;
	asm volatile ("mrs %0, s3_0_c15_c3_5" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline uint32_t tegrabl_nv_dcache_flush(void)
{
	uint32_t reg;
	asm volatile ("mrs %0, s3_0_c15_c3_6" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline uint32_t tegrabl_nv_cache_flush(void)
{
	uint32_t reg;
	asm volatile ("mrs %0, s3_0_c15_c3_7" : "=r"(reg) : : "memory", "cc");
	return reg;
}

static inline void tegrabl_write_nvg_channel_idx(uint32_t channel)
{
	asm volatile ("msr s3_0_c15_c1_2, %0" : : "r"(channel) : "memory", "cc");
}

static inline void tegrabl_write_nvg_channel_data(uint64_t data)
{
	asm volatile ("msr s3_0_c15_c1_3, %0" : : "r"(data) : "memory", "cc");
}

static inline uint64_t tegrabl_read_nvg_channel_data(void)
{
	uint64_t reg;
	asm volatile ("mrs %0, s3_0_c15_c1_3" : "=r"(reg) : : "memory", "cc");
	return reg;
}

/**
 * @brief Infinite loop.
 */
static inline void tegrabl_hang(void)
{
    asm volatile ("b ." : : : "memory", "cc");
}


#endif /* !defined(_ASSEMBLY_) */

#endif /* INCLUDED_ARMV8A_H */
