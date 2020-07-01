/*
 * Copyright (c) 2017, NVIDIA Corporation. All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_BCT_AUX_INTERNAL_DATA_H
#define TEGRABL_BCT_AUX_INTERNAL_DATA_H

#include "nvcommon.h"

typedef struct NvBctAuxInternalDataRec
{
	/**
	 * Do not change the position or size of any element in this structure,
	 * relative to the \b end of the structure. In other words, add new fields
	 * at the start of this structure.
	 *
	 * This ensures that when this struct is embedded into the BCT at the end
	 * of the \c CustomerData[] field, all fields in this struct will be at the
	 * same offset from the start of the BCT.
	 *
	 * This is required so that the various consumers and producers of this
	 * data, not all of which are part of the NVIDIA SW tree, all agree on the
	 * BCT-relative location of the fields in this struct, and that location
	 * does not vary.
	 */
	NvU16 SmdDeviceType; /**< Device which has SMD */
	NvU16 SmdDeviceInstance; /**< Instance of device which has SMD */
	NvU32 SmdStartSector; /**< SMD partition stat sector */
	NvU32 SmdPartitionSize; /**< SMD partition size */
	NvU32 NVDumperAddress; /**< Saved carve out address */
	NvU32 CustomerOption2; /**< Also known as ODMDATA2. */
	NvU32 CustomerOption; /**< Also known as ODMDATA. */
	NvU8  BctPartitionId;
} NvBctAuxInternalData;

#endif /* TEGRABL_BCT_AUX_INTERNAL_DATA_H */
