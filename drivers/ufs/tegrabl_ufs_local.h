/*
 * Copyright (c) 2017 NVIDIA Corporation.  All rights reserved.
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


/* Align macros */
#define CEIL_PAGE(LEN, PAGE_SIZE)  (((LEN)+(PAGE_SIZE)-1)/(PAGE_SIZE))
#define ALIGN_LEN(LEN, BYTES) ((((LEN)+(BYTES)-1)/(BYTES)) * (BYTES))
#define ALIGN_ADDR(ADDR, BYTES) ((((ADDR)+(BYTES)-1)/(BYTES)) * (BYTES))

/** READ/WRITE MACROS **/
#define BIT_MASK(REGFLD)  ((1 << REGFLD##_REGISTERSIZE) - 1)
#define SHIFT(REGFLD) (REGFLD##_BITADDRESSOFFSET)
#define SHIFT_MASK(REGFLD) (BIT_MASK(REGFLD) << SHIFT(REGFLD))
#define SET_FLD(REGFLD, VAL, REGDATA) \
	 ((REGDATA & ~SHIFT_MASK(REGFLD)) | (VAL << SHIFT(REGFLD)))
#define READ_FLD(REGFLD, REGDATA) \
	((REGDATA & SHIFT_MASK(REGFLD)) >> SHIFT(REGFLD))


/** Macros to convert endianness
 */
#define BYTE_SWAP32(a)   \
        ((((a)&0xff) << 24) | (((a)&0xff00)<< 8) | \
        (((a)&0xff0000) >> 8) | (((a)&0xff000000) >> 24))
#define BYTE_SWAP16(a)   \
        ( (((a)&0xff)<< 8) | (((a)&0xff00) >> 8))

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
#define NEXT_TRD_IDX(idx) (((idx) == ((MAX_TRD_NUM) - 1)) ? 0 : ((idx) + 1))

#define MAX_CMD_DESC_NUM   12
#define NEXT_CD_IDX(idx) (((idx) == ((MAX_CMD_DESC_NUM) - 1)) ? 0 : ((idx) + 1))

#define SYSRAM_DIFFERENCE   0x0

#define UFSHC_BLOCK_BASEADDRESS  38076416
#define HCE (UFSHC_BLOCK_BASEADDRESS + 0x34)
#define HCS (UFSHC_BLOCK_BASEADDRESS + 0x30)
#define UICCMDARG1 (UFSHC_BLOCK_BASEADDRESS + 0x94)
#define UICCMDARG2 (UFSHC_BLOCK_BASEADDRESS + 0x98)
#define UICCMDARG3 (UFSHC_BLOCK_BASEADDRESS + 0x9c)
#define UICCMD (UFSHC_BLOCK_BASEADDRESS + 0x90)
#define IS_UPMS_BITADDRESSOFFSET 4
#define IS (UFSHC_BLOCK_BASEADDRESS + 0x20)
#define IS_UCCS_BITADDRESSOFFSET 10
#define IS_UPMS_REGISTERSIZE 1
#define IS_UCCS_REGISTERSIZE 1
#define UTMRLRSR (UFSHC_BLOCK_BASEADDRESS + 0x80)
#define UTMRLRSR_UTMRLRSR_REGISTERSIZE 1
#define UTMRLRSR_UTMRLRSR_BITADDRESSOFFSET 0
#define UTRLRSR (UFSHC_BLOCK_BASEADDRESS + 0x60)
#define UTRLRSR_UTRLRSR_BITADDRESSOFFSET 0
#define UTRLRSR_UTRLRSR_REGISTERSIZE 1
#define HCS_UTMRLRDY_BITADDRESSOFFSET 2
#define HCS_UTMRLRDY_REGISTERSIZE 1
#define HCS_UTRLRDY_BITADDRESSOFFSET 1
#define HCS_UTRLRDY_REGISTERSIZE 1


#define UTRLBA (UFSHC_BLOCK_BASEADDRESS + 0x50)
#define UTRLBAU (UFSHC_BLOCK_BASEADDRESS + 0x54)
#define UTMRLBA (UFSHC_BLOCK_BASEADDRESS + 0x70)
#define UTMRLBAU (UFSHC_BLOCK_BASEADDRESS + 0x74)
#define UTRLDBR (UFSHC_BLOCK_BASEADDRESS + 0x58)
#define UECPA (UFSHC_BLOCK_BASEADDRESS + 0x38)
#define UECDL (UFSHC_BLOCK_BASEADDRESS + 0x3c)
#define UECN (UFSHC_BLOCK_BASEADDRESS + 0x40)
#define UECT (UFSHC_BLOCK_BASEADDRESS + 0x44)
#define UECDME (UFSHC_BLOCK_BASEADDRESS + 0x48)

#define IS_SBFES_REGISTERSIZE 1
#define IS_SBFES_BITADDRESSOFFSET 17
#define IS_HCFES_BITADDRESSOFFSET 16
#define IS_HCFES_REGISTERSIZE 1
#define IS_UTPES_BITADDRESSOFFSET 12
#define IS_UTPES_REGISTERSIZE 1
#define IS_DFES_BITADDRESSOFFSET 11
#define IS_DFES_REGISTERSIZE 1

#define HCE_REGISTERSIZE 32
#define HCE_REGISTERRESETVALUE 0x0
#define HCE_REGISTERRESETMASK 0xffffffff
#define HCE_HCE_BITADDRESSOFFSET 0
#define HCE_HCE_REGISTERSIZE 1
#define HCLKDIV (UFSHC_BLOCK_BASEADDRESS + 0xfc)
#define HCLKDIV_REGISTERSIZE 32
#define HCLKDIV_REGISTERRESETVALUE 0xc8
#define HCLKDIV_REGISTERRESETMASK 0xffffffff
#define HCLKDIV_HCLKDIV_BITADDRESSOFFSET 0
#define HCLKDIV_HCLKDIV_REGISTERSIZE 16
#define IS_UPMS_REGISTERSIZE 1

tegrabl_error_t tegrabl_ufs_link_uphy_setup(uint32_t num_lanes);
void tegrabl_ufs_link_uphy_deinit(uint32_t num_lanes);
tegrabl_error_t tegrabl_ufs_link_mphy_setup(void);

#endif

