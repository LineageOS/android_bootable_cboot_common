/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef TEGRABL_UFS_HCI_H
#define TEGRABL_UFS_HCI_H
#include <tegrabl_ufs_rpmb.h>

#define DATA_DIR_NIL 0x0U
#define DATA_DIR_H2D 0x1U
#define DATA_DIR_D2H 0x2U
#define OCS_SUCCESS 0x0U
#define OCS_INVALID 0xFU

/** Create structure for Transfer Request descriptor
 */
struct task_mgmt_upiu {
	uint32_t todo1;
};

struct task_mgmt_request_descriptor {
	uint32_t DW0;
	uint32_t DW1;
	uint32_t DW2;
	uint32_t DW3;
	struct task_mgmt_upiu request;
	struct task_mgmt_upiu response;
	/** 80 bytes till here **/
};

struct transfer_request_descriptor {
	/* DW0 */
	union {
		struct {
			uint32_t rsvd0:24;
			uint32_t interrupt:1;
			uint32_t dd:2;
			uint32_t rsvd1:1;
			uint32_t control_type:4;
		};
		uint32_t dw;
	} dw0;
	/* DW1 */
	union {
		uint32_t reserved;
		uint32_t dw;
	} dw1;
	/* DW2 */
	union {
		struct {
			uint32_t ocs:8;
			uint32_t rsvd0:24;
		};
		uint32_t dw;
	} dw2;
	/* DW3 */
	union {
		uint32_t reserved;
		uint32_t dw;
	} dw3;
	/* DW4 */
	union {
	struct {
		uint32_t rsvd0:7;
		uint32_t ctba:25;
	};
	uint32_t dw;
	} dw4;
	/* DW5 */
	union {
		uint32_t ctbau;
		uint32_t dw;
	} dw5;
	/* DW6 */
	union {
		struct {
			uint32_t rul:16;
			uint32_t ruo:16;
		};
		uint32_t dw;
	} dw6;
	/* DW7 */
	union {
		struct {
			uint32_t prdtl:16;
			uint32_t prdto:16;
		};
		uint32_t dw;
	} dw7;
};


/** Transfer Request Descriptor defines for use with NV DRF macros
 */
/** DW0 **/
#define UFS_TRD_DW0_0_CT_RANGE ((31) : (28))
#define UFS_TRD_DW0_0_DD_RANGE ((26) : (25))
#define UFS_TRD_DW0_0_I_RANGE ((24) : (24))
#define UFS_TRD_DW0_0_RESERVED_RANGE ((23) : (0))

/** DW1 **/
#define UFS_TRD_DW1_0_RESERVED_RANGE ((31) : (0))

/** DW2 **/
#define UFS_TRD_DW2_0_RESERVED_RANGE ((31) : (8))
#define UFS_TRD_DW2_0_OCS_RANGE ((7) : (0))

/** DW3 **/
#define UFS_TRD_DW3_0_RESERVED_RANGE ((31) : (0))

/** DW4 **/
#define UFS_TRD_DW4_0_UCDBA_RANGE ((31) : (7))
#define UFS_TRD_DW4_0_RESERVED_RANGE ((6) : (0))

/** DW5 **/
#define UFS_TRD_DW5_0_UCDBAU_RANGE ((31) : (0))

/** DW6 **/
#define UFS_TRD_DW6_0_RUO_RANGE ((31) : (16))
#define UFS_TRD_DW6_0_RUL_RANGE ((15) : (00))

/** DW7 **/
#define UFS_TRD_DW7_0_PRDTO_RANGE ((31) : (16))
#define UFS_TRD_DW7_0_PRDTL_RANGE ((15) : (0))

/****************************************************/
#define UFS_TRD_DW0_0_CT_UFS 1U

#define UFS_TRD_DW0_0_DD_WRITE 1U
#define UFS_TRD_DW0_0_DD_READ 2U


/** END Transfer Request Descriptor defines */
/** Structure to hold DME commands. This is not directly written into
 * UICCMD registers
 */
struct dme_cmd {
	// This is C11 but compiles without warning with std=99
	union {
		struct {
			uint32_t cmdop:8;
			uint32_t rsvd0_dw0:24;
		};
		uint32_t dw;
	} uic_cmd;
	union {
		struct {
			/* Begin Arg 1. Reset level is a special case*/
			uint32_t gen_selector_index:16;
			uint32_t mib_attribute:16;
		};
		uint32_t dw;
	} uic_cmd_arg1;
	union {
		struct {
			/* Begin Arg 2 */
			uint32_t config_error_code:8;
			uint32_t rsvd0_dw2:8;
			uint32_t attr_set_type:8;
			uint32_t rsvd1_dw2:8;
		};
		uint32_t dw;
		} uic_cmd_arg2;
	/* Begin Arg 3 */
	uint32_t read_write_value;
} ;
/** End structure to hold DME commands*/

/** UIC CMD Opcodes */
#define DME_GET 0x1U
#define DME_SET 0x2U
#define DME_PEER_GET 0x3U
#define DME_PEER_SET 0x4U
#define DME_ENABLE 0x12U
#define DME_RESET 0x14U
#define DME_LINKSTARTUP 0x16U
#define DME_HIBERNATE_ENTER 0x17U
#define DME_HIBERNATE_EXIT 0x18U
/** End UIC CMD Opcodes */

/** UIC MIB Attributes */
#define pa_avail_tx_data_lanes		0x1520U
#define pa_avail_rx_data_lanes		0x1540U
#define pa_active_tx_data_lanes		0x1560U
#define pa_connected_tx_data_lanes	0x1561U
#define pa_tx_gear			0x1568U
#define pa_tx_termination		0x1569U
#define pa_hs_series			0x156AU
#define pa_active_rx_data_lanes		0x1580U
#define pa_connected_rx_data_lanes	0x1581U
#define pa_rx_gear			0x1583U
#define pa_rx_termination		0x1584U
#define pa_tx_hs_g1_prepare_length	0x1553U
#define pa_tx_hs_g2_prepare_length	0x1555U
#define pa_tx_hs_g3_prepare_length	0x1557U

#define pa_maxrxhsgear			0x1587U

#define pa_tx_hs_g1_sync_length		0x1552U
#define pa_tx_hs_g2_sync_length		0x1554U
#define pa_tx_hs_g3_sync_length		0x1556U

#define pa_local_tx_lcc_enable		0x155eU
#define pa_peer_tx_lcc_enable		0x155fU
#define pa_tx_trailing_clocks		0x1564U
#define pa_pwr_mode			0x1571U
#define pa_sleep_no_config_time		0x15a2U
#define pa_stall_no_config_time		0x15a3U
#define pa_save_config_time		0x15a4U

#define pa_hibern8time			0x15a7U
#define pa_tactivate			0x15a8U
#define pa_granularity			0x15aaU

#define pwr_mode_user_data0		0x15B0U
#define pwr_mode_user_data1		0x15B1U
#define pwr_mode_user_data2		0x15B2U

#define t_cportflags			0x4025U
#define t_connectionstate		0x4020U

#define dme_layerenable			0xD000U
#define dme_linkstartup			0xD020U
#define vs_txburstclosuredelay		0xD084U

#define dme_fc0protectiontimeoutval	0xD041U
#define dme_tc0replaytimeoutval		0xD042U
#define dme_afc0reqtimeoutval		0xD043U

#define vs_debugsaveconfigtime		0xD0A0U
#define vs_debugsaveconfigtime_tref	0x6U
#define vs_debugsaveconfigtime_st_sct	0x3U

#define set_tref(x) (((x) & 0x7) << 2)
#define set_st_sct(x) (((x) & 0x3) << 0)

/** Unipro powerchange mode.
 * SLOW : PWM
 * SLOW_AUTO : PWM (but does auto burst closure for power saving)
 */
#define PWRMODE_SLOW_MODE		0x2U
#define PWRMODE_FAST_MODE		0x1U
#define PWRMODE_FASTAUTO_MODE		0x4U
#define PWRMODE_SLOWAUTO_MODE		0x5U

/** Define ranges for UPIU Fields */
#define UFS_UPIU_FLAGS_O_SHIFT		6U
#define UFS_UPIU_FLAGS_U_SHIFT		5U
#define UFS_UPIU_FLAGS_D_SHIFT		4U
#define UFS_UPIU_FLAGS_R_SHIFT		6U
#define UFS_UPIU_FLAGS_W_SHIFT		5U
#define UFS_UPIU_FLAGS_A_SHIFT		0U
#define UFS_UPIU_FLAGS_A_MASK		0x3U

/** Define transaction codes etc for different UPIUs */
#define UPIU_NOP_OUT_TRANSACTION	0x0U
#define UPIU_NOP_IN_TRANSACTION		0x20U
#define UPIU_COMMAND_TRANSACTION	0x1U
#define UPIU_RESPONSE_TRANSACTION	0x21U
#define UPIU_QUERY_REQUEST_TRANSACTION	0x16U
#define UPIU_QUERY_RESPONSE_TRANSACTION	0x36U

/* TODO fill in other transactions */

/** Define codes etc for different types of Query Requests*/
#define UPIU_QUERY_FUNC_STD_READ	0x1U
#define UPIU_QUERY_FUNC_STD_WRITE	0x81U
/** Define codes etc for different types of Command Requests*/
#define UPIU_COMMAND_READ		0x1U
#define UPIU_COMMAND_WRITE		0x81U

/** Define codes etc for different families of Command*/
#define UPIU_COMMAND_SET_SCSI		0x0U
#define UPIU_COMMAND_SET_UFS		0x1U

/** All CDB defintions from SCSI block and primary commands*/
#define SCSI_READ6_OPCODE		0x08U
#define SCSI_FORMAT_UNIT_OPCODE		0x04U
#define SCSI_INQUIRY_OPCODE		0x12U
#define SCSI_MODE_SELECT10_OPCODE	0x55U
#define SCSI_MODE_SENSE10_OPCODE	0x5AU
#define SCSI_ORE_FETCH10_OPCODE		0x34U
#define SCSI_READ10_OPCODE		0x28U
#define SCSI_READ_CAPACITY10_OPCODE	0x25U
#define SCSI_REQUEST_SENSE_OPCODE	0x03U
#define SCSI_UNMAP_OPCODE		0x42U
#define SCSI_VERIFY10_OPCODE		0x2FU
#define SCSI_WRITE6_OPCODE		0x0AU
#define SCSI_WRITE10_OPCODE		0x2AU
#define SCSI_WRITE_BUFFER_OPCODE	0x3BU

#define SCSI_STATUS_GOOD		0x0U
#define SCSI_STATUS_CHECK_CONDITION	0x2U
#define SCSI_STATUS_BUSY		0x8U

/* RPMB */
#define SCSI_SECURITY_PROTOCOL_UFS	0xECU
#define SCSI_SECURITY_PROTOCOL_OUT_OPCODE	0xB5U
#define SCSI_SECURITY_PROTOCOL_IN_OPCODE	0xA2U
#define SCSI_SECURITY_PROTOCOL_CMD_LEN	12U

/** Define codes etc for different types of Query READ/WRITE Requests*/
#define TSF_OPCODE_READ_DESC		0x1U
#define TSF_OPCODE_WRITE_DESC		0x2U
#define TSF_OPCODE_READ_ATTRB		0x3U
#define TSF_OPCODE_WRITE_ATTRB		0x4U
#define TSF_OPCODE_READ_FLAG		0x5U
#define TSF_OPCODE_SET_FLAG			0x6U

/* TODO fill in other query reqs */
/** Define codes etc for different Attribute IDNs*/
#define QUERY_ATTRB_BOOT_LUN_EN		0x0U
#define QUERY_ATTRB_CURR_POWER_MODE	0x2U
#define QUERY_ATTRB_ACTIVE_ICC_LVL	0x3U
#define QUERY_ATTRB_REF_CLK_FREQ	0xAU

/** Define codes etc for different Flag IDNs*/
#define QUERY_FLAG_DEVICE_INIT_IDN	0x1U

/** Define codes etc for different Descriptor IDNs*/
#define QUERY_DESC_DEVICE_DESC_IDN	0x0U
#define QUERY_DESC_CONF_DESC_IDN	0x1U
#define QUERY_DESC_UNIT_DESC_IDN	0x2U
#define QUERY_DESC_GEO_DESC_IDN		0x7U

/* TODO fill in other descriptor ids*/

/** Define offsets etc for different Descriptors*/
#define UFS_DESC_MAX_SIZE		255U
/** DEVICE DESCRIPTOR */
#define UFS_DEV_DESC_LENGTH			0x0U
#define UFS_DEV_DESC_DESC_TYPE			0x1U
#define UFS_DEV_DESC_DEVICE			0x2U
#define UFS_DEV_DESC_DEVICE_CLASS		0x3U
#define UFS_DEV_DESC_DEVICE_SUB_CLASS		0x4U
#define UFS_DEV_DESC_PROTOCOL			0x5U
#define UFS_DEV_DESC_NUM_LUN			0x6U
#define UFS_DEV_DESC_NUM_WLUN			0x7U
#define UFS_DEV_DESC_BOOT_ENABLE		0x8U
#define UFS_DEV_DESC_DESC_ACCESS_ENABLE		0x9U
#define UFS_DEV_DESC_INIT_POWER_MODE		0xAU
#define UFS_DEV_DESC_HIGH_PRIORITY_LUN		0xBU
#define UFS_DEV_DESC_SECURE_REMOVAL_TYPE	0xCU
#define UFS_DEV_DESC_SECURITY_LU		0xDU
#define UFS_DEV_DESC_RESERVED			0xEU
#define UFS_DEV_DESC_UD0_BASE_OFFSET		0x1AU
#define UFS_DEV_DESC_UD_CONFIG_PLENGTH		0x1BU

/** UNIT DESCRIPTOR */
#define UFS_UNIT_DESC_LENGTH			0x0U
#define UFS_UNIT_DESC_DESC_TYPE			0x1U
#define UFS_UNIT_DESC_UNIT_INDEX		0x2U
#define UFS_UNIT_DESC_LU_ENABLE			0x3U
#define UFS_UNIT_DESC_BOOT_LUN_ID		0x4U
#define UFS_UNIT_DESC_LU_WRITE_PROTECT		0x5U
#define UFS_UNIT_DESC_LU_QUEUE_DEPTH		0x6U
#define UFS_UNIT_DESC_RESERVED			0x7U
#define UFS_UNIT_DESC_MEMORY_TYPE		0x8U
#define UFS_UNIT_DESC_DATA_RELIABILITY		0x9U
#define UFS_UNIT_DESC_LOGICAL_BLOCK_SIZE	0xAU
#define UFS_UNIT_DESC_QLOGICAL_BLOCK_COUNT	0xBU
#define UFS_UNIT_DESC_ERASE_BLOCK_SIZE		0x13U
#define UFS_UNIT_DESC_PROVISIONING_TYPE		0x17U
#define UFS_UNIT_DESC_QPHY_MEM_RSRC_COUNT	0x18U

/* Well known logical unit id in LUN field of UPIU */
#define UFS_UPIU_LUN_0				0x0U
#define UFS_UPIU_LUN_1				0x1U
#define UFS_UPIU_LUN_2				0x2U
#define UFS_UPIU_LUN_3				0x3U
#define UFS_UPIU_LUN_4				0x4U
#define UFS_UPIU_LUN_5				0x5U
#define UFS_UPIU_LUN_6				0x6U
#define UFS_UPIU_LUN_7				0x7U
#define UFS_UPIU_REPORT_LUNS_WLUN		0x81U
#define UFS_UPIU_UFS_DEVICE_WLUN		0xD0U
#define UFS_UPIU_BOOT_WLUN			0xB0U
#define UFS_UPIU_RPMB_WLUN			0xC4U


TEGRABL_PACKED(
struct ufs_device_descriptor {
	uint8_t length;
	uint8_t descriptor_type;
	uint8_t device;
	uint8_t device_class;
	uint8_t device_subclass;
	uint8_t protocol;
	uint8_t number_lu;
	uint8_t number_wlu;
	uint8_t boot_enable;
	uint8_t desc_access_en;
	uint8_t init_power_mode;
	uint8_t high_priority_lun;
	uint8_t secure_removal_type;
	uint8_t security_lu;
	uint8_t reserved;
	uint8_t init_active_icc_level;
	uint16_t spec_version;
	uint16_t manufacture_date;
	uint8_t manufacturer_name;
	uint8_t product_name;
	uint8_t serial_number;
	uint8_t oem_id;
	uint16_t manufacturer_id;
	uint8_t ud0_base_offset;
	uint8_t udc_configp_length;
	uint8_t device_rtt_cap;
	uint16_t periodic_rtc_update;
});


TEGRABL_PACKED(
struct ufs_unit_descriptor {
	uint8_t length;
	uint8_t desc_type;
	uint8_t unit_index;
	uint8_t lu_enable;
	uint8_t boot_lun_id;
	uint8_t lu_write_protect;
	uint8_t lu_queue_depth;
	uint8_t reserved;
	uint8_t memory_type;
	uint8_t data_reliability;
	uint8_t logical_blocksize;
	uint64_t logical_blockcount;
	uint32_t erase_blocksize;
	uint8_t provision_type;
	uint64_t phy_memresource_count;
	uint16_t context_capabilities;
	uint8_t large_unit_sizem1;
});


/*****************************************************************************/
/** UPIU Basic Header **/

TEGRABL_PACKED(
struct upiu_basic_header {
	uint8_t trans_code;
	uint8_t flags;
	uint8_t lun;
	uint8_t task_tag;
	uint8_t cmd_set_type;
	uint8_t query_tm_function;
	uint8_t response;
	uint8_t status;
	uint8_t ehs_length;
	uint8_t device_info;
	uint16_t data_seg_len_bige;
	/** 12 bytes till here**/
});

/** Command UPIU
 * Used for SCSI Commands.
 * 32 bytes
 */

TEGRABL_PACKED(
struct command_upiu {
	struct upiu_basic_header basic_header;
	/** 12 bytes till here*/
	uint32_t expected_data_tx_len_bige;
	uint8_t cdb[16];
});

/** Response UPIU
 */

TEGRABL_PACKED(
struct response_upiu {
	struct upiu_basic_header basic_header;
	uint32_t residual_tx_count_bige;
	uint8_t reserved[16];
	uint16_t sense_data_length;
	uint8_t sense_data[18];
});

/** NOP_OUT UPIU
 */

TEGRABL_PACKED(
struct nop_upiu {
	struct upiu_basic_header basic_header;
	uint8_t reserved[20];
});


/** Query Request/Response UPIU
 * Used primarily for transferring descriptors between host and device.
 * Max descriptor size is 255 bytes.
 * Keeping data segment size as 256 (255 data + 1 byte padding) so that
 * Response UPIU in TRD will be aligned to 64 bit boundary.
*/

TEGRABL_PACKED(
struct flag_fields
{
	uint8_t flag_idn;
	uint8_t index;
	uint8_t selector;
	uint8_t reserved1[7];
	uint8_t flag_value;
	uint32_t reserved2;
});


TEGRABL_PACKED(
struct attrb_fields
{
	uint8_t attrb_idn;
	uint8_t index;
	uint8_t selector;
	uint8_t reserved1[4];
	uint32_t value_bige;
	uint32_t reserved2;
});


TEGRABL_PACKED(
struct desc_fields
{
	uint8_t desc_idn;
	uint8_t index;
	uint8_t selector;
	uint8_t reserved1[2];
	uint16_t length_bige;
	uint32_t reserved2[2];
});

TEGRABL_PACKED(
struct tsf
{
	uint8_t op_code;
	union
	{
		struct desc_fields vdesc_fields;
		struct attrb_fields vattrb_fields;
		struct flag_fields vflag_fields;
	};
});


TEGRABL_PACKED(
struct query_req_resp_upiu
{
	struct upiu_basic_header basic_header;
	struct tsf  vtsf;
	uint32_t reserved;
	/** 32 bytes till here **/
	/** choosing data segment size of 256 as descriptor is maximum 255 bytes
	and that is probably the largest chunk of data we should receiving**/
	uint8_t data_segment[256];
	/** 288 bytes till here **/
});


TEGRABL_PACKED(
struct task_mgmt_descriptor_upiu
{
	struct upiu_basic_header basic_header;
	uint32_t input_param1;
	uint32_t input_param2;
	uint32_t input_param3;
	uint32_t reserved1;
	uint32_t reserved2;
	/** 32 bytes till here **/
});

/** Define PRDT */

TEGRABL_PACKED(
struct prdt
{
	uint32_t dw0;
	uint32_t dw1;
	uint32_t dw2;
	uint32_t dw3;
});
/* PRDT_DW0 */
#define PRDT_DW0_0_RESERVED_RANGE   1:0
#define PRDT_DW0_0_DBA_RANGE        31:2
/* PRDT_DW3 */
#define PRDT_DW3_0_DBA_RANGE        17:2
#define PRDT_DW3_0_RESERVED_RANGE   31:18
/** End PRDT defines and structures**/

/* Define union of Command UPIU, QUERY UPIU and NOP UPIU*/

union ucd_generic_req_upiu
{
	struct nop_upiu vnop_upiu;
	struct command_upiu vcmd_upiu;
	struct query_req_resp_upiu vquery_req_resp_upiu;
	/** 288 bytes **/
};

/* Define generic response */
union ucd_generic_resp_upiu
{
	struct nop_upiu vnop_upiu;
	struct response_upiu vresp_upiu;
	struct query_req_resp_upiu vquery_req_resp_upiu;
	/** 288 bytes **/
};

/** UFS Command Descriptor
 *  Structure is as follows:
 *     --------------------
 *    |  UCD Generic UPIU  | NOP/Query/SCI Cmd UPIU, Max 288 bytes (QueryUPIU)
 *     --------------------
 *    |  Response UPIU     | NOP/Query/SCI RSP Aligned to 64 bit boundary,
 *     --------------------  288 bytes
 *    |  PRDT (if required)| Needed for SCSI Cmd UPIUs
 *     --------------------
 * Structure itself should be aligned to 128 byte aligned
 * so pad accordingly to form array.
 * TODO implement MACRO if possible to complain if alignment not met
 */
#define CMD_DESC_REQ_LENGTH 512U
#define CMD_DESC_RESP_LENGTH 512U
#define CMD_DESC_PRDT_LENGTH 512U
#define MAX_PRDT_LENGTH 16U

TEGRABL_PACKED(
struct cmd_descriptor
{
	union
	{
		union ucd_generic_req_upiu vucd_generic_req_upiu; /* 288 bytes */
		uint8_t    cmd_desc_req[CMD_DESC_REQ_LENGTH];
	};
	/** Response UPIU has to be aligned to 64 bit boundary **/
	union
	{
		union ucd_generic_resp_upiu vucd_generic_resp_upiu; /* 288 bytes */
		uint8_t    cmd_desc_resp[CMD_DESC_RESP_LENGTH];
	};
	union
	{
		struct prdt vprdt[MAX_PRDT_LENGTH];
		uint8_t  cmd_desc_prdt[CMD_DESC_PRDT_LENGTH];
	};
}) ;
/** Data OUT UPIU and DATA IN UPIU handled by Host Controller.
 */

uint32_t tegrabl_ufs_dme_link_setup(void);
void tegrabl_ufs_setup_trtdm_lists(void);
uint32_t tegrabl_ufs_start_tmtr_engines(void);
uint32_t tegrabl_ufs_chk_if_dev_ready_to_rec_desc(void);
uint32_t tegrabl_ufs_get_devinfo(void);
uint32_t tegrabl_ufs_get_devInfo_partial_init(void);
void tegrabl_ufs_free_trd_cmd_desc(void);
uint32_t tegrabl_ufs_get_trd_slot(void);
uint32_t tegrabl_ufs_complete_init(void);
uint32_t tegrabl_ufs_set_dme_command(uint8_t cmd_op,
	uint16_t gen_sel_idx, uint16_t mib_attr, uint32_t* data);
tegrabl_error_t tegrabl_ufs_clock_enable(void);
uint32_t tegrabl_ufs_reset_disable(void);
uint32_t tegrabl_ufs_create_trd(uint32_t trd_index,
	uint32_t cmd_desc_index, uint32_t data_dir, uint16_t prdt_length);
tegrabl_error_t tegrabl_ufs_test_unit_ready(uint8_t lun);
tegrabl_error_t tegrabl_ufs_request_sense(uint8_t lun);
tegrabl_error_t tegrabl_ufs_set_timer_threshold(uint32_t Gear,
	uint32_t ActiveLanes);
uint32_t tegrabl_ufs_link_uphy_pll_setup(uint32_t Pll);
uint32_t tegrabl_ufs_link_uphy_lane_setup(uint32_t Lane);
uint32_t tegrabl_ufs_link_uphy_pll_params_setup(uint32_t Pll);
void tegrabl_ufs_uphy_lane_iddq_clamp_release(uint32_t Lane);
void tegrabl_ufs_uphy_lane_iddq_clamp_assert(uint32_t Lane);
tegrabl_error_t tegrabl_ufs_set_activate_time(void);
tegrabl_error_t tegrabl_ufs_read_capacity(uint32_t *size);
tegrabl_error_t tegrabl_ufs_get_provisioning_type(uint32_t lun,
	uint8_t *provision_type);
tegrabl_error_t tegrabl_ufs_erase(struct tegrabl_bdev *dev, uint8_t lun_id, uint32_t start_block,
					uint32_t blocks);
uint8_t tegrabl_ufs_is_rpmb_lun_supported(void);
tegrabl_error_t tegrabl_ufs_get_lun_capacity(uint8_t lun_id, uint32_t *block_size_log2,
						uint32_t *block_count);
tegrabl_error_t ufs_rpmb_io(uint8_t is_write,
		ufs_rpmb_context_t *rpmb_context, struct tegrabl_ufs_context *context);

#endif
