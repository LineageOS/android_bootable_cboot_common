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

#ifndef _TEGRABL_SMMUV2_REG_H
#define _TEGRABL_SMMUV2_REG_H

#define MAX_SMR (0xA00U-0x800U)/4U
#define MAX_S2CR (0xE00U-0xC00U)/4U
#define MAX_SID 128U

uint32_t sid_to_smr_map[MAX_SID];
uint32_t const smr_mask = 0x7f800000;
uint32_t const cb_base;

struct SMMU {
	uint64_t pagesize;
	uint32_t base;
	uint32_t size;
	uint32_t nr_pages;
	uint32_t nr_cb;
	uint32_t *cbs;
	void *table_base;
	uint32_t nr_smr;
	uint32_t *smr;
	uint32_t in_addr_size;
} smmu;

struct priv_data {
	uint32_t cb;
	uint32_t sid;
	void *table_base;
};

struct GRS0_offset {
	const uint32_t CR0;
	const uint32_t CR1;
	const uint32_t CR2;
	const uint32_t ACR;
	const uint32_t IDR0;
	const uint32_t IDR1;
	const uint32_t IDR2;
	const uint32_t IDR3;
	const uint32_t IDR4;
	const uint32_t IDR5;
	const uint32_t IDR6;
	const uint32_t IDR7;
	const uint32_t GFAR;
	const uint32_t GFSR;
	const uint32_t GFSYNR0;
	const uint32_t GFSYNR1;
	const uint32_t GFSYNR2;
	const uint32_t TLBIVMID;
	const uint32_t TLBIALLNSNH;
	const uint32_t TLBIALLH;
	const uint32_t TLBIVAH;
	const uint32_t TLBIVALH64;
	const uint32_t TLBIVAH64;
	const uint32_t TLBGSYNC;
	const uint32_t TLBGSTATUS;
	const uint32_t GATS1UR;
	const uint32_t GATSR;
	const uint32_t TLBIVMIDS1;
	const uint32_t NSCR0;
	const uint32_t NSCR2;
	const uint32_t NSACR;
	const uint32_t SMR;
	const uint32_t SMR127;
	const uint32_t S2CR;
} grs0_offs = {
	0x000U,
	0x004U,
	0x008U,
	0x010U,
	0x020U,
	0x024U,
	0x028U,
	0x02CU,
	0x030U,
	0x034U,
	0x038U,
	0x03CU,
	0x040U,
	0x048U,
	0x050U,
	0x054U,
	0x058U,
	0x064U,
	0x068U,
	0x06CU,
	0x078U,
	0x0B0U,
	0x0C0U,
	0x070U,
	0x074U,
	0x100U,
	0x188U,
	0x0B8U,
	0x400U,
	0x408U,
	0x410U,
	0x800U,
	0x9FCU,
	0xC00U
};

struct GFSR {
	uint32_t const MULTI;
	uint32_t const RSVD;
	uint32_t const UUT;
	uint32_t const PF;
	uint32_t const EF;
	uint32_t const CAF;
	uint32_t const UCIF;
	uint32_t const UCBF;
	uint32_t const SMCF;
	uint32_t const USF;
	uint32_t const ICF;
} gfsr = {
	BIT_FIELD(31, 31),
	BIT_FIELD(30, 9),
	BIT_FIELD(8, 8),
	BIT_FIELD(7, 7),
	BIT_FIELD(6, 6),
	BIT_FIELD(5, 5),
	BIT_FIELD(4, 4),
	BIT_FIELD(3, 3),
	BIT_FIELD(2, 2),
	BIT_FIELD(1, 1),
	BIT_FIELD(0, 0)
};

struct FSR {
	uint32_t const MULTI;
	uint32_t const SS;
	uint32_t const FORMAT;
	uint32_t const UUT;
	uint32_t const ASF;
	uint32_t const TLBLKF;
	uint32_t const TLBMCF;
	uint32_t const EF;
	uint32_t const PF;
	uint32_t const AFF;
	uint32_t const TF;
} fsr = {
	BIT_FIELD(31, 31),
	BIT_FIELD(30, 30),
	BIT_FIELD(10, 9),
	BIT_FIELD(8, 8),
	BIT_FIELD(7, 7),
	BIT_FIELD(6, 6),
	BIT_FIELD(5, 5),
	BIT_FIELD(4, 4),
	BIT_FIELD(3, 3),
	BIT_FIELD(2, 2),
	BIT_FIELD(1, 1)
};

struct CR0 {
	uint32_t const NSCFG;
	uint32_t const WACFG;
	uint32_t const RACFG;
	uint32_t const SHCFG;
	uint32_t const SMCFCFG;
	uint32_t const MTCFG;
	uint32_t const MEMATTR;
	uint32_t const BSU;
	uint32_t const FB;
	uint32_t const PTM;
	uint32_t const VMIDPNE;
	uint32_t const USFCFG;
	uint32_t const GSE;
	uint32_t const STALLD;
	uint32_t const TRANSIENTCFG;
	uint32_t const GCFGFIE;
	uint32_t const GCFGFRE;
	uint32_t const EXIDENABLE;
	uint32_t const GFIE;
	uint32_t const GFRE;
	uint32_t const CLIENTPD;
} cr0 = {
	BIT_FIELD(29, 28),
	BIT_FIELD(27, 26),
	BIT_FIELD(25, 24),
	BIT_FIELD(23, 22),
	BIT_FIELD(21, 21),
	BIT_FIELD(20, 20),
	BIT_FIELD(19, 16),
	BIT_FIELD(15, 14),
	BIT_FIELD(13, 13),
	BIT_FIELD(12, 12),
	BIT_FIELD(11, 11),
	BIT_FIELD(10, 10),
	BIT_FIELD(9, 9),
	BIT_FIELD(8, 8),
	BIT_FIELD(7, 6),
	BIT_FIELD(5, 5),
	BIT_FIELD(4, 4),
	BIT_FIELD(3, 3),
	BIT_FIELD(2, 2),
	BIT_FIELD(1, 1),
	BIT_FIELD(0, 0)
};

struct CR1 {
	uint32_t const NSNUMSMRGO;
	uint32_t const NSNUMCBO;
} cr1 = {
	BIT_FIELD(15, 8),
	BIT_FIELD(7, 0)
};

struct IDR0 {
	uint32_t const SES;
	uint32_t const S1TS;
	uint32_t const S2TS;
	uint32_t const NTS;
	uint32_t const SMS;
	uint32_t const ATOSNS;
	uint32_t const PTFS;
	uint32_t const NUMIRPT;
	uint32_t const EXSMRGS;
	uint32_t const CTTW;
	uint32_t const BTM;
	uint32_t const NUMSIDB;
	uint32_t const EXIDS;
	uint32_t const NUMSMRG;
} idr0 = {
	BIT_FIELD(31, 31),
	BIT_FIELD(30, 30),
	BIT_FIELD(29, 29),
	BIT_FIELD(28, 28),
	BIT_FIELD(27, 27),
	BIT_FIELD(26, 26),
	BIT_FIELD(25, 24),
	BIT_FIELD(23, 16),
	BIT_FIELD(15, 15),
	BIT_FIELD(14, 14),
	BIT_FIELD(13, 13),
	BIT_FIELD(12, 9),
	BIT_FIELD(8, 8),
	BIT_FIELD(7, 0)
};

struct IDR1 {
	uint32_t const PageSize;
	uint32_t const NumPageNdxb;
	uint32_t const NumS2Cb;
	uint32_t const NumCb;
} idr1 = {
	BIT_FIELD(31, 31), // 0 = 4KB, 1 = 64KB
	BIT_FIELD(30, 28),
	BIT_FIELD(23, 16),
	BIT_FIELD(7, 0)
};

struct IDR2 {
	uint32_t const PTFSv8_64KB;
	uint32_t const PTFSv8_16KB;
	uint32_t const PTFSv8_4KB;
	uint32_t const UBS;
	uint32_t const OAS;
	uint32_t const IAS;
} idr2 = {
	BIT_FIELD(14, 14),
	BIT_FIELD(13, 13),
	BIT_FIELD(12, 12),
	BIT_FIELD(11, 8),
	BIT_FIELD(7, 4),
	BIT_FIELD(3, 0)
};

struct SMR {
	uint32_t const VALID;
	uint32_t const MASK;
	uint32_t const ID;
} smr = {
	BIT_FIELD(31, 31),
	BIT_FIELD(30, 16),
	BIT_FIELD(14, 0)
};

struct S2CR {
	uint32_t TRANSIENTCFG;
	uint32_t INSTCFG;
	uint32_t PRIVCFG;
	uint32_t WACFG;
	uint32_t RACFG;
	uint32_t NSCFG;
	uint32_t TYPE;	// Context = 0U, Bypass = 1U, Fault = 2U
	uint32_t MEMATTR;
	uint32_t MTCFG;
	uint32_t EXIDVALID;
	uint32_t SHCFG;
	uint32_t CBNDX;
} s2cr = {
	BIT_FIELD(29, 28),
	BIT_FIELD(27, 26),
	BIT_FIELD(25, 24),
	BIT_FIELD(23, 22),
	BIT_FIELD(21, 20),
	BIT_FIELD(19, 18),
	BIT_FIELD(17, 16),
	BIT_FIELD(15, 12),
	BIT_FIELD(11, 11),
	BIT_FIELD(10, 10),
	BIT_FIELD(9, 8),
	BIT_FIELD(7, 0)
};

struct TLBGSTATUS {
	uint32_t const GSACTIVE;
} tlbgstatus = {
	BIT_FIELD(0, 0)
};

struct GFSYNR0 {
	uint32_t const WNR;
} gfsynr0 = {
	BIT_FIELD(1, 1)
};

struct GFSYNR1 {
	uint32_t const STREAMID;
} gfsynr1 = {
	BIT_FIELD(7, 0)
};

struct GRS1_offset {
	const uint32_t CBAR;
	const uint32_t CBFRSYNRA;
	const uint32_t CBA2R;
} grs1_offs = {
	0x000U,
	0x400U,
	0x800U
};

struct CBAR {
	uint32_t const IRPTNDX;
	uint32_t const SBZ;
	uint32_t const TYPE;	// S2 = 0U, S1 = 1U, S1S2 = 3U
	uint32_t const S1_MEMATTR;
	uint32_t const S1_BPSHCFG;
	uint32_t const VMID;
} cbar = {
	BIT_FIELD(31, 24),
	BIT_FIELD(19, 18),
	BIT_FIELD(17, 16),
	BIT_FIELD(15, 12),
	BIT_FIELD(9, 8),
	BIT_FIELD(7, 0)
};

struct CBA2R {
	uint32_t const VMID16;
	uint32_t const MONC;
	uint32_t const VA64;
} cba2r = {
	BIT_FIELD(31, 16),
	BIT_FIELD(1, 1),
	BIT_FIELD(0, 0)
};

struct CBFRSYNRA {
	uint32_t const STREAMID;
} cbfrsynra = {
	BIT_FIELD(7, 0)
};

struct CB_offset {
	const uint32_t SCTLR;
	const uint32_t TTBR0;
	const uint32_t TTBR1;
	const uint32_t TCR;
	const uint32_t TCR2;
	const uint32_t CONTEXTIDR;
	const uint32_t MAIR0;
	const uint32_t RESUME;
	const uint32_t MAIR1;
	const uint32_t FSR;
	const uint32_t FAR;
	const uint32_t FSYNR0;
	const uint32_t FSYNR1;
	const uint32_t S1_TLBIASID;
	const uint32_t S1_TLBIVAL;
	const uint32_t TLBSYNC;
	const uint32_t TLBSTATUS;
} cb_offs = {
	0x0U,
	0x20U,
	0x28U,
	0x30U,
	0x10U,
	0x34U,
	0x38U,
	0x8U,
	0x3CU,
	0x58U,
	0x60U,
	0x68U,
	0x6CU,
	0x610,
	0x620,
	0x7F0U,
	0x7F4U
};

struct TCR {
	uint32_t const EAE;
	uint32_t const EPD1;
	uint32_t const A1;
	uint32_t const TG0;
	uint32_t const SH0;
	uint32_t const ORGN0;
	uint32_t const IRGN0;
	uint32_t const EPD0;
	uint32_t const T0SZ;
} tcr = {
	BIT_FIELD(31, 31),
	BIT_FIELD(23, 23),
	BIT_FIELD(22, 22),
	BIT_FIELD(15, 14),
	BIT_FIELD(13, 12),
	BIT_FIELD(11, 10),
	BIT_FIELD(9, 8),
	BIT_FIELD(7, 7),
	BIT_FIELD(5, 0)
};

struct TCR2 {
	uint32_t const SEP;		// IDR2_UBS = 7U
	uint32_t const AS;
	uint32_t const PASIZE;
} tcr2 = {
	BIT_FIELD(17, 15),
	BIT_FIELD(4, 4),
	BIT_FIELD(3, 0)
};

struct TTBR0 {
	uint64_t const ASID;
	uint64_t const BASEADDR;
} ttbr0 = {
	BIT_FIELD_64(63, 48),
	BIT_FIELD_64(47, 0)
};

struct SCTLR {
	uint32_t const WACFG;
	uint32_t const RACFG;
	uint32_t const SHCFG;
	uint32_t const FB;
	uint32_t const MEMATTR;
	uint32_t const BSU;
	uint32_t const PTW;
	uint32_t const S1_ASIDPNE;
	uint32_t const HUPCF;
	uint32_t const CFCFG;
	uint32_t const CFIE;
	uint32_t const CFRE;
	uint32_t const ENDIAN;
	uint32_t const AFFD;
	uint32_t const AFE;
	uint32_t const TRE;
	uint32_t const M;
} sctlr = {
	BIT_FIELD(27, 26),
	BIT_FIELD(25, 24),
	BIT_FIELD(23, 22),
	BIT_FIELD(21, 21),
	BIT_FIELD(19, 16),
	BIT_FIELD(15, 14),
	BIT_FIELD(13, 13),
	BIT_FIELD(12, 12),
	BIT_FIELD(8, 8),
	BIT_FIELD(7, 7),
	BIT_FIELD(6, 6),
	BIT_FIELD(5, 5),
	BIT_FIELD(4, 4),
	BIT_FIELD(3, 3),
	BIT_FIELD(2, 2),
	BIT_FIELD(1, 1),
	BIT_FIELD(0, 0)
};

struct FSYNR0 {
	uint32_t const WNR;
} fsynr0 =  {
	BIT_FIELD(4, 4)
};

struct TLBSTATUS {
	uint32_t const SACTIVE;
} tlbstatus = {
	BIT_FIELD(0, 0)
};

static inline uint32_t smmu_read32(uint32_t offs)
{
	return NV_READ32(smmu.base + offs);
}

static inline void smmu_write32(uint32_t offs, uint32_t val)
{
	NV_WRITE32(smmu.base + offs, val);
}

static inline uint64_t smmu_read64(uint32_t offs)
{
	return NV_READ64(smmu.base + offs);
}

static inline void smmu_write64(uint32_t offs, uint64_t val)
{
	NV_WRITE64(smmu.base + offs, val);
}

static inline uint32_t cb_read(uint32_t idx, uint32_t offs)
{
	return smmu_read32((smmu.pagesize * (smmu.nr_pages + idx)) + offs);
}

static inline void cb_write(uint32_t idx, uint32_t offs, uint32_t val)
{
	smmu_write32((smmu.pagesize * (smmu.nr_pages + idx)) + offs, val);
}

static inline uint64_t cb_read64(uint32_t idx, uint32_t offs)
{
	return smmu_read64((smmu.pagesize * (smmu.nr_pages + idx)) + offs);
}

static inline void cb_write64(uint32_t idx, uint32_t offs, uint64_t val)
{
	smmu_write64((smmu.pagesize * (smmu.nr_pages + idx)) + offs, val);
}

#endif	/* _TEGRABL_SMMUV2_REG_H */
