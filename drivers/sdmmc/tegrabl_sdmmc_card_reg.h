/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMMC_CARD_REG_H
#define TEGRABL_SDMMC_CARD_REG_H


#define RESPONSE_BUFFER_SIZE					16
#define OCR_READY_MASK							0x80000000U
#define CARD_CAPACITY_MASK						0x40000000U
#define RCA_OFFSET								16

/* sdmmc CSD fields */
#define CSD_SPEC_VERS_MASK						0x3C0000U
#define CSD_SPEC_VERS_SHIFT						18
#define CSD_TRAN_SPEED_MASK						0xFF000000U
#define CSD_TRAN_SPEED_SHIFT					24
#define CSD_READ_BL_LEN_MASK					0xF00U
#define CSD_READ_BL_LEN_SHIFT					8
#define CSD_V4_3_TRAN_SPEED						0x32U
#define CSD_0_C_SIZE_0_RANGE					(31 : 22)
#define CSD_0_C_SIZE_1_RANGE					(1 : 0)
#define CSD_C_SIZE_1_LEFT_SHIFT_OFFSET			10
#define CSD_0_C_SIZE_MULTI_RANGE				(9 : 7)
#define CSD_MAX_C_SIZE							0xFFF
#define CSD_MAX_C_SIZE_MULTI					0x7
#define CSD_UHS50_TRAN_SPEED					0x0B
#define CSD_UHS104_TRAN_SPEED					0x2B

/* sdmmc Extended CSD fields. */
#define ECSD_BUFFER_SIZE						512
#define ECSD_BC_BPE_RANGE						(5 : 3)
#define ECSD_BC_BPE_OFFSET						3
#define ECSD_BC_BPE_MASK						0x7U
#define ECSD_BC_BPE_NOTENABLED					0
#define ECSD_BC_BPE_BAP1						1U
#define ECSD_BC_BPE_BAP2						2U
#define ECSD_BC_BPE_UAE							7
#define ECSD_BOOT_BUS_WIDTH						177
#define ECSD_CT_HS_DDR_52_120					8U
#define ECSD_CT_HS_DDR_52_180_300				4U
#define ECSD_CT_HS_DDR_52_120_MASK				0x8U
#define ECSD_CT_HS_DDR_52_180_300_MASK			0x4U
#define ECSD_CT_HS400_180						0x40U
#define ECSD_CT_HS400_180_MASK					0x40U
#define ECSD_CT_HS_DDR_RANGE					(3 : 2)
#define ECSD_CT_HS_DDR_OFFSET					2
#define ECSD_CT_HS_DDR_MASK						0xCU
#define ECSD_CT_HS_52							2
#define ECSD_CT_HS_26							1
#define ECSD_HS_TIMING_OFFSET					185
#define ECSD_POWER_CLASS_MASK					0xFU
#define ECSD_POWER_CLASS_4_BIT_OFFSET			0
#define ECSD_POWER_CLASS_8_BIT_OFFSET			4
#define ECSD_SECTOR_COUNT_0_OFFSET				212
#define ECSD_SECTOR_COUNT_1_OFFSET				213
#define ECSD_SECTOR_COUNT_2_OFFSET				214
#define ECSD_SECTOR_COUNT_3_OFFSET				215
#define ECSD_POWER_CL_26_360_OFFSET				203
#define ECSD_POWER_CL_52_360_OFFSET				202
#define ECSD_POWER_CL_26_195_OFFSET				201
#define ECSD_POWER_CL_52_195_OFFSET				200
#define ECSD_POWER_CL_DDR_52_360_OFFSET			239
#define ECSD_POWER_CL_DDR_52_195_OFFSET			238
#define ECSD_CARD_TYPE_OFFSET					196
#define ECSD_POWER_CLASS_OFFSET					187
#define ECSD_HS_TIMING_OFFSET					185
#define ECSD_BUS_WIDTH							183
#define ECSD_BOOT_CONFIG_OFFSET					179
#define ECSD_BOOT_PARTITION_SIZE_OFFSET			226
#define ECSD_POWER_CLASS_4_BIT_OFFSET			0
#define ECSD_POWER_CLASS_8_BIT_OFFSET			4
#define ECSD_SEC_FEATURE_OFFSET					231
#define ECSD_SEC_SANITIZE_MASK					0x40U
#define ECSD_SEC_SANITIZE_SHIFT					6
#define ECSD_ERASE_GROUP_DEF					175
#define ECSD_HIGH_CAP_ERASE_MASK				0x1
#define ECSD_ERASE_GRP_SIZE						224
#define ECSD_ERASE_TIMEOUT_OFFSET				223
#define ECSD_RPMB_SIZE_OFFSET					168
#define ECSD_REV								192

/* sdmmc switch command arg */
#define SWITCH_HIGH_SPEED_ENABLE_ARG			0x03b90100
#define SWITCH_HIGH_SPEED_DISABLE_ARG			0x03b90000
#define SWITCH_BUS_WIDTH_ARG					0x03b70000u
#define SWITCH_BUS_WIDTH_OFFSET					8
#define SWITCH_1BIT_BUS_WIDTH_ARG				0x03b70000
#define SWITCH_4BIT_BUS_WIDTH_ARG				0x03b70100
#define SWITCH_8BIT_BUS_WIDTH_ARG				0x03b70200u
#define SWITCH_SELECT_PARTITION_ARG				0x03b30000u
#define SWITCH_SELECT_PARTITION_MASK			0x7U
#define SWITCH_SELECT_PARTITION_OFFSET			0x8
#define SWITCH_4BIT_DDR_BUS_WIDTH_ARG			0x03b70500
#define SWITCH_8BIT_DDR_BUS_WIDTH_ARG			0x03b70600
#define SWITCH_CARD_TYPE_ARG					0x03C20000
#define SWITCH_INDEX_OFFSET						8
#define SWITCH_FUNCTION_TYPE_ARG			0x80000000UL
#define SWITCH_CURRENT_OFFSET					12
#define SWITCH_DRIVESTRENGHT_OFFSET				8
#define SWITCH_ACCESSMODE_OFFSET				0
#define SWITCH_SELECT_POWER_CLASS_ARG			0x03bb0000u
#define SWITCH_SELECT_POWER_CLASS_OFFSET		8
#define SWITCH_SANITIZE_ARG					0x03A50100U
#define SWITCH_HIGH_CAPACITY_ERASE_ARG			0x03AF0100
#define WRITE_BYTE								0x03

/* Card status fields. */
#define CS_ADDRESS_OUT_OF_RANGE_MASK			0x80000000U
#define CS_ADDRESS_OUT_OF_RANGE_SHIFT			31
#define CS_ADDRESS_MISALIGN_MASK				0x40000000U
#define CS_ADDRESS_MISALIGN_SHIFT				30
#define CS_BLOCK_LEN_ERROR_MASK					0x20000000U
#define CS_BLOCK_LEN_ERROR_SHIFT				29
#define CS_COM_CRC_ERROR_MASK					0x800000U
#define CS_COM_CRC_ERROR_SHIFT					23
#define CS_ILLEGAL_CMD_MASK						0x400000U
#define CS_ILLEGAL_CMD_SHIFT					22
#define CS_CURRENT_STATE_MASK					0x1E00U
#define CS_CURRENT_STATE_SHIFT					9
#define CS_CC_ERROR_MASK						0x100000U
#define CS_CC_ERROR_SHIFT						20
#define CS_SWITCH_ERROR_MASK					0x80U
#define CS_SWITCH_ERROR_SHIFT					7
#define CS_CARD_ECC_FAILED_MASK					0x200000U
#define CS_CARD_ECC_FAILED_SHIFT				21
#define CS_ERASE_CMD_ERROR_MASK					0x10002000U
#define CS_TRANSFER_STATE_MASK					0x1E00U
#define CS_TRANSFER_STATE_SHIFT					9UL

/* RPMB frame size in bytes. */
#define RPMB_FRAME_SIZE						512

/* CID Manufacturing ID info */
#define MANUFACTURING_ID_MASK					0xFF0000U
#define MANUFACTURING_ID_SHIFT					16
#define DEVICE_TYPE_MASK						0x0300U
#define DEVICE_TYPE_SHIFT						8
#define DEVICE_OEM_MASK							0x0FFU
#define PRV_MASK								0x0FF00U
#define PRV_SHIFT								8
#define MDT_MASK								0xFFU
#define DEVICE_TYPE0							"Removable"
#define DEVICE_TYPE1							"BGA (Discrete embedded)"
#define DEVICE_TYPE2							"POP"
#define DEVICE_TYPE3							"Reserved"

#endif /* TEGRABL_SDMMC_CARD_REG_H */
