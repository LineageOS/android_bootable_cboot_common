/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file tegrabl_pcie_soc_common.c
 */

#include <address_map_new.h>
#include <tegrabl_clock.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_timer.h>
#include <tegrabl_pcie_soc_common.h>
#include <tegrabl_module.h>

/** ATU Control Register-1 Offset */
#define PCIE_ATU_CR1					0x0UL
/** ATU Conversion type for Config access type-0 */
#define PCIE_ATU_TYPE_CFG0				0x4UL
/** ATU Conversion type for Config access type-1 */
#define PCIE_ATU_TYPE_CFG1				0x5UL
/** ATU control to increase region size access to >4GB */
#define PCIE_ATU_INCREASE_REGION_SIZE			(1UL << 13U)
/** ATU Control Register-2 Offset */
#define PCIE_ATU_CR2					0x4UL
/** ATU Bit to enable ATU functionality */
#define PCIE_ATU_ENABLE					(0x1UL << 31U)
/** ATU Offset to program lower base */
#define PCIE_ATU_LOWER_BASE				0x8U
/** ATU Offset to program Upper base */
#define PCIE_ATU_UPPER_BASE				0xCU
/** ATU Offset to program lower limit */
#define PCIE_ATU_LIMIT					0x10U
/** ATU Offset to program lower target */
#define PCIE_ATU_LOWER_TARGET				0x14U
/** ATU Offset to program upper target */
#define PCIE_ATU_UPPER_TARGET				0x18U
/** ATU Offset to program upper limit */
#define PCIE_ATU_UPPER_LIMIT				0x20U

/** Define for 128KB size */
#define SZ_128K 0x00020000

/** ATU region index (for accessing config space) */
#define PCIE_ATU_REGION_INDEX0  0

/** PCIe Controller-0 */
#define PCIE_CTRL_0		0U
/** PCIe Controller-1 */
#define PCIE_CTRL_1		1U
/** PCIe Controller-2 */
#define PCIE_CTRL_2		2U
/** PCIe Controller-3 */
#define PCIE_CTRL_3		3U
/** PCIe Controller-4 */
#define PCIE_CTRL_4		4U
/** PCIe Controller-5 */
#define PCIE_CTRL_5		5U
/** PCIe Controller-6 */
#define PCIE_CTRL_6		6U
/** PCIe Controller-7 */
#define PCIE_CTRL_7		7U
/** PCIe Controller-8 */
#define PCIE_CTRL_8		8U
/** PCIe Controller-9 */
#define PCIE_CTRL_9		9U
/** PCIe Controller-10 */
#define PCIE_CTRL_10		10U
/** PCIe Controller-11 */
#define PCIE_CTRL_MAX		11U

#define NV_CLK_RST_READ_OFFSET(offset)						\
	NV_READ32(((uintptr_t)(NV_ADDRESS_MAP_CAR_BASE)) +		\
			((uint32_t)offset))

#define NV_CLK_RST_WRITE_OFFSET(offset, value)				\
	NV_WRITE32(((uintptr_t)(NV_ADDRESS_MAP_CAR_BASE)) +		\
			((uint32_t)offset), (value))

extern uint32_t max_ctrl_supported;

/**
 * @brief Allocate memory for structures that represent the PCIe Host
 * controllers that exist in a given system
 */
static struct tegrabl_tegra_pcie tegra_pcie_info[PCIE_CTRL_MAX];
/**
 * @brief Allocate memory for structures that represent the PCIe bus started
 * by each PCIe Host controller that exist in a given system
 */
static struct tegrabl_pcie_bus tegra_pcie_bus[PCIE_CTRL_MAX];

/** Macro to fit bus number in Bus:Device:Function format */
#define PCIE_ATU_BUS(x)		(((x) & 0xffUL) << 24U)
/** Macro to fit device and function numbers in Bus:Device:Function format */
#define PCIE_ATU_DEVFN(x)	(((x) & 0xffUL) << 16U)

/** Macro to extract upper 32-bits of a 64-bit value */
#define upper_32_bits(n)	((uint32_t)(((n) >> 16) >> 16))
/** Macro to extract lower 32-bits of a 64-bit value */
#define lower_32_bits(n)	((uint32_t)(n))

/**
 * @brief Helper API to Program an ATU register
 *
 * @param[in] pdev Pointer to PCIe device
 * @param[in] i Index of ATU region
 * @param[in] val Value to be written to ATU register
 * @param[in] reg Offset to register in ATU region
 */
static inline void prog_atu(struct tegrabl_pcie_dev *pdev, uint32_t i, uint32_t val, uint32_t reg)
{
	struct tegrabl_tegra_pcie *pdata = (struct tegrabl_tegra_pcie *)pdev->pdata;

	NV_WRITE32(pdata->atu_dma_base + (i * 0x200UL) + reg, val);
}

/**
 * @brief API to configure outbound region of an ATU
 *
 * @param[in] pdev Pointer to PCIe device
 * @param[in] i Index of ATU region
 * @param[in] type Translation type
 * @param[in] busdev Bus:Device number of the PCIe device
 */
static void outbound_atu(struct tegrabl_pcie_dev *pdev, uint32_t i, uint32_t type, uint32_t busdev)
{
	struct tegrabl_tegra_pcie *pdata = (struct tegrabl_tegra_pcie *)pdev->pdata;

	prog_atu(pdev, i, lower_32_bits(pdata->cfg1_base), PCIE_ATU_LOWER_BASE);
	prog_atu(pdev, i, upper_32_bits(pdata->cfg1_base), PCIE_ATU_UPPER_BASE);
	prog_atu(pdev, i, lower_32_bits(pdata->cfg1_base + pdata->cfg1_size - 1UL), PCIE_ATU_LIMIT);
	prog_atu(pdev, i, upper_32_bits(pdata->cfg1_base + pdata->cfg1_size - 1UL), PCIE_ATU_UPPER_LIMIT);
	prog_atu(pdev, i, lower_32_bits(busdev), PCIE_ATU_LOWER_TARGET);
	prog_atu(pdev, i, upper_32_bits(busdev), PCIE_ATU_UPPER_TARGET);
	prog_atu(pdev, i, type | PCIE_ATU_INCREASE_REGION_SIZE, PCIE_ATU_CR1);
	prog_atu(pdev, i, PCIE_ATU_ENABLE, PCIE_ATU_CR2);
}

/**
 * @brief API to read configuration space of a PCIe device
 *
 * @param[in] pdev Pointer to PCIe device
 * @param[in] offset Offset in the config space to read
 * @param[in] val pointer to store the read value
 */
static void tegrabl_pcie_soc_conf_read(struct tegrabl_pcie_dev *pdev, int offset, uint32_t *val)
{
	struct tegrabl_tegra_pcie *pdata = (struct tegrabl_tegra_pcie *)pdev->pdata;
	uint32_t type;
	uint32_t busdev;

	/* busnr == 0 is root port */
	if (pdev->busnr == 0UL) {
		*val = NV_READ32(pdata->cfg0_base + ((uint32_t)offset));
	} else {
		busdev = PCIE_ATU_BUS(pdev->busnr) | PCIE_ATU_DEVFN(pdev->devfn);

		/* busnr == 1 is immediate child */
		if (pdev->busnr == 1UL) {
			type = PCIE_ATU_TYPE_CFG0;
		} else {
			type = PCIE_ATU_TYPE_CFG1;
		}

		outbound_atu(pdev, PCIE_ATU_REGION_INDEX0, type, busdev);
		*val = NV_READ32(pdata->cfg1_base + ((uint32_t)offset));
	}
}

/**
 * @brief API to write configuration space of a PCIe device
 *
 * @param[in] pdev Pointer to PCIe device
 * @param[in] offset Offset in the config space to write
 * @param[in] val Value to be written
 */
static void tegrabl_pcie_soc_conf_write(struct tegrabl_pcie_dev *pdev, int offset, uint32_t val)
{
	struct tegrabl_tegra_pcie *pdata = (struct tegrabl_tegra_pcie *)pdev->pdata;
	uint32_t type;
	uint32_t busdev;

	/* busnr == 0 is root port */
	if (pdev->busnr == 0UL) {
		NV_WRITE32(pdata->cfg0_base + ((uint32_t)offset), val);
	} else {
		busdev = PCIE_ATU_BUS(pdev->busnr) | PCIE_ATU_DEVFN(pdev->devfn);

		/* busnr == 1 is immediate child */
		if (pdev->busnr == 1UL) {
			type = PCIE_ATU_TYPE_CFG0;
		} else {
			type = PCIE_ATU_TYPE_CFG1;
		}

		outbound_atu(pdev, PCIE_ATU_REGION_INDEX0, type, busdev);
		NV_WRITE32(pdata->cfg1_base + ((uint32_t)offset), val);
	}
}

/**
 * @brief Performs initialization of the Host PCIE controller.
 *
 * @param[in] ctrl_num Controller number which needs to be initialized
 * @param[in] flags Specifies configuration information like link speed Etc.
 * @param[out] bus Pointer to pointer of a PCIe bus structure
 * @param[out] pdata Pointer to PCIe private data structure
 *
 * @return TEGRABL_NO_ERROR if successful, appropriate error otherwise
 */
tegrabl_error_t tegrabl_pcie_soc_host_init(uint8_t ctrl_num, uint32_t flags, struct tegrabl_pcie_bus **bus,
										   void **pdata)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t link_speed;
	uint32_t *dbi_reg_offset;
	uint32_t *iatu_dma_offset;
	uint32_t *pcie_io_base;
	uint32_t *pcie_mem_base;

	if (ctrl_num >= max_ctrl_supported) {
		pr_error("Controller number is invalid\n");
		return TEGRABL_ERR_NOT_SUPPORTED;
	}

	link_speed = flags & 0x7;
	/** Currently only Gen-1 speed is supported */
	if (link_speed != 1) {
		pr_error("Target link speed-%d is not supported\n", link_speed);
		return TEGRABL_ERR_NOT_SUPPORTED;
	}

	error = tegrabl_pcie_soc_preinit(ctrl_num);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed tegrabl_pcie_soc_preinit(), error=0x%x\n", error);
		return error;
	}

	dbi_reg_offset = tegrabl_pcie_get_dbi_reg();
	iatu_dma_offset = tegrabl_pcie_get_iatu_reg();
	pcie_io_base = tegrabl_pcie_get_io_base();
	pcie_mem_base = tegrabl_pcie_get_mem_base();

	if ((dbi_reg_offset == NULL) || (iatu_dma_offset == NULL) || (pcie_io_base == NULL) ||
		(pcie_mem_base == NULL)) {
		return TEGRABL_ERR_INVALID;
	}

	error = tegrabl_pcie_soc_init(ctrl_num, link_speed);
	if (error != TEGRABL_NO_ERROR) {
		pr_warn("Failed tegrabl_pcie_soc_init(), error=0x%x\n", error);
		return error;
	}

	/**
	 * Store config base address in host structure to get it back
	 * from PCIe subsystem during read/write config.
	 * cfg0_base: Root port config base address which is same as DBI base
	 * cfg0_size: Root port config size is 4 KB, however setting as side of 128 KB
	 * cfg1_base: Endpoint config base address which is DBI base + 128 KB
	 * cfg1_size: Endpoint config size is 4 KB per BDF, setting as side of 128 KB
	 */
	tegra_pcie_info[ctrl_num].ctrl_num = ctrl_num;
	tegra_pcie_info[ctrl_num].cfg0_base = dbi_reg_offset[ctrl_num];
	pr_info("tegra_pcie_info[%d].cfg0_base = 0x%08X\n", ctrl_num, tegra_pcie_info[ctrl_num].cfg0_base);
	tegra_pcie_info[ctrl_num].cfg0_size = SZ_128K;
	tegra_pcie_info[ctrl_num].cfg1_base = dbi_reg_offset[ctrl_num] + tegra_pcie_info[ctrl_num].cfg0_size;
	pr_info("tegra_pcie_info[%d].cfg1_base = 0x%08X\n", ctrl_num, tegra_pcie_info[ctrl_num].cfg1_base);
	tegra_pcie_info[ctrl_num].cfg1_size = SZ_128K;
	tegra_pcie_info[ctrl_num].atu_dma_base = iatu_dma_offset[ctrl_num];
	pr_info("tegra_pcie_info[%d].atu_dma_base = 0x%08X\n", ctrl_num, tegra_pcie_info[ctrl_num].atu_dma_base);

	/** Populate PCIe bus structure */
	tegra_pcie_bus[ctrl_num].read = tegrabl_pcie_soc_conf_read;
	tegra_pcie_bus[ctrl_num].write = tegrabl_pcie_soc_conf_write;
	tegra_pcie_bus[ctrl_num].io = pcie_io_base[ctrl_num];
	tegra_pcie_bus[ctrl_num].io_size = PCIE_IO_SIZE;
	tegra_pcie_bus[ctrl_num].mem = pcie_mem_base[ctrl_num];
	pr_info("tegra_pcie_bus[%d].mem = 0x%08X\n", ctrl_num, tegra_pcie_bus[ctrl_num].mem);
	tegra_pcie_bus[ctrl_num].mem_size = PCIE_MEM_SIZE;

	*bus = &tegra_pcie_bus[ctrl_num];
	*pdata = (void *)&tegra_pcie_info[ctrl_num];

	if (*bus == NULL || *pdata == NULL) {
		error = TEGRABL_ERR_INIT_FAILED;
	}

	return error;
}

tegrabl_error_t tegrabl_pcie_reset_state(uint8_t ctrl_num)
{
	pr_trace("%s:\n", __func__);

	tegrabl_pcie_soc_pme_turnoff(ctrl_num);
	tegrabl_car_rst_set(TEGRABL_MODULE_PCIE_CORE, ctrl_num);
	tegrabl_car_rst_set(TEGRABL_MODULE_PCIE_APB, ctrl_num);
	tegrabl_car_clk_disable(TEGRABL_MODULE_PCIE_CORE, ctrl_num);
	tegrabl_set_ctrl_state(ctrl_num, false);
	tegrabl_pcie_soc_powergate(ctrl_num);
	tegrabl_pcie_disable_regulators(ctrl_num);

	return TEGRABL_NO_ERROR;
}
