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

#include "build_config.h"
#include <tegrabl_ar_macro.h>
#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_timer.h>
#include <tegrabl_utils.h>
#include <tegrabl_malloc.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_malloc.h>
#include <tegrabl_io.h>
#include <tegrabl_dmamap.h>
#include <kernel/thread.h>

/* CACHE flags defined in both kernel/thread.h and tegrabl_cache.h */
#ifdef ICACHE
#undef ICACHE
#endif
#ifdef DCACHE
#undef DCACHE
#endif
#ifdef UCACHE
#undef UCACHE
#endif

#include <tegrabl_cache.h>

#include "smmu_helper.h"
#include "tegrabl_smmuv2_reg.h"
#include "tegrabl_smmu_ext.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000U
#endif

#define TABLE_SIZE 0x1000U
#define PAGE_TABLE_START_LEVEL 0
#define MAX_PAGE_TABLE_LEVEL 4

#define LEVEL0_PAGE_SHIFT 39
#define LEVEL1_PAGE_SHIFT 30
#define LEVEL2_PAGE_SHIFT 21
#define LEVEL3_PAGE_SHIFT 12

#define LEVEL0_PAGE_SIZE (1UL << LEVEL0_PAGE_SHIFT)
#define LEVEL1_PAGE_SIZE (1UL << LEVEL1_PAGE_SHIFT)
#define LEVEL2_PAGE_SIZE (1UL << LEVEL2_PAGE_SHIFT)
#define LEVEL3_PAGE_SIZE (1UL << LEVEL3_PAGE_SHIFT)

#define PAGE_INDEX_SIZE (LEVEL2_PAGE_SHIFT - LEVEL3_PAGE_SHIFT)

#define PTE_TYPE_BLOCK		1
#define PTE_TYPE_TABLE		3
#define PTE_TYPE_PAGE		3
#define PTE_nG			(((uint64_t)1) << 11)
#define PTE_AP_UNPRIV		(((uint64_t)1) << 6)
#define PTE_SH_IS		(((uint64_t)3) << 8)
#define PTE_AF			(((uint64_t)1) << 10)
#define PTE_AP_RDONLY		(((uint64_t)2) << 6)
#define MAIR_ATTR_IDX_CACHE	1
#define PTE_ATTRINDX_SHIFT	2
#define PTE_ADDR_MASK		0xfffffffff000

#define PTE_FLAGS		(PTE_nG | PTE_AP_UNPRIV | PTE_SH_IS | PTE_AF)

#define MAIR_ATTR_SHIFT(n)		((n) << 3)
#define MAIR_ATTR_MASK			0xff
#define MAIR_ATTR_DEVICE		0x04
#define MAIR_ATTR_NC			0x44
#define MAIR_ATTR_INC_OWBRWA		0xf4
#define MAIR_ATTR_WBRWA			0xff
#define MAIR_ATTR_IDX_NC		0
#define MAIR_ATTR_IDX_CACHE		1
#define MAIR_ATTR_IDX_DEV		2
#define MAIR_ATTR_IDX_INC_OCACHE	3

static struct {
	uint64_t size;
	uint32_t shift;
} tt_level[] = {
	{ LEVEL0_PAGE_SIZE, LEVEL0_PAGE_SHIFT },
	{ LEVEL1_PAGE_SIZE, LEVEL1_PAGE_SHIFT },
	{ LEVEL2_PAGE_SIZE, LEVEL2_PAGE_SHIFT },
	{ LEVEL3_PAGE_SIZE, LEVEL3_PAGE_SHIFT },
};

static inline uint64_t table_index(unsigned long iova, int level)
{
	return (iova >> tt_level[level].shift) & ((1 << PAGE_INDEX_SIZE) - 1);
}

static void *tegrabl_smmu_alloc_pages(size_t size)
{
	void *pages = tegrabl_alloc_align(TEGRABL_HEAP_DMA, PAGE_SIZE, size);

	if (!pages) {
		pr_error("Out of memory for pages\n");
		return NULL;
	}
	memset(pages, 0, size);

	(void)tegrabl_dma_map_buffer(TEGRABL_MODULE_PCIE, 0, pages, size, TEGRABL_DMA_TO_DEVICE);

	return pages;
}

static void tegrabl_smmu_free_pages(void *pages, size_t size)
{
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_PCIE, 0, pages, size, TEGRABL_DMA_FROM_DEVICE);
	tegrabl_free(pages);
}

static void tlb_inv_and_sync(void)
{
	uint32_t reg;

	smmu_write32(grs0_offs.TLBIALLH, 0x0U);
	smmu_write32(grs0_offs.TLBIALLNSNH, 0x0U);
	smmu_write32(grs0_offs.TLBGSYNC, 0x0U);

	do {
		reg = smmu_read32(grs0_offs.TLBGSTATUS);
	} while ((reg & tlbgstatus.GSACTIVE) != 0U);
}

static void cb_tlb_inv_and_sync(unsigned long iova, uint32_t cb)
{
	uint32_t reg;

	wmb();

	iova >>= 12;
	iova |= (uint64_t)cb << 48;
	cb_write64(cb, cb_offs.S1_TLBIVAL, iova);

	cb_write(cb, cb_offs.TLBSYNC, 0);
	do {
		reg = cb_read(cb, cb_offs.TLBSTATUS);
	} while ((reg & tlbgstatus.GSACTIVE) != 0U);
}

static void tlb_inv_sync_context(uint32_t cb)
{
	uint32_t reg;

	wmb();

	cb_write(cb, cb_offs.S1_TLBIASID, cb);

	cb_write(cb, cb_offs.TLBSYNC, 0);
	do {
		reg = cb_read(cb, cb_offs.TLBSTATUS);
	} while ((reg & tlbgstatus.GSACTIVE) != 0U);
}

static uint32_t allocate_smr(void)
{
	uint32_t smr_id;

	for (smr_id = 0; smr_id < smmu.nr_smr; smr_id++) {
		if (smmu.smr[smr_id] == 0) {
			smmu.smr[smr_id] = 1;
			break;
		}
	}

	return smr_id;
}

static tegrabl_error_t program_smr(uint32_t sid)
{
	uint32_t const smr_id = allocate_smr();
	uint32_t l_smr = 0U;

	if (smr_id >= smmu.nr_smr)
		return TEGRABL_ERR_OUT_OF_RANGE;

	sid_to_smr_map[sid] = smr_id;

	l_smr |= smr_mask;
	l_smr |= set_bits(smr.VALID, 1U);
	l_smr |= set_bits(smr.ID, sid);

	smmu_write32(grs0_offs.SMR + (smr_id << 2), l_smr);

	return TEGRABL_NO_ERROR;
}

static void free_smr(uint32_t sid)
{
	uint32_t smr_id = sid_to_smr_map[sid];
	uint32_t l_s2cr = 0U;

	l_s2cr |= set_bits(s2cr.TYPE, 0x2U);
	smmu.smr[smr_id] = 0U;
	smmu_write32(grs0_offs.SMR + (smr_id << 2), 0U);
	smmu_write32(grs0_offs.S2CR + (smr_id << 2), l_s2cr);

	 sid_to_smr_map[sid] = 0U;
}

static void program_s2cr(uint32_t sid, uint32_t cb)
{
	uint32_t smr_id, l_s2cr = 0U;

	smr_id = sid_to_smr_map[sid];

	l_s2cr |= set_bits(s2cr.TYPE, 0U);
	l_s2cr |= set_bits(s2cr.CBNDX, cb);
	l_s2cr |= set_bits(s2cr.PRIVCFG, 0U);

	smmu_write32(grs0_offs.S2CR + (smr_id << 2), l_s2cr);
}

static uint32_t alloc_cb(void)
{
	uint32_t cb;

	for (cb = 0; cb < smmu.nr_cb; cb++) {
		if (smmu.cbs[cb] == 0) {
			smmu.cbs[cb] = 1;
			break;
		}
	}
	return cb;
}

static void free_cb(uint32_t cb)
{
	smmu.cbs[cb] = 0;
}

static tegrabl_error_t configure_cb(struct priv_data *data)
{
	uint32_t l_tcr = 0U, l_tcr2 = 0U, l_cba2r = 0U, l_cbar = 0U;
	uint32_t l_mair = 0U, l_mair2 = 0U, l_sctlr = 0U;
	uint64_t l_ttbr = 0U, l_ttbr2 = 0, mair64 = 0U;
	uint32_t cb = data->cb;

	l_cba2r |= set_bits(cba2r.VA64, 1U);
	smmu_write32(smmu.pagesize + grs1_offs.CBA2R + (cb << 2), l_cba2r);

	l_cbar |= set_bits(cbar.TYPE, 1U);
	l_cbar |= set_bits(cbar.S1_BPSHCFG, 3U);
	l_cbar |= set_bits(cbar.S1_MEMATTR, 0xFU);
	smmu_write32(smmu.pagesize + grs1_offs.CBAR + (cb << 2), l_cbar);

	l_tcr |= set_bits(tcr.SH0, 2U);
	l_tcr |= set_bits(tcr.ORGN0, 0U);
	l_tcr |= set_bits(tcr.IRGN0, 0U);
	l_tcr |= set_bits(tcr.TG0, 0U);
	l_tcr |= set_bits(tcr.T0SZ, 0x10U);
	l_tcr |= set_bits(tcr.EPD1, 1U);
	cb_write(cb, cb_offs.TCR, l_tcr);

	l_tcr2 |= set_bits(tcr2.AS, 1U);
	l_tcr2 |= set_bits(tcr2.PASIZE, 0x5U);
	l_tcr2 |= set_bits(tcr2.SEP, 0x7U);
	cb_write(cb, cb_offs.TCR2, l_tcr2);

	data->table_base = tegrabl_smmu_alloc_pages(TABLE_SIZE);
	if (!data->table_base)
		return TEGRABL_ERR_NO_MEMORY;

	wmb();

	l_ttbr |= set_bits64(ttbr0.ASID, cb);
	l_ttbr |= (uint64_t)data->table_base;
	cb_write64(cb, cb_offs.TTBR0, l_ttbr);

	l_ttbr2 |= set_bits64(ttbr0.ASID, cb);
	cb_write64(cb, cb_offs.TTBR1, l_ttbr2);

	mair64 |= (MAIR_ATTR_NC << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_NC));
	mair64 |= (MAIR_ATTR_WBRWA << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_CACHE));
	mair64 |= (MAIR_ATTR_DEVICE << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_DEV));
	mair64 |= (MAIR_ATTR_INC_OWBRWA << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_INC_OCACHE));
	l_mair = mair64;
	cb_write(cb, cb_offs.MAIR0, l_mair);

	l_mair2 = mair64 >> 32;
	cb_write(cb, cb_offs.MAIR1, l_mair2);

	l_sctlr |= set_bits(sctlr.CFIE, 1U);
	l_sctlr |= set_bits(sctlr.CFRE, 1U);
	l_sctlr |= set_bits(sctlr.AFE, 1U);
	l_sctlr |= set_bits(sctlr.TRE, 1U);
	l_sctlr |= set_bits(sctlr.M, 1U);
	l_sctlr |= set_bits(sctlr.HUPCF, 1U);
	l_sctlr |= set_bits(sctlr.S1_ASIDPNE, 1U);
	cb_write(cb, cb_offs.SCTLR, l_sctlr);

	return TEGRABL_NO_ERROR;
}

static int tegrabl_smmu_context_fault(void *arg)
{
	uint32_t l_fsr, l_fsynr, l_cbfrsynra, reg, fsr_fault = 0U;
	struct priv_data *data = arg;
	unsigned long long iova = 0;
	uint32_t cb = data->cb;

	fsr_fault |= set_bits(fsr.MULTI, 1U);
	fsr_fault |= set_bits(fsr.SS, 1U);
	fsr_fault |= set_bits(fsr.UUT, 1U);
	fsr_fault |= set_bits(fsr.EF, 1U);
	fsr_fault |= set_bits(fsr.PF, 1U);
	fsr_fault |= set_bits(fsr.TF, 1U);
	fsr_fault |= set_bits(fsr.AFF, 1U);
	fsr_fault |= set_bits(fsr.ASF, 1U);
	fsr_fault |= set_bits(fsr.TLBMCF, 1U);
	fsr_fault |= set_bits(fsr.TLBLKF, 1U);

	pr_info("Registering SMMU context fault handler\n");
	while (true) {
		reg = cb_read(cb, cb_offs.SCTLR);
		if (reg == 0)
			break;

		l_fsr = cb_read(cb, cb_offs.FSR);
		if (l_fsr & fsr_fault) {
			pr_error("SMMU context fault occurred\n");
			l_fsynr = cb_read(cb, cb_offs.FSYNR0);
			iova = cb_read64(cb, cb_offs.FAR);
			l_cbfrsynra = smmu_read32(smmu.pagesize + grs1_offs.CBFRSYNRA + (cb << 2));
			pr_error("SMMU context fault: fsr=0x%x, iova=0x%llx, fsynr=0x%x, cbfrsynra=0x%x, cb=%d\n",
				  l_fsr, iova, l_fsynr, l_cbfrsynra, cb);
		}
	}
	pr_info("Exiting SMMU context fault handler\n");

	return 0;
}

static void init_context_fault_logger(struct priv_data *data)
{
	thread_t *t;

	t = thread_create("tegrabl_smmu_context_fault", tegrabl_smmu_context_fault, data, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE);
	if (!t)
		pr_error("context fault thread creation failed\n");
	else {
		if (thread_detach(t))
			pr_error("thread detach failed\n");
		else if (thread_resume(t))
			pr_error("thread resume failed\n");
	}
}

tegrabl_error_t tegrabl_smmu_add_device(const void *fdt, int32_t node_offset,
					void **data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct priv_data *priv;
	uint32_t res[2], num;
	uint32_t cb, sid;

	err = tegrabl_dt_get_prop_u32_array(fdt, node_offset, "iommus", 2, &res[0], &num);
	if (err != TEGRABL_NO_ERROR || num != 2) {
		pr_error("iommus property not found in dt\n");
		return err;
	}

	sid = res[1];

	err = program_smr(sid);
	if (err != TEGRABL_NO_ERROR)
		return err;

	cb = alloc_cb();
	if (cb >= smmu.nr_cb)
		return TEGRABL_ERR_OUT_OF_RANGE;

	priv = tegrabl_malloc(sizeof(*priv));
	if (!priv) {
		err = TEGRABL_ERR_NO_MEMORY;
		goto free_cb;
	}
	priv->sid = sid;
	priv->cb = cb;

	err = configure_cb(priv);
	if (err != TEGRABL_NO_ERROR)
		goto free_data;

	init_context_fault_logger(priv);

	program_s2cr(sid, cb);

	*data = priv;

	return err;

free_data:
	tegrabl_free(priv);
free_cb:
	free_cb(cb);

	return err;
}

static void free_page_table(int lvl, uint64_t *tt_pte)
{
	uint64_t *start = tt_pte, *end;

	if (lvl == MAX_PAGE_TABLE_LEVEL - 1)
		end = tt_pte;
	else
		end = (void *)tt_pte + TABLE_SIZE;

	while (tt_pte != end) {
		uint64_t pte = *tt_pte++;

		if (!pte)
			continue;

		free_page_table(lvl + 1, (uint64_t *)(pte & PTE_ADDR_MASK));
	}

	tegrabl_smmu_free_pages(start, TABLE_SIZE);
}

void tegrabl_smmu_remove_device(void *data)
{
	struct priv_data *priv = data;
	uint32_t sid = priv->sid;
	uint32_t cb = priv->cb;

	free_smr(sid);

	tlb_inv_sync_context(priv->cb);
	free_page_table(PAGE_TABLE_START_LEVEL, priv->table_base);
	tegrabl_free(priv);

	cb_write(cb, cb_offs.SCTLR, 0);
	free_cb(cb);
	pr_error("exiting remove dev\n");
}

static void tegrabl_smmu_hw_initialize(void)
{
	uint32_t l_cr0 = 0U, l_fsr = 0U, l_s2cr = 0U;
	uint32_t i;

	smmu_write32(grs0_offs.GFAR, 0x0U);
	smmu_write32(grs0_offs.GFSR, 0x0U);
	smmu_write32(grs0_offs.GFSYNR0, 0x0U);
	smmu_write32(grs0_offs.GFSYNR1, 0x0U);
	smmu_write32(grs0_offs.GFSYNR2, 0x0U);

	tlb_inv_and_sync();

	l_s2cr |= set_bits(s2cr.TYPE, 0x2U);
	for (i = 0; i < smmu.nr_smr; i++) {
		if (i < MAX_SMR) {
			smmu.smr[i] = 0U;
			smmu_write32(grs0_offs.SMR + (i << 2), 0U);

			smmu_write32(grs0_offs.S2CR + (i << 2), l_s2cr);
		}
	}

	l_fsr |= set_bits(fsr.MULTI, 1U);
	l_fsr |= set_bits(fsr.SS, 1U);
	l_fsr |= set_bits(fsr.UUT, 1U);
	l_fsr |= set_bits(fsr.EF, 1U);
	l_fsr |= set_bits(fsr.PF, 1U);
	l_fsr |= set_bits(fsr.TF, 1U);
	l_fsr |= set_bits(fsr.AFF, 1U);
	l_fsr |= set_bits(fsr.ASF, 1U);
	l_fsr |= set_bits(fsr.TLBMCF, 1U);
	l_fsr |= set_bits(fsr.TLBLKF, 1U);

	for (i = 0; i < smmu.nr_cb; i++) {
		cb_write(i, cb_offs.SCTLR, 0);
		cb_write(i, cb_offs.FSR, l_fsr);
	}

	/* Enable fault reporting */
	l_cr0 |= set_bits(cr0.GFRE, 1U);
	l_cr0 |= set_bits(cr0.GFIE, 1U);
	l_cr0 |= set_bits(cr0.GCFGFRE, 1U);
	l_cr0 |= set_bits(cr0.GCFGFIE, 1U);
	/* Disable TLB broadcasting */
	l_cr0 |= set_bits(cr0.VMIDPNE, 1U);
	l_cr0 |= set_bits(cr0.PTM, 1U);
	/* Enable client access, handling unmatched streams as appropriate */
	l_cr0 |= set_bits(cr0.CLIENTPD, 0U);
	/* Enable bypass */
	l_cr0 |= set_bits(cr0.USFCFG, 0U);
	/* Disable forced broadcasting */
	l_cr0 |= set_bits(cr0.FB, 0U);
	/* Don't upgrade barriers */
	l_cr0 |= set_bits(cr0.BSU, 0U);
	/* Raise a fault */
	l_cr0 |= set_bits(cr0.SMCFCFG, 1U);

	smmu_write32(grs0_offs.CR0, l_cr0);

}

static void install_table_pte(uint64_t *table, uint64_t *tt_pte)
{
	dma_wmb();

	*tt_pte = (uintptr_t)table | PTE_TYPE_TABLE;

	tegrabl_arch_clean_dcache_range((uintptr_t)tt_pte, sizeof(*tt_pte));
}

static void install_block_pte(uint64_t paddr, int prot, int lvl, uint64_t *pte)
{
	*pte |= PTE_FLAGS;

	if (!(prot & SMMU_WRITE) && (prot & SMMU_READ))
		*pte |= PTE_AP_RDONLY;

	*pte |= (MAIR_ATTR_IDX_CACHE << PTE_ATTRINDX_SHIFT);

	if (lvl == MAX_PAGE_TABLE_LEVEL - 1)
		*pte |= PTE_TYPE_PAGE;
	else
		*pte |= PTE_TYPE_BLOCK;

	*pte |= paddr;

	tegrabl_arch_clean_dcache_range((uintptr_t)pte, sizeof(*pte));
}

static void clear_pte(uint64_t *pte)
{
	*pte = 0;

	tegrabl_arch_clean_dcache_range((uintptr_t)pte, sizeof(*pte));
}

tegrabl_error_t tegrabl_smmu_enable_prot(void *data, unsigned long iova,
				uint64_t paddr, size_t size, int prot)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	unsigned long orig_iova = iova;
	struct priv_data *priv = data;
	size_t orig_size = size;

	if (!(prot & (SMMU_READ | SMMU_WRITE)))
		return TEGRABL_ERR_NO_ACCESS;

	if (!size || !IS_ALIGNED(size, PAGE_SIZE))
		return TEGRABL_ERR_INVALID;

	while (size) {
		uint64_t *table = (uint64_t *)priv->table_base;
		int lvl;

		for (lvl = 0; lvl < MAX_PAGE_TABLE_LEVEL; lvl++) {
			uint64_t tbl_idx = table_index(iova, lvl);
			uint64_t *tt_pte = table + tbl_idx;
			uint64_t *tt_next;

			if (tt_level[lvl].size == PAGE_SIZE) {
				install_block_pte(paddr, prot, lvl, tt_pte);
				break;
			}

			if (*tt_pte) {
				table = (uint64_t *)(*tt_pte & PTE_ADDR_MASK);
				continue;
			}

			tt_next = tegrabl_smmu_alloc_pages(TABLE_SIZE);
			if (!tt_next) {
				err = TEGRABL_ERR_NO_MEMORY;
				break;
			}

			install_table_pte(tt_next, tt_pte);

			table = tt_next;
		}

		if (err != TEGRABL_NO_ERROR)
			break;

		iova += PAGE_SIZE;
		paddr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	wmb();

	if (size) {
		tegrabl_smmu_disable_prot(priv, orig_iova, orig_size);
		return TEGRABL_ERR_INVALID;
	}

	return TEGRABL_NO_ERROR;
}

size_t tegrabl_smmu_disable_prot(void *data, unsigned long iova, size_t size)
{
	struct priv_data *priv = data;
	size_t unmapped = 0UL;

	if (!size || !IS_ALIGNED(size, PAGE_SIZE))
		return 0;

	while (unmapped < size) {
		uint64_t *table = (uint64_t *)priv->table_base;
		int lvl;

		for (lvl = 0; lvl < MAX_PAGE_TABLE_LEVEL; lvl++) {
			uint64_t tbl_idx = table_index(iova, lvl);
			uint64_t *tt_pte = table + tbl_idx;

			if (*tt_pte == 0U) {
				pr_error("No PTE mappings found for iova %lx\n", iova);
				return 0;
			}

			if (tt_level[lvl].size == PAGE_SIZE) {
				clear_pte(tt_pte);
				cb_tlb_inv_and_sync(iova, priv->cb);
				break;
			}

			table = (uint64_t *)(*tt_pte & PTE_ADDR_MASK);
		}

		iova += PAGE_SIZE;
		unmapped += PAGE_SIZE;
	}

	return unmapped;
}

tegrabl_error_t tegrabl_smmu_init(void)
{
	uint64_t hw_pgsize;
	uint32_t reg;

	smmu.base = NV_ADDRESS_MAP_SMMU0_BASE;
	reg = smmu_read32(grs0_offs.IDR1);
	hw_pgsize = (reg & idr1.PageSize) ? 0x10000U : 0x1000U;
	smmu.pagesize = hw_pgsize;
	smmu.nr_pages = 1 << (BIT_RSH(reg, idr1.NumPageNdxb) + 1);
	smmu.nr_cb = BIT_RSH(reg, idr1.NumCb);
	smmu.cbs = tegrabl_calloc(smmu.nr_cb, sizeof(*smmu.cbs));
	if (!smmu.cbs)
		return TEGRABL_ERR_NO_MEMORY;

	reg = smmu_read32(grs0_offs.IDR0);
	smmu.nr_smr = BIT_RSH(reg, idr0.NUMSMRG);
	smmu.smr = tegrabl_calloc(smmu.nr_smr, sizeof(*smmu.smr));
	if (!smmu.cbs) {
		tegrabl_free(smmu.cbs);
		return TEGRABL_ERR_NO_MEMORY;
	}

	tegrabl_smmu_hw_initialize();
	return TEGRABL_NO_ERROR;
}

void tegrabl_smmu_deinit(void)
{
	smmu_write32(grs0_offs.CR0, cr0.CLIENTPD);

	tegrabl_free(smmu.smr);
	tegrabl_free(smmu.cbs);
}
