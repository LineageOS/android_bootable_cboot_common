/*
 * Copyright (c) 2017 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef TEGRABL_UFS_OTP_H
#define TEGRABL_UFS_OTP_H
#include <tegrabl_error.h>

#define MAGICID_OTP 0x5F4F5450 /* "_otp" */
#define MANUFACTURE_TOSHIBA	0x0198
#define MANUFACTURE_SAMSUNG	0x01CE
#define MANUFACTURE_HYNIX	0x01AD
#define MANUFACTURE_UNKNOWN	0xFFFF

struct vendor_id_info {
	uint16_t id;
	char     name[8];
};

tegrabl_error_t tegrabl_ufs_common_upiu_write(uint8_t *pdesc_req_data,
	uint32_t data_size);
tegrabl_error_t tegrabl_ufs_get_manufacture_id(uint16_t *manufacture_id);
#endif
