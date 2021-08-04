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

#ifndef TEGRABL_NVME_PRIV_H
#define TEGRABL_NVME_PRIV_H

#include <tegrabl_nvme_spec.h>

#define DEFAULT_NSID 1
#define DEFAULT_IO_Q 1

enum queue_type {
	NVME_SQ_TYPE,
	NVME_CQ_TYPE
};

struct tegrabl_nvme_sq {
	/* submission queue */
	/* queue size is 2 by default to be simple */
	uint64_t head;
	uint64_t tail;
	size_t size;
	volatile uint32_t *doorbell;
	struct tegrabl_nvme_sq_cmd *entries;
};

struct tegrabl_nvme_cq {
	/* completetion queue */
	/* queue size is 2 by default to be simple */
	uint64_t head;
	uint64_t tail;
	size_t size;
	uint8_t phase;
	volatile uint32_t *doorbell;
	struct tegrabl_nvme_cq_cmd *entries;
};

struct tegrabl_nvme_queue_pair {
	uint16_t id;
	struct tegrabl_nvme_sq sq;
	struct tegrabl_nvme_cq cq;
};

struct tegrabl_prplist {
	size_t max_size;
	size_t max_entries;
	void *prp1;
	uint64_t *prp_list;
};

struct tegrabl_nvme_ctrl {
	struct tegrabl_nvme_ctrlr_data cdata;
	struct tegrabl_nvme_ns_data nsdata;
	struct tegrabl_nvme_registers volatile *rgst;
	uint16_t current_cid;
	uint32_t chosen_nsid;
	struct tegrabl_prplist prp_list;
	struct tegrabl_nvme_queue_pair admin_q;
	struct tegrabl_nvme_queue_pair io_q;
};

struct tegrabl_nvme_context {
	size_t block_size_log2;
	bnum_t block_count;
	size_t max_transfer_blk;
	uint8_t page_size_log2;
	size_t page_size;
	uint8_t instance;
	struct tegrabl_nvme_ctrl ctrl;
};

/**
 * @brief initializes the nvme controller and context
 *
 * @param context nvme context
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_nvme_init(struct tegrabl_nvme_context *context);

/**
 * @brief performs nvme read/write blocks
 *
 * @param context nvme context
 * @param buffer buffer address to be read to or write from
 * @param blknr block number to read/write
 * @param blkcnt number of blocks to read/write
 * @param write flag: TRUE is to write, FALSE is to read
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
tegrabl_error_t tegrabl_nvme_rw_blocks(struct tegrabl_nvme_context *context, void *buffer,
									bnum_t blknr, bnum_t blkcnt, bool write);

/**
 * @brief frees all buffers used by nvme controller
 *
 * @param context nvme context
 * @return void
 */
void tegrabl_nvme_free_buffers(struct tegrabl_nvme_context *context);

/**
 * @brief resets the pcie controller
 *
 * @param context nvme context
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
tegrabl_error_t tegrabl_nvme_reset_pcie(struct tegrabl_nvme_context *context);

#endif
