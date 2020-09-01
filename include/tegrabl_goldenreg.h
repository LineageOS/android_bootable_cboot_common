/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_GR_H
#define INCLUDED_TEGRABL_GR_H

/**
 * @brief Declares offsets and sizes of Golden register dump
 * for all bootloader components in the Golden Register carveout
 */
struct tegrabl_gr_hdr {
	uint32_t mb1_offset;
	uint32_t mb1_size;
	uint32_t mb2_offset;
	uint32_t mb2_size;
	uint32_t cpu_bl_offset;
	uint32_t cpu_bl_size;
};


/**
 * @brief Declares Golden Register Address and value for that
 * address that is dumped into Golden Register Carveout
 */
struct tegrabl_gr_value {
	/* Register Address */
	uint32_t gr_address;
	/* Value corresponding to this address */
	uint32_t gr_value;
};

/**
 * @brief Declares bootloader component stages at which call to
 * tegrabl_dump_golden_regs is made
 */
typedef uint32_t tegrabl_gr_state_t;
#define	TEGRABL_GR_MB1 0
#define	TEGRABL_GR_MB2 1
#define	TEGRABL_GR_CPU_BL 2

#define TEGRABL_GR_CARVEOUT_SIZE (64 * 1024)


/**
 * @brief dumps the golden registers specified by golden_reg.h
 */
void tegrabl_dump_golden_regs(tegrabl_gr_state_t state, uint64_t start);


/**
 * @brief Defines parameters that is encoded into the GR Blob.
 * GR Blob is appended to the end of MB and QB binary during flash
 *
 * NAME_LEN  - Max length of the MB1/MB2/QB binary
 * SIG_LEN   - Length of the Signature used to validate the GR blob
 * SIGNATURE - String that is used to validate the GR blob
 * MAX_BIN   - Max number of binary info in the GR blob
 */
#define NAME_LEN 8
#define SIG_LEN 8
#define SIGNATURE "GOLDENR"
#define MAX_BIN 2

/**
 * @brief Defines parameters that is used to represent the GR information
 * in the GR blob associated with a bootloader component
 */
struct BinaryInfo {
	/* Name of the Bootloader component */
	uint8_t Name[NAME_LEN];
	/* Offset from which the GR registers starts */
	uint32_t Offset;
	/* Size of the GR registers that is in the blob */
	uint32_t Size;
};

/**
 * @brief Defines parameters that make up the GR Blob
 */
struct GrBlob {
	/* String that is used to validate the GR blob */
	uint8_t Signature[SIG_LEN];
	/* Number of Binaries in the GR Blob */
	uint32_t NumBinaries;
	/* Binary Information associated with each binary */
	struct BinaryInfo Binaryinfo[MAX_BIN];
	/* Pointer to the list of register addresses */
	uint32_t *Data;
};

#endif /* INCLUDED_TEGRABL_GR_H */
