/* Copyright (c) 2018-2019 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_USBH

#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_debug.h>
#include <tegrabl_fuse.h>

#include <tegrabl_partition_manager.h>
#include <tegrabl_gpt.h>
#include <tegrabl_a_b_boot_control.h>
#include <tegrabl_utils.h>
#include <xhci_priv.h>

#define DRAM_MAP_FW_START   0x80040000LLU
#define DRAM_MAP_FW_SIZE    0x20000

#define CFG_1			0x00000004
#define  MEMORY_SPACE_ENABLE	0x2
#define  BUS_MASTER_ENABLE	0x4
#define  IO_SPACE_ENABLE	0x1
#define CFG_4			0x00000010
#define  BASE_ADDR_MASK		0xffff8000
#define  BASE_ADDR_SHIFT	15
/* MEMPOOL CSB registers */
#define MEMPOOL_ILOAD_ATTR	0x00101a00
#define MEMPOOL_ILOAD_BASE_LO	0x00101a04
#define MEMPOOL_ILOAD_BASE_HI	0x00101a08
#define MEMPOOL_L2IMEMOP_SIZE	0x00101a10
#define  SRC_OFFSET_SHIFT	8
#define  SRC_OFFSET_MASK	0xfff
#define  SRC_COUNT_SHIFT	24
#define  SRC_COUNT_MASK		0xff
#define MEMPOOL_L2IMEMOP_TRIG	0x00101a14
#define  ACTION_MASK		0xff000000
#define  ACTION_L2IMEM_INVALIDATE_ALL		0x40000000
#define  ACTION_L2IMEM_LOAD_LOCKED_RESULT	0x11000000
#define MEMPOOL_L2IMEMOP_RESULT	0x00101A18
#define  VLD_SET		0x80000000
#define MEMPOOL_APMAP		0x0010181c
#define  BOOTPATH_SET		0x80000000
/* FALCON registers */
#define FALCON_CPUCTL		0x100
#define  STARTCPU		0x2
#define  STATE_HALTED		0x10
#define  STATE_STOPPED		0x20
#define FALCON_BOOTVEC		0x104
#define FALCON_DMACTL		0x10c
#define FALCON_IMFILLRNG1	0x154
#define  TAG_MASK		0xffff
#define  TAG_LO_SHIFT		0
#define  TAG_HI_SHIFT		16
#define FALCON_IMFILLCTL	0x158

#define ENABLE_FW_LOG 0
#define DEBUG_ENABLE 0
#define NUM_LOG_ENTRIES 1024
static uint8_t *fw_log;

struct tegra_xhci_fw_cfgtbl {
	uint32_t boot_loadaddr_in_imem;
	uint32_t boot_codedfi_offset;
	uint32_t boot_codetag;
	uint32_t boot_codesize;
	uint32_t phys_memaddr;
	uint16_t reqphys_memsize;
	uint16_t alloc_phys_memsize;
	uint32_t rodata_img_offset;
	uint32_t rodata_section_start;
	uint32_t rodata_section_end;
	uint32_t main_fnaddr;
	uint32_t fwimg_cksum;
	uint32_t fwimg_created_time;
	uint32_t imem_resident_start;
	uint32_t imem_resident_end;
	uint32_t idirect_start;
	uint32_t idirect_end;
	uint32_t l2_imem_start;
	uint32_t l2_imem_end;
	uint32_t version_id;
	uint8_t  init_ddirect;
	uint8_t  reserved[3];
	uint32_t phys_addr_log_buffer;
	uint32_t total_log_entries;
	uint32_t dequeue_ptr;
	uint32_t dummy_var[2];
	uint32_t fwimg_len;
	uint8_t  magic[8];
	uint32_t ss_low_power_entry_timeout;
	uint8_t  num_hsic_port;
	uint8_t  ss_portmap;
	uint8_t  build_log:4;
	uint8_t  build_type:4;
	uint8_t  padding[137]; /* Padding to make 256-bytes cfgtbl */
};

#define NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE NV_ADDRESS_MAP_XUSB_HOST_PF_CFG_BASE
#define XUSB_CSB_BASE (NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + 0x800)

static void xusbh_csb_writel(uint32_t reg, uint32_t value)
{
	uint32_t reg_offset = reg % XUSB_CFG_PAGE_SIZE;
	uint32_t page_num = reg / XUSB_CFG_PAGE_SIZE;

	NV_WRITE32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + ARU_C11_CSBRANGE,
					page_num);
	NV_WRITE32(XUSB_CSB_BASE + reg_offset, value);
}

static uint32_t xusbh_csb_readl(uint32_t reg)
{
	uint32_t value;
	uint32_t reg_offset = reg % XUSB_CFG_PAGE_SIZE;
	uint32_t page_num = reg / XUSB_CFG_PAGE_SIZE;

	NV_WRITE32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + ARU_C11_CSBRANGE,
					page_num);
	value = NV_READ32(XUSB_CSB_BASE + reg_offset);

	return value;
}

tegrabl_error_t xusbh_has_firmware_partition(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char partition_name[TEGRABL_GPT_MAX_PARTITION_NAME + 1];
	uint32_t active_slot = BOOT_SLOT_A;
	struct tegrabl_partition part;

	strcpy(partition_name, "xusb-fw");
#if defined(CONFIG_ENABLE_A_B_SLOT)
	err = tegrabl_a_b_get_active_slot(NULL, &active_slot);
	if (err == TEGRABL_NO_ERROR) {
		tegrabl_a_b_set_bootslot_suffix(active_slot,
							partition_name, false);
	}
#endif
	err = tegrabl_partition_open(partition_name, &part);
	return err;
}

#if DEBUG_ENABLE
static void dump_clocks(void)
{
	pr_error("OUT_ENB_XUSB = 0x%x\n", NV_READ32(0x20000000 + 0x471000));
	pr_error("SOURCE_XUSB_CORE_HOST = 0x%x\n", NV_READ32(0x20000000 + 0x473000));
	pr_error("SOURCE_XUSB_FALCON = 0x%x\n", NV_READ32(0x20000000 + 0x473004));
	pr_error("SOURCE_XUSB_FS = 0x%x\n", NV_READ32(0x20000000 + 0x473008));
	pr_error("SOURCE_XUSB_CORE_DEV = 0x%x\n", NV_READ32(0x20000000 + 0x47300c));
	pr_error("SOURCE_XUSB_SS = 0x%x\n", NV_READ32(0x20000000 + 0x473010));
	pr_error("PLLE_AUX_0 = 0x%x\n", NV_READ32(0x20000000 + 0x4101c));
	pr_error("UTMIPLL_HW_PWRDN_CFG0 = 0x%x\n", NV_READ32(0x20000000 + 0x3f0010));

	/* RST_XUSB_AON */
	pr_error("RST_DEV_XUSB_AON = 0x%x\n", NV_READ32(0x20000000 + 0xb00018));
	pr_error("RST_DEV_XUSB_AON_SET = 0x%x\n", NV_READ32(0x20000000 + 0xb0001c));
	pr_error("RST_DEV_XUSB_AON_CLR = 0x%x\n", NV_READ32(0x20000000 + 0xb00020));
}
#endif

#define IMEM_BLOCK_SIZE 256
#define MAX_STORAGE_BLOCK_SIZE 4096
tegrabl_error_t xusbh_load_firmware(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition part;
	struct tegra_xhci_fw_cfgtbl *fw_header = NULL;
	uint32_t header_alloc_size;
	uint32_t fw_alloc_size;
	uint8_t *fw_data;
	uint64_t fw_base;
	char partition_name[TEGRABL_GPT_MAX_PARTITION_NAME + 1];
	uint32_t active_slot = BOOT_SLOT_A;
	dma_addr_t dma_buf;
	uint32_t reg_val;
	uint32_t code_tag_blocks;
	uint32_t code_size_blocks;
	uint32_t timeout;

	/* ARU reset..this is done in kernel code */
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + 0x42c, 0x1);
	tegrabl_udelay(2000);

	if (xusbh_csb_readl(MEMPOOL_ILOAD_BASE_LO) != 0) {
		pr_info("Firmware already loaded, Falcon state 0x%x\n",
					 xusbh_csb_readl(FALCON_CPUCTL));
		return err;
	}

	reg_val = NV_READ32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + CFG_4);
	reg_val &= ~(0x3fff << 18);
	reg_val |= 0x3610000 & (0x3fff << 18);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + CFG_4, reg_val);
	tegrabl_mdelay(1);

	reg_val = NV_READ32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + CFG_1);
	reg_val |= MEMORY_SPACE_ENABLE;
	reg_val |= BUS_MASTER_ENABLE;
	reg_val |= IO_SPACE_ENABLE;
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + CFG_1, reg_val);

	/* Load fw from storage device */
	strcpy(partition_name, "xusb-fw");
#if defined(CONFIG_ENABLE_A_B_SLOT)
	err = tegrabl_a_b_get_active_slot(NULL, &active_slot);
	if (err == TEGRABL_NO_ERROR) {
		tegrabl_a_b_set_bootslot_suffix(active_slot,
						partition_name, false);
	}
#endif
	err = tegrabl_partition_open(partition_name, &part);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Cannot open partition %s\n", partition_name);
		goto done;
	}

	/* Read FW header */
	if ((sizeof(struct tegra_xhci_fw_cfgtbl) % MAX_STORAGE_BLOCK_SIZE) != 0) {
		header_alloc_size = sizeof(struct tegra_xhci_fw_cfgtbl) -
							(sizeof(struct tegra_xhci_fw_cfgtbl) % MAX_STORAGE_BLOCK_SIZE) +
							MAX_STORAGE_BLOCK_SIZE;
	} else {
		header_alloc_size = sizeof(struct tegra_xhci_fw_cfgtbl);
	}

	fw_header = (struct tegra_xhci_fw_cfgtbl *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 8, header_alloc_size);
	if (fw_header == NULL) {
		pr_error("Failed to allocate memory for fw header\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}
	err = tegrabl_partition_read(&part, fw_header, sizeof(struct tegra_xhci_fw_cfgtbl));
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading xusb fw header\n");
		goto done;
	}

	/* Read firmware */
	if ((fw_header->fwimg_len % MAX_STORAGE_BLOCK_SIZE) != 0) {
		fw_alloc_size = fw_header->fwimg_len - (fw_header->fwimg_len % MAX_STORAGE_BLOCK_SIZE) +
						MAX_STORAGE_BLOCK_SIZE;
	} else {
		fw_alloc_size = fw_header->fwimg_len;
	}
	fw_data = (uint8_t *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 256, fw_alloc_size);
	if (fw_data == NULL) {
		pr_error("failed to allocate memory for xusb-fw data\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	err = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (err != TEGRABL_NO_ERROR) {
		goto done;
	}
	err = tegrabl_partition_read(&part, fw_data, fw_header->fwimg_len);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading xusb fw data\n");
		goto done;
	}

#if ENABLE_FW_LOG
	struct tegra_xhci_fw_cfgtbl *fw_cfgtbl;

	fw_log = (uint8_t *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 16, NUM_LOG_ENTRIES * 32);
	if (fw_log == NULL) {
		pr_error("failed to allocate memory for xusb-fw log\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}
	memset(fw_log, 0x0, NUM_LOG_ENTRIES * 32);

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSB_HOST, 0,
			fw_log, NUM_LOG_ENTRIES * 32, TEGRABL_DMA_BIDIRECTIONAL);
	fw_cfgtbl = (struct tegra_xhci_fw_cfgtbl *)fw_data;
	fw_cfgtbl->phys_addr_log_buffer = (uint32_t)dma_buf;
	fw_cfgtbl->total_log_entries = NUM_LOG_ENTRIES;
	fw_cfgtbl->ss_portmap = 0x0;
#else
	TEGRABL_UNUSED(fw_log);
#endif

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSB_HOST, 0,
			fw_data, fw_header->fwimg_len, TEGRABL_DMA_TO_DEVICE);
	/* Program the size of DFI into ILOAD_ATTR. */
	xusbh_csb_writel(MEMPOOL_ILOAD_ATTR, fw_header->fwimg_len);
	/*
	 * Boot code of the firmware reads the ILOAD_BASE registers
	 * to get to the start of the DFI in system memory.
	 */
	fw_base = dma_buf + sizeof(struct tegra_xhci_fw_cfgtbl);
	xusbh_csb_writel(MEMPOOL_ILOAD_BASE_LO, U64_TO_U32_LO(fw_base));
	xusbh_csb_writel(MEMPOOL_ILOAD_BASE_HI, U64_TO_U32_HI(fw_base));

	/* Set BOOTPATH to 1 in APMAP. */
	reg_val = xusbh_csb_readl(MEMPOOL_APMAP);
	reg_val |= BOOTPATH_SET;
	xusbh_csb_writel(MEMPOOL_APMAP, reg_val);

	/* Invalidate L2IMEM. */
	reg_val = xusbh_csb_readl(MEMPOOL_L2IMEMOP_TRIG);
	reg_val &= ~ACTION_MASK;
	reg_val |= ACTION_L2IMEM_INVALIDATE_ALL;
	xusbh_csb_writel(MEMPOOL_L2IMEMOP_TRIG, reg_val);

	/* Program bootcode location and size in system memory. */
	code_tag_blocks  = DIV_ROUND_UP(fw_header->boot_codetag,
					IMEM_BLOCK_SIZE);
	code_size_blocks = DIV_ROUND_UP(fw_header->boot_codesize,
					IMEM_BLOCK_SIZE);
	reg_val = ((code_tag_blocks & SRC_OFFSET_MASK) << SRC_OFFSET_SHIFT) |
		((code_size_blocks & SRC_COUNT_MASK) << SRC_COUNT_SHIFT);
	xusbh_csb_writel(MEMPOOL_L2IMEMOP_SIZE, reg_val);

	/* Trigger L2IMEM Load operation. */
	reg_val = xusbh_csb_readl(MEMPOOL_L2IMEMOP_TRIG);
	reg_val &= ~ACTION_MASK;
	reg_val |= ACTION_L2IMEM_LOAD_LOCKED_RESULT;
	xusbh_csb_writel(MEMPOOL_L2IMEMOP_TRIG, reg_val);

	/* Setup Falcon Auto-fill. */
	xusbh_csb_writel(FALCON_IMFILLCTL, code_size_blocks);

	reg_val = ((code_tag_blocks & TAG_MASK) << TAG_LO_SHIFT) |
		(((code_size_blocks + code_tag_blocks) & TAG_MASK) << TAG_HI_SHIFT);
	xusbh_csb_writel(FALCON_IMFILLRNG1, reg_val);

	pr_info("USB Firmware Version: %2x.%02x %s\n",
			(fw_header->version_id >> 24) & 0xff,
			(fw_header->version_id >> 16) & 0xff,
			(fw_header->build_log == 1) ? "debug" : "release");

	xusbh_csb_writel(FALCON_DMACTL, 0);
	tegrabl_mdelay(50);

	/* wait for RESULT_VLD to get set */
	timeout = 1000;
	while (timeout > 0) {
		reg_val = xusbh_csb_readl(MEMPOOL_L2IMEMOP_RESULT);
		if ((reg_val & VLD_SET) == VLD_SET)
			break;
		timeout--;
		tegrabl_udelay(10);
	};

	if (timeout == 0) {
		pr_error("DMA controller not ready\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
		return err;
	}

	xusbh_csb_writel(FALCON_BOOTVEC, fw_header->boot_codetag);

	/* Start Falcon CPU */
	xusbh_csb_writel(FALCON_CPUCTL, STARTCPU);
	tegrabl_udelay(2000);

	/* wait for USBSTS_CNR to get cleared */
	timeout = 200;
	do {
		reg_val = xusbh_xhci_readl(OP_USBSTS);
		if ((reg_val & STS_CNR) == 0)
			break;
		tegrabl_mdelay(1);
		timeout--;
	} while (timeout > 0);

#if DEBUG_ENABLE
	pr_debug("HS port 0 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x116800), xusbh_csb_readl(0x116804), xusbh_csb_readl(0x116808));
	pr_debug("HS port 1 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x116810), xusbh_csb_readl(0x116814), xusbh_csb_readl(0x116818));
	pr_debug("HS port 2 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x116820), xusbh_csb_readl(0x116824), xusbh_csb_readl(0x116828));
	pr_debug("HS port 3 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x116830), xusbh_csb_readl(0x116834), xusbh_csb_readl(0x116838));

	pr_debug("FS port 0 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x117000), xusbh_csb_readl(0x117004), xusbh_csb_readl(0x117008));
	pr_debug("FS port 1 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x117010), xusbh_csb_readl(0x117014), xusbh_csb_readl(0x117018));
	pr_debug("FS port 2 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x117020), xusbh_csb_readl(0x117024), xusbh_csb_readl(0x117028));
	pr_debug("FS port 3 status: 0x%x, 0x%x, 0x%x\n",
		xusbh_csb_readl(0x117030), xusbh_csb_readl(0x117034), xusbh_csb_readl(0x117038));
#endif

	if ((xusbh_xhci_readl(OP_USBSTS) & STS_CNR) != 0) {
		pr_error("XHCI Controller not ready. Falcon state: 0x%x\n",
						xusbh_csb_readl(FALCON_CPUCTL));

		err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
#if DEBUG_ENABLE
		/* Dump EXCI register */
		xusbh_csb_writel(0x200, 0x1c08);
		tegrabl_mdelay(10);
		pr_debug("EXCI is: 0x%x\n", xusbh_csb_readl(0x20c));
		pr_debug("vendor id is: 0x%x\n", NV_READ32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE));
		pr_debug("vendor id is: 0x%x\n", NV_READ32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + 0x8000));
		pr_debug("L2IMEM is: 0x%x\n", xusbh_csb_readl(0x010180c));
		pr_debug("fw_data addr is: %p\n", fw_data);
		dump_clocks();
#endif
	}

done:
	return err;
}

void xhci_dump_fw_log(void)
{
#if ENABLE_FW_LOG
	uint32_t i;
	uint32_t *log_buf = (uint32_t *)fw_log;

	pr_error("dumping fw log buffer %p\n", fw_log);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_HOST_CFG_BASE + 0x440, 0x07000000);
	tegrabl_mdelay(10000);
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSB_HOST, 0, fw_log, NUM_LOG_ENTRIES * 32,
							 TEGRABL_DMA_BIDIRECTIONAL);
	for (i = 0; i < (64 * 8); i += 8) {
		pr_error("log entry %d: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", (i / 8) + 1, log_buf[i], log_buf[i + 1],
			log_buf[i + 2], log_buf[i + 3], log_buf[i + 4], log_buf[i + 5], log_buf[i + 6],
			log_buf[i + 7]);
	}
#endif
}
