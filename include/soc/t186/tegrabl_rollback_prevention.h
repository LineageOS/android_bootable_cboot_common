/*
 * Copyright (c) 2016-2022 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file tegrabl_rollback_prevention.h
 *
 * Defines the parameters and data structures related to rollback.
 */

#ifndef INCLUDE_TEGRABL_ROLLBACK_PREVENTION_H
#define INCLUDE_TEGRABL_ROLLBACK_PREVENTION_H

#if defined(__cplusplus)
extern "C"
{
#endif

TEGRABL_PACKED(
struct rollback_limits {
	const uint8_t boot;               /* BL's rollback level except mb1 */
	const uint8_t bpmp_fw;            /* bpmp-fw's rollback level */
	const uint8_t tos;                /* TLK and SM rollback level */
	const uint8_t tsec;               /* TSEC's rollback level */
	const uint8_t nvdec;              /* NVDEC's rollback level */
	const uint8_t srm;                /* SRM file rollback level */
	const uint8_t tsec_gsc_ucode;     /* GSC uCode rollback level */
	const uint8_t early_spe_fw;       /* Early SPE-FW's rollback level */
	const uint8_t extended_spe_fw;    /* Extended SPE-FW's rollback level */
	const uint8_t hdcp2srm;           /* srm rollback level, HDCP 2.2 */
	const uint8_t cpubl;              /* srm rollback level for cpubl (quickboot) */
}
);

/* Rollback struct is aligned to 64 bytes */
TEGRABL_PACKED(
struct tegrabl_rollback {
	const uint8_t version;            /* Version of the struct definition */
	uint8_t enabled;                  /* 1 -> rollback will be prevented */
	const uint8_t fuse_idx;           /* Idx in odm reserved fuses array */
	const uint8_t level;              /* mb1_bct's rollback level */
	const struct rollback_limits limits;
	uint8_t reserved[49];
}
);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDE_TEGRABL_ROLLBACK_PREVENTION_H */
