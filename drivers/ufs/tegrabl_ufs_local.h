/*
 * Copyright (c) 2017-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _TEGRABL_UFS_LOCAL_H
#define _TEGRABL_UFS_LOCAL_H
#include <tegrabl_drf.h>
#include <tegrabl_error.h>


#define TEGRABL_UFS_BUF_ALIGN_SIZE 8U

/* Align macros */
#define CEIL_PAGE(LEN, PAGE_SIZE)  (((LEN)+(PAGE_SIZE)-1)/(PAGE_SIZE))
#define ALIGN_LEN(LEN, BYTES) ((((LEN)+(BYTES)-1)/(BYTES)) * (BYTES))
#define ALIGN_ADDR(ADDR, BYTES) ((((ADDR)+(BYTES)-1)/(BYTES)) * (BYTES))

/** READ/WRITE MACROS **/
#define BIT_MASK(REGFLD)  ((1UL << REGFLD##_REGISTERSIZE) - 1U)
#define SHIFT(REGFLD) (REGFLD##_BITADDRESSOFFSET)
#define SHIFT_MASK(REGFLD) (BIT_MASK(REGFLD) << SHIFT(REGFLD))
#define SET_FLD(REGFLD, VAL, REGDATA) \
	(((REGDATA) & ~SHIFT_MASK(REGFLD)) | ((VAL) << SHIFT(REGFLD)))
#define READ_FLD(REGFLD, REGDATA) \
	(((REGDATA) & SHIFT_MASK(REGFLD)) >> SHIFT(REGFLD))


/* Setting large timeouts. */
#define HCE_SET_TIMEOUT             500000
#define UTRLRDY_SET_TIMEOUT         500000
#define UTMRLRDY_SET_TIMEOUT        500000
#define IS_UCCS_TIMEOUT             500000
#define IS_UPMS_TIMEOUT             500000
#define NOP_TIMEOUT                 500000
#define QUERY_REQ_DESC_TIMEOUT      200000000
#define QUERY_REQ_FLAG_TIMEOUT      100000000
#define QUERY_REQ_ATTRB_TIMEOUT     500000
#define REQUEST_SENSE_TIMEOUT       500000

#define UFS_READ32(REG) NV_READ32(REG)
#define UFS_WRITE32(REG, VALUE) NV_WRITE32(REG, VALUE)


/** Static structure of TRDs in system memory aligned to 1KB boundary
 */
#define NEXT_TRD_IDX(idx) (((idx) == ((MAX_TRD_NUM) - 1U)) ? 0UL : ((idx) + 1U))

#if defined(CONFIG_UFS_MAX_CMD_DESCRIPTORS)
#define MAX_CMD_DESC_NUM	CONFIG_UFS_MAX_CMD_DESCRIPTORS
#else
#define MAX_CMD_DESC_NUM	8UL
#endif

#define NEXT_CD_IDX(idx) (((idx) == ((MAX_CMD_DESC_NUM) - 1U)) ? 0U : ((idx) + 1U))

#define SYSRAM_DIFFERENCE		0x0U

#define UFSHC_BLOCK_BASEADDRESS		38076416U
#define HCE				(UFSHC_BLOCK_BASEADDRESS + 0x34U)
#define HCS				(UFSHC_BLOCK_BASEADDRESS + 0x30U)
#define UICCMDARG1			(UFSHC_BLOCK_BASEADDRESS + 0x94U)
#define UICCMDARG2			(UFSHC_BLOCK_BASEADDRESS + 0x98U)
#define UICCMDARG3			(UFSHC_BLOCK_BASEADDRESS + 0x9cU)
#define UICCMD				(UFSHC_BLOCK_BASEADDRESS + 0x90U)
#define IS_UPMS_BITADDRESSOFFSET	4U
#define IS				(UFSHC_BLOCK_BASEADDRESS + 0x20U)
#define IS_UCCS_BITADDRESSOFFSET	10U
#define IS_UPMS_REGISTERSIZE		1U
#define IS_UCCS_REGISTERSIZE		1U
#define IS_UHES_BITADDRESSOFFSET	6U
#define IS_UHES_REGISTERSIZE		1U
#define IS_UHXS_BITADDRESSOFFSET	5U
#define IS_UHXS_REGISTERSIZE		1U

#define UTMRLRSR			(UFSHC_BLOCK_BASEADDRESS + 0x80U)
#define UTMRLRSR_UTMRLRSR_REGISTERSIZE	1U
#define UTMRLRSR_UTMRLRSR_BITADDRESSOFFSET 0U
#define UTRLRSR				(UFSHC_BLOCK_BASEADDRESS + 0x60U)
#define UTRLRSR_UTRLRSR_BITADDRESSOFFSET 0U
#define UTRLRSR_UTRLRSR_REGISTERSIZE	1U
#define HCS_UTMRLRDY_BITADDRESSOFFSET	2U
#define HCS_UTMRLRDY_REGISTERSIZE	1U
#define HCS_UTRLRDY_BITADDRESSOFFSET	1U
#define HCS_UTRLRDY_REGISTERSIZE	1U


#define UTRLBA				(UFSHC_BLOCK_BASEADDRESS + 0x50U)
#define UTRLBAU				(UFSHC_BLOCK_BASEADDRESS + 0x54U)
#define UTMRLBA				(UFSHC_BLOCK_BASEADDRESS + 0x70U)
#define UTMRLBAU			(UFSHC_BLOCK_BASEADDRESS + 0x74U)
#define UTRLDBR				(UFSHC_BLOCK_BASEADDRESS + 0x58U)
#define UECPA				(UFSHC_BLOCK_BASEADDRESS + 0x38U)
#define UECDL				(UFSHC_BLOCK_BASEADDRESS + 0x3cU)
#define UECN				(UFSHC_BLOCK_BASEADDRESS + 0x40U)
#define UECT				(UFSHC_BLOCK_BASEADDRESS + 0x44U)
#define UECDME				(UFSHC_BLOCK_BASEADDRESS + 0x48U)

#define IS_SBFES_REGISTERSIZE		1U
#define IS_SBFES_BITADDRESSOFFSET	17U
#define IS_HCFES_BITADDRESSOFFSET	16U
#define IS_HCFES_REGISTERSIZE		1U
#define IS_UTPES_BITADDRESSOFFSET	12U
#define IS_UTPES_REGISTERSIZE		1U
#define IS_DFES_BITADDRESSOFFSET	11U
#define IS_DFES_REGISTERSIZE		1U

#define HCE_REGISTERSIZE		32U
#define HCE_REGISTERRESETVALUE		0x0U
#define HCE_REGISTERRESETMASK		0xffffffffUL
#define HCE_HCE_BITADDRESSOFFSET	0U
#define HCE_HCE_REGISTERSIZE		1U
#define HCLKDIV				(UFSHC_BLOCK_BASEADDRESS + 0xfcU)
#define HCLKDIV_REGISTERSIZE		32U
#define HCLKDIV_REGISTERRESETVALUE	0xc8U
#define HCLKDIV_REGISTERRESETMASK	0xffffffffUL
#define HCLKDIV_HCLKDIV_BITADDRESSOFFSET 0U
#define HCLKDIV_HCLKDIV_REGISTERSIZE	16U
#define IS_UPMS_REGISTERSIZE		1U

#define UFSHC_AUX_UFSHC_SW_EN_CLK_SLCG_0	_MK_ADDR_CONST(0x8)
#define UFSHC_AUX_UFSHC_SW_EN_CLK_SLCG_0_UFSHC_CG_SYS_CLK_OVR_ON_RANGE	(4) : (4)

#define UFSHC_AUX_UFSHC_DEV_CTRL_0               _MK_ADDR_CONST(0x14)
#define UFSHC_AUX_UFSHC_DEV_CTRL_0_UFSHC_DEV_CLK_EN_RANGE   (0) : (0)
#define UFSHC_AUX_UFSHC_DEV_CTRL_0_UFSHC_DEV_RESET_RANGE    (1) : (1)


#define	UFSHC_AUX_UFSHC_STATUS_0	_MK_ADDR_CONST(0x10)
#define	UFSHC_HIBERN8_MASK		0x1U
#define	UFSHC_HIBERN8_ENTRY_STATUS	0x1U
#define	UFSHC_HIBERN8_EXIT_STATUS	0x0U



tegrabl_error_t tegrabl_ufs_link_uphy_setup(uint32_t num_lanes);
void tegrabl_ufs_link_uphy_deinit(uint32_t num_lanes);
tegrabl_error_t tegrabl_ufs_link_mphy_setup(void);
void tegrabl_ufs_uphy_clk_enable_reset_disable(uint32_t num_lanes);
void tegrabl_ufs_uphy_clk_disable_reset_enable(uint32_t num_lanes);

#endif

