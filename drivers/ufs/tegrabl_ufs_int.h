/*
 * Copyright (c) 2016-2017 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _TEGRABL_UFS_INT_H
#define _TEGRABL_UFS_INT_H

#include <tegrabl_ufs_defs.h>

//#define MAX_TRD_NUM    8
//#define MAX_TMD_NUM    8
#define UFS_BLOCK_SIZE_LOG2  12
#define UFS_PAGE_SIZE_LOG2   12
#define SCSI_REQ_READ_TIMEOUT      1000000
#define SCSI_REQ_ERASE_TIMEOUT      1000000000

/* Create UFS context structure
*/
struct trdinfo {
	uint32_t  trd_timeout_in_us;
	uint32_t trd_starttime;
	uint32_t trd_timeout;
};

struct tegrabl_ufs_internal_params {
	uint32_t boot_enabled;
	uint32_t page_size_log2;
	uint32_t boot_lun;
	uint32_t num_lanes;
};

struct tegrabl_ufs_context {
	/* Start: Device Stuff obtained from descriptors */
	uint32_t boot_lun_num_blocks;
	uint32_t boot_lun_block_size;
	uint32_t boot_enabled;
	uint32_t num_lun;
	uint32_t num_wlu;
	/* End: Device Stuff obtained from descriptors */
	/* Start: Device stuff obtained from fuses, bct */
	uint32_t boot_lun;
	uint32_t block_size_log2;
	uint32_t page_size_log2;
	uint32_t active_lanes;
	uint32_t num_lanes;
	/* End: Device stuff obtained from fuses, bct */
	/* Start: House keeping */
	uint32_t init_done;
	uint32_t current_pwm_gear;
	uint32_t cmd_desc_in_use;
	uint32_t last_cmd_desc_index;
	uint32_t tx_req_des_in_use;;
	uint32_t last_trd_index;
	struct trdinfo trd_info[MAX_TRD_NUM];
	/* End: House keeping */
};

tegrabl_error_t tegrabl_ufs_init(const struct tegrabl_ufs_params *params,
	struct tegrabl_ufs_context *context);
tegrabl_error_t tegrabl_ufs_rw_common(const uint32_t block, const uint32_t page,
			const uint32_t length, uint32_t *pbuffer, uint32_t direction);
tegrabl_error_t tegrabl_ufs_read(const uint32_t block, const uint32_t page,
	const uint32_t length, uint32_t *pbuffer);
tegrabl_error_t tegrabl_ufs_write(const uint32_t block, const uint32_t page,
	const uint32_t length, uint32_t *pbuffer);
void tegrabl_ufs_get_params(const uint32_t param_index,
	struct tegrabl_ufs_params **params);
bool tegrabl_ufs_validate_params(const struct tegrabl_ufs_params *Params);
void tegrabl_ufs_get_block_sizes(const struct tegrabl_ufs_params *params,
	uint32_t *block_size_log2, uint32_t *page_size_log2);
void tegrabl_ufs_shutdown(void);
tegrabl_error_t tegrabl_ufs_pinmux_init(const void *params);
tegrabl_error_t tegrabl_ufs_change_num_lanes(void);
tegrabl_error_t tegrabl_ufs_change_gear(uint32_t gear);
tegrabl_error_t tegrabl_ufs_get_cmd_descriptor(uint32_t *pcmddescindex);
tegrabl_error_t  tegrabl_ufs_pollfield(uint32_t reg_addr,
		uint32_t mask, uint32_t expected_value, uint32_t timeout);
tegrabl_error_t tegrabl_ufs_hw_init(uint32_t re_init);
#endif
