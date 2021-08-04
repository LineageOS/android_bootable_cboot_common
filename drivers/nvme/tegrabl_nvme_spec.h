/*
 * Copyright (c) 2021, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NVME

#ifndef TEGRABL_NVME_SPEC_H
#define TEGRABL_NVME_SPEC_H

/*
 * Length of the serial number field.
 */
#define NVME_SERIAL_NUM_CHAR 20

/*
 * Length of the model number field.
 */
#define NVME_MODEL_NUM_CHAR 40
/*
 * Length of the firmware revision field.
 */
#define NVME_FR_REV_NUM_CHAR 			8
#define TIMEOUT_IN_MS 					10000
#define PAGE_SIZE 						0x1000
#define PAGE_SIZE_LOG2 					12
#define QUEUE_SIZE 						2
#define NVME_MAX_READ_WRITE_SECTORS 	256
#define MAX_TRANSFER					0x80000
#define TEGRABL_NVME_BUF_ALIGN_SIZE 	8U
#define NVME_CLASS_CODE 				0x010802

/*
 * Admin opcodes
 */
enum tegrabl_nvme_admin_opcode {
	NVME_DELETE_IO_SQ_OPCODE                = 0x00,
	NVME_CREATE_IO_SQ_OPCODE                = 0x01,
	NVME_DELETE_IO_CQ_OPCODE                = 0x04,
	NVME_CREATE_IO_CQ_OPCODE                = 0x05,
	NVME_IDENTIFY_OPCODE                    = 0x06,
	NVME_SET_FEATURES_OPCODE                = 0x09,
	NVME_GET_FEATURES_OPCODE                = 0x0a,
};

enum tegrabl_nvme_feature {
	NVME_FEAT_NUM_Q                         = 0x07
};

/*
 * NVM command set opcodes
 */
enum tegrabl_nvme_nvm_opcode {
	NVME_FLUSH_OPCODE                       = 0x00,
	NVME_WRITE_OPCODE                       = 0x01,
	NVME_READ_OPCODE                        = 0x02,
};

/*
 * Identify command CNS value
 */
enum tegrabl_nvme_identify_cns {
	NVME_IDENTIFY_NAMESPACE                 = 0x00,
	NVME_IDENTIFY_CONTROLLER                = 0x01,
	NVME_GET_ACTIVE_NS                      = 0x02,
	NVME_NS_IDENTIFICATION_DESC             = 0x03
};

/*
 * Status code types
 */
enum tegrabl_nvme_sts_code_type {
	NVME_SCTYPE_GENERIC                     = 0x0,
	NVME_SCTYPE_COMMAND_SPECIFIC            = 0x1,
	NVME_SCTYPE_MEDIA_ERROR                 = 0x2,
	NVME_SCTYPE_VENDOR_SPECIFIC             = 0x7,
};

/*
 * Generic command status codes
 */
enum tegrabl_nvme_generic_cmd_sts_code {
	NVME_STATUS_SUCCESS                     = 0x00,
	NVME_STATUS_INVALID_OPCODE              = 0x01,
	NVME_STATUS_INVALID_FIELD               = 0x02
};

/* Submission queue entry */
struct tegrabl_nvme_sq_cmd {
	uint16_t opc:8;     /* opcode */
	uint16_t fuse:2;    /* fused operation */
	uint16_t rsrv1:4;
	uint16_t psdt:2;
	uint16_t cid;       /* command identifier */
	uint32_t nsid;      /* namespace identifier */
	uint32_t rsrv2;
	uint32_t rsrv3;
	uint64_t mptr;      /* metadata pointer */
	struct {
		uint64_t prp1;  /* prp entry 1 */
		uint64_t prp2;  /* prp entry 2 */
	} dptr;             /* data pointer */
	uint32_t cdw10;
	uint32_t cdw11;
	uint32_t cdw12;
	uint32_t cdw13;
	uint32_t cdw14;
	uint32_t cdw15;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_sq_cmd) == 64, "Size is incorrect");

struct tegrabl_nvme_status {
	uint16_t p:1;     /* phase tag */
	uint16_t sc:8;    /* status code */
	uint16_t sct:3;   /* status code type */
	uint16_t rsvd2:2;
	uint16_t m:1;     /* more */
	uint16_t dnr:1;   /* do not retry */
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_status) == 2, "Size is incorrect");

/*
 * Completion queue entry
 */
struct tegrabl_nvme_cq_cmd {
	uint32_t cdw0;  /* command specific cdword */
	uint32_t rsrv1;
	uint16_t sqhd;  /* submission queue head pointer */
	uint16_t sqid;  /* submission queue identifier */
	uint16_t cid;   /* command identifier */
	struct tegrabl_nvme_status sf;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_cq_cmd) == 16, "Size is incorrect");

struct tegrabl_nvme_cap_register {
	uint32_t mqes:16;          /* maximum queue entries supported */
	uint32_t cqr:1;
	uint32_t ams:2;            /* arbitration mechanism supported */
	uint32_t rsrv1:5;
	/*
	 * Minimum time to wait for the controller to become ready
	 * (in 500 millisecond units)
	 */
	uint32_t to:8;
	uint32_t dstrd:4;          /* doorbell stride */
	uint32_t nssrs:1;
	uint32_t css_nvm:1;        /* command sets supported */
	uint32_t css_rsrv:6;
	uint32_t css_no_io:1;
	uint32_t bps:1;            /* boot partition supported */
	uint32_t rsrv2:2;
	uint32_t mpsmin:4;         /* memory page size minimum */
	uint32_t mpsmax:4;         /* memory page size maximum */
	uint32_t rsrv3:8;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_cap_register) == 8, "Size is incorrect");

struct tegrabl_nvme_cc_register {
	uint32_t en:1;              /* enable */
	uint32_t rsrv1:3;
	uint32_t css:3;             /* I/O command set selected */
	uint32_t mps:4;             /* memory page size */
	uint32_t ams:3;             /* arbitration mechanism selected */
	uint32_t shn:2;
	uint32_t iosqes:4;          /* I/O submission queue entry size */
	uint32_t iocqes:4;          /* I/O completion queue entry size */
	uint32_t rsrv2:8;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_cc_register) == 4, "Size is incorrect");

struct tegrabl_nvme_vs_register {
	uint32_t ter:8;
	uint32_t mnr:8;
	uint32_t mjr:16;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_vs_register) == 4, "Size is incorrect");

struct tegrabl_nvme_csts_register {
	uint32_t rdy:1;
	uint32_t cfs:1;
	uint32_t shst:2;
	uint32_t rsrv1:28;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_csts_register) == 4, "Size is incorrect");

struct tegrabl_nvme_aqa_register {
	uint32_t asqs:12;           /* admin submission queue size */
	uint32_t rsrv1:4;
	uint32_t acqs:12;           /* admin completion queue size */
	uint32_t rsrv2:4;
};

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_aqa_register) == 4, "Size is incorrect");

struct tegrabl_nvme_registers {
	struct tegrabl_nvme_cap_register cap;       /* controller capabilities */
	struct tegrabl_nvme_vs_register vs;         /* NVMe specification version */
	uint32_t  intms;
	uint32_t  intmc;
	struct tegrabl_nvme_cc_register cc;         /* controller configurations */
	uint32_t  rsrv1;
	struct tegrabl_nvme_csts_register csts;     /* controller status */
	uint32_t  nssr;
	struct tegrabl_nvme_aqa_register aqa;       /* admin queue attributes */
	uint64_t asq;                               /* admin submission queue base address */
	uint64_t acq;                               /* admin completion queue base address */
	uint32_t cmbloc;
	uint32_t cmbsz;
	uint32_t  rsrv3[0x3f0];
	struct {
		uint32_t sq_tdbl;                       /* submission queue tail doorbell */
		uint32_t cq_hdbl;                       /* completion queue head doorbell */
	} doorbell[1];
};

/*
 * NVMe controller register space offsets.
 */
TEGRABL_COMPILE_ASSERT(0x00 == offsetof(struct tegrabl_nvme_registers, cap),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x08 == offsetof(struct tegrabl_nvme_registers, vs),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x0C == offsetof(struct tegrabl_nvme_registers, intms),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x10 == offsetof(struct tegrabl_nvme_registers, intmc),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x14 == offsetof(struct tegrabl_nvme_registers, cc),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x1C == offsetof(struct tegrabl_nvme_registers, csts),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x20 == offsetof(struct tegrabl_nvme_registers, nssr),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x24 == offsetof(struct tegrabl_nvme_registers, aqa),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x28 == offsetof(struct tegrabl_nvme_registers, asq),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x30 == offsetof(struct tegrabl_nvme_registers, acq),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x38 == offsetof(struct tegrabl_nvme_registers, cmbloc),
		   "Incorrect register offset");
TEGRABL_COMPILE_ASSERT(0x3C == offsetof(struct tegrabl_nvme_registers, cmbsz),
		   "Incorrect register offset");

TEGRABL_PACKED(
struct tegrabl_nvme_ctrlr_data {
	/* controller capabilities and features (bytes 0-255) */
	uint16_t vid;                       /* pci vendor id. */
	uint16_t ssvid;                     /* pci subsystem vendor id. */
	char sn[NVME_SERIAL_NUM_CHAR];      /* serial number. */
	char mn[NVME_MODEL_NUM_CHAR];       /* model number. */
	char fr[NVME_FR_REV_NUM_CHAR];      /* firmware revision. */
	uint8_t rab;
	uint8_t ieee[3];                    /* IEEE oui identifier. */
	uint8_t cmic;
	uint8_t mdts;                       /* maximum data transfer size. (bytes 77) */
	uint8_t unused1[447];
	struct {
		uint8_t present:1;
		uint8_t flush_behavior:2;
		uint8_t reserved:5;
	} vwc;                              /* volatite write cache (bytes 525) */
	uint8_t unused2[14];
	uint32_t mnan;
	uint8_t unused3[3552];
});

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_ctrlr_data) == 4096, "Incorrect size");
TEGRABL_COMPILE_ASSERT(525 == offsetof(struct tegrabl_nvme_ctrlr_data, vwc),
		   "Incorrect offset");
TEGRABL_COMPILE_ASSERT(540 == offsetof(struct tegrabl_nvme_ctrlr_data, mnan),
		   "Incorrect offset");

TEGRABL_PACKED(
struct tegrabl_nvme_ns_data {
	uint64_t nsze;                          /* namespace size (number of sectors). */
	uint64_t ncap;                          /* namespace capacity. */
	uint64_t nuse;
	uint8_t nsfeat;
	uint8_t nlbaf;                          /* numbers of lba formats. */
	struct {
		uint8_t format:4;
		uint8_t extended:1;
		uint8_t reserved2:3;
	} flbas;                                /* formatted lba size. */
	uint8_t mc;
	uint8_t dpc;
	uint8_t dps;
	uint8_t nmic;
	uint8_t nsrescap;
	uint8_t fpi;
	uint8_t reserved1;
	uint16_t atomic_info[6];
	uint16_t reserved2;
	uint64_t nvmcap[2];
	uint8_t reserved4[40];
	uint8_t nguid[16];
	uint64_t eui64;
	struct {
		uint32_t ms:16;                     /* metadata size */
		uint32_t lbads:8;                   /* LBA data size. */
		uint32_t rp:2;                      /* relative performance. */
		uint32_t reserved:6;
	} lbaf[16];                             /* LBA format supported. */
	uint8_t reserved5[192];
	uint8_t vendor_specific[3712];
});

TEGRABL_COMPILE_ASSERT(sizeof(struct tegrabl_nvme_ns_data) == 4096, "Incorrect size");
TEGRABL_COMPILE_ASSERT(128 == offsetof(struct tegrabl_nvme_ns_data, lbaf),
		   "Incorrect offset");
TEGRABL_COMPILE_ASSERT(26 == offsetof(struct tegrabl_nvme_ns_data, flbas),
		   "Incorrect offset");
#endif
