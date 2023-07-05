/*
 * Copyright (c) 2021-2023, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#define MODULE TEGRABL_ERR_NVME

#include "build_config.h"
#include <string.h>
#include <inttypes.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_io.h>
#include <tegrabl_cache.h>
#include <tegrabl_nvme_priv.h>
#include <tegrabl_nvme_err.h>
#include <tegrabl_pcie.h>
#include <tegrabl_smmu_ext.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_pcie_soc_local.h>

static void tegrabl_nvme_print_serial(char *array, size_t size, const char *prefix)
{
	char output[100];
	strncpy(output, array, MIN(100 - 1, size));
	output[MIN(100 - 1, size)] = '\0';
	pr_info("%s %s\n", prefix, output);
}

static tegrabl_error_t nvme_smmu_protect(struct tegrabl_nvme_context *context,
										 void *buffer,
										 uint32_t *size,
										 int prot)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	unsigned long smmu_buffer = (unsigned long)buffer;

	if (context->smmu_en) {
		pr_debug("SMMU: Enable SMMU protection @(%p, 0x%x)\n", buffer, *size);
		/* smmu_buffer has to be page aligned */
		if (smmu_buffer & (context->page_size - 1)) {
			smmu_buffer -= (smmu_buffer & (context->page_size - 1));
			pr_debug("new buffer = 0x%lx\n", smmu_buffer);
			*size += ((unsigned long)buffer - smmu_buffer);
		}
		/* size has to be page aligned */
		if (*size & (context->page_size - 1)) {
			*size = ALIGN(*size, context->page_size);
			pr_debug("new size = 0x%x\n", *size);
		}

		err = tegrabl_smmu_enable_prot(
							context->pcie_dev->smmu_cookie,
							smmu_buffer,
							smmu_buffer,
							(size_t)*size,
							prot);
		if (err != TEGRABL_NO_ERROR) {
			pr_warn("SMMU: Failed protection @(%p, 0x%x)\n", buffer, *size);
			*size = 0;
		}
	} else {
		*size = 0;
	}
	pr_debug("returning size=0x%x\n", *size);
	return err;
}

static uint32_t nvme_smmu_unprotect(struct tegrabl_nvme_context *context,
									void *buffer,
									uint32_t size)
{
	unsigned long smmu_buffer = (unsigned long)buffer;
	size_t rsize = 0;

	if (context->smmu_en && size) {
		pr_debug("SMMU: Unprotect @(%p, 0x%x)\n", buffer, size);
		/* smmu_buffer has to be page aligned */
		if (smmu_buffer & (context->page_size - 1)) {
			smmu_buffer -= (smmu_buffer & (context->page_size - 1));
			pr_debug("new buffer = 0x%lx\n", smmu_buffer);
			size += ((unsigned long)buffer - smmu_buffer);
		}
		/* size has to be page aligned */
		if (size & (context->page_size - 1)) {
			size = ALIGN(size, context->page_size);
			pr_debug("new size = 0x%x\n", size);
		}

		rsize = tegrabl_smmu_disable_prot(
							context->pcie_dev->smmu_cookie,
							smmu_buffer,
							(size_t)size);
		if (rsize != (size_t)size) {
			pr_warn("SMMU: Failed unprotection @(%p, 0x%x); rsize=0x%lx\n", buffer, size, rsize);
		}
	}

	return (uint32_t)rsize;
}

static void nvme_free_buffer(struct tegrabl_nvme_context *context,
							 void *buffer,
							 uint32_t size)
{
	pr_debug("SMMU: free @(%p, 0x%x)\n", buffer, size);
	if (context->smmu_en && size) {
		nvme_smmu_unprotect(context, buffer, size);
	}

	tegrabl_free(buffer);
}

static tegrabl_error_t tegrabl_construct_nvme_qpair(struct tegrabl_nvme_context *context,
													struct tegrabl_nvme_queue_pair *qpair,
													size_t queue_size,
													size_t alignment, uint16_t entry)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	volatile uint32_t *doorbell_base = &context->ctrl.rgst->doorbell[0].sq_tdbl;
	uint32_t doorbell_stride = 1 << context->ctrl.rgst->cap.dstrd;
	uint32_t msize;
	pr_debug("doorbell base: 0x%lx\n", (uintptr_t)doorbell_base);
	pr_debug("doorbell_stride: %lu\n", (uintptr_t)doorbell_stride);

	qpair->id = entry;

	/* Create submission queue */
	qpair->sq.head = 0;
	qpair->sq.tail = 0;
	qpair->sq.size = queue_size;
	qpair->sq.doorbell = doorbell_base + (2 * entry + 0) * doorbell_stride;
	pr_debug("sq.doorbell: 0x%lx\n", (uintptr_t)qpair->sq.doorbell);

	msize = sizeof(struct tegrabl_nvme_sq_cmd) * queue_size;
	msize = ALIGN(msize, alignment);

	qpair->sq.entries = (struct tegrabl_nvme_sq_cmd *)
		tegrabl_alloc_align(TEGRABL_HEAP_DMA, alignment, msize);
	if (qpair->sq.entries == NULL) {
		err = TEGRABL_ERR_NO_MEMORY;
		pr_error("%s: Failed to allocate memory for sq\n", __func__);
		return err;
	}

	memset(qpair->sq.entries, 0, msize);
	tegrabl_arch_clean_dcache_range((uintptr_t)qpair->sq.entries, msize);

	pr_debug("SMMU: protection on SQ @%p\n", qpair->sq.entries);
	err = nvme_smmu_protect(context,
							(void *)qpair->sq.entries,
							&msize,
							SMMU_WRITE);
	qpair->sq.msize = msize;
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("SMMU: Failed protection on SQ @%p\n", qpair->sq.entries);
		err = TEGRABL_NO_ERROR;
	}

	/* Create completion queue */
	qpair->cq.head = 0;
	qpair->cq.tail = 0;
	qpair->cq.phase = 1;
	qpair->cq.size = queue_size;
	qpair->cq.doorbell = doorbell_base + (2 * entry + 1) * doorbell_stride;
	pr_debug("cq.doorbell: 0x%lx\n", (uintptr_t)qpair->cq.doorbell);

	msize = sizeof(struct tegrabl_nvme_cq_cmd) * queue_size;
	msize = ALIGN(msize, alignment);

	qpair->cq.entries = (struct tegrabl_nvme_cq_cmd *)
		tegrabl_alloc_align(TEGRABL_HEAP_DMA, alignment, msize);
	if (qpair->cq.entries == NULL) {
		err = TEGRABL_ERR_NO_MEMORY;
		pr_error("%s: Failed to allocate memory for cq\n", __func__);
		nvme_free_buffer(context, (void *)qpair->sq.entries, qpair->sq.msize);
		return err;
	}
	memset(qpair->cq.entries, 0, msize);
	tegrabl_arch_clean_dcache_range((uintptr_t)qpair->cq.entries, msize);

	pr_debug("SMMU: protection on CQ @%p\n", qpair->cq.entries);
	err = nvme_smmu_protect(context,
							(void *)qpair->cq.entries,
							&msize,
							SMMU_WRITE);
	qpair->cq.msize = msize;
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("SMMU: Failed protection on CQ @%p\n", qpair->cq.entries);
		err = TEGRABL_NO_ERROR;
	}

	return err;
}

static tegrabl_error_t tegrabl_change_ctrl_status(struct tegrabl_nvme_registers volatile *const rgst,
												  uint8_t status)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t status_bit = status & 1;
	if (rgst->csts.rdy == status_bit)
		return err;

	rgst->cc.en = status_bit;
	time_t current_time = tegrabl_get_timestamp_ms();
	time_t expired_time = rgst->cap.to * 500;

	/* wait for cts.rdy to have the same status_bit as cc.en */
	while (rgst->csts.rdy != status_bit) {
		if (tegrabl_get_timestamp_ms() - current_time > expired_time) {
			return TEGRABL_ERR_TIMEOUT;
		};
	}
	return err;
}

static tegrabl_error_t tegrabl_create_prp1(struct tegrabl_nvme_context *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	size_t alignment = context->page_size;
	uint32_t msize;

	TEGRABL_ASSERT(alignment >= sizeof(struct tegrabl_nvme_ctrlr_data));

	msize = alignment;
	context->ctrl.prp_list.prp1 = tegrabl_alloc_align(TEGRABL_HEAP_DMA, alignment, msize);
	if (context->ctrl.prp_list.prp1 == NULL) {
		err = TEGRABL_ERR_NO_MEMORY;
		pr_error("%s: Failed to allocate memory\n", __func__);
		return err;
	}

	pr_debug("SMMU: protection on prp1 @%p\n", context->ctrl.prp_list.prp1);
	err = nvme_smmu_protect(context,
							(void *)context->ctrl.prp_list.prp1,
							&msize,
							SMMU_READ | SMMU_WRITE);
	context->ctrl.prp_list.prp1_msize = msize;
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("SMMU: Failed protection on prp1. @%p\n", context->ctrl.prp_list.prp1);
		err = TEGRABL_NO_ERROR;
	}

	return err;
}

static tegrabl_error_t tegrabl_create_prp_list(struct tegrabl_nvme_context *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t msize;

	/* Preallocate prp list using max transfer size */
	/* Max transfer size is 512KiB */
	size_t alignment = context->page_size;
	size_t min_page_size = 1 << (context->ctrl.rgst->cap.mpsmin + 12);
	size_t prplist_size = context->ctrl.cdata.mdts == 0 ?
		MAX_TRANSFER : (1 << context->ctrl.cdata.mdts) * min_page_size;
	prplist_size = MIN(MAX_TRANSFER, prplist_size);

	pr_debug("prplist size 0x%lx\n", prplist_size);
	pr_debug("alignment 0x%lx\n", alignment);
	pr_debug("mdts 0x%lx\n", (1 << context->ctrl.cdata.mdts) * min_page_size);

	msize = prplist_size / alignment * sizeof(uint64_t);
	msize = ALIGN(msize, alignment);
	context->ctrl.prp_list.prp_list = tegrabl_alloc_align(TEGRABL_HEAP_DMA, alignment, msize);
	if (context->ctrl.prp_list.prp_list == NULL) {
		err = TEGRABL_ERR_NO_MEMORY;
		pr_error("%s: Failed to allocate prp_list\n", __func__);
		return err;
	}

	pr_debug("SMMU: protection on prp_list @%p\n", context->ctrl.prp_list.prp_list);
	err = nvme_smmu_protect(context,
							(void *)context->ctrl.prp_list.prp_list,
							&msize,
							SMMU_READ | SMMU_WRITE);
	context->ctrl.prp_list.prp_list_msize = msize;
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("SMMU: Failed protection on prp_list @%p\n", context->ctrl.prp_list.prp_list);
		err = TEGRABL_NO_ERROR;
	}

	context->ctrl.prp_list.max_size = prplist_size;
	context->ctrl.prp_list.max_entries = prplist_size / alignment;
	pr_debug("max_entries 0x%lx\n", context->ctrl.prp_list.max_entries);
	return err;
}

static inline void tegrabl_sq_ring_doorbell(struct tegrabl_nvme_sq *sq)
{
	NV_WRITE32(sq->doorbell, sq->tail);
}

static inline void tegrabl_cq_ring_doorbell(struct tegrabl_nvme_cq *cq)
{
	NV_WRITE32(cq->doorbell, cq->head);
}

static inline void tegrabl_increment_command_id(struct tegrabl_nvme_context *context)
{
	context->ctrl.current_cid = context->ctrl.current_cid + 1;
	if (context->ctrl.current_cid == 0xffff)
		context->ctrl.current_cid = 0;
}

static inline void tegrabl_advance_tail_sq(struct tegrabl_nvme_sq *sq)
{
	sq->tail += 1;
	if (sq->tail == sq->size)
		sq->tail = 0;
}

static inline struct tegrabl_nvme_sq_cmd *tegrabl_get_current_entry(struct tegrabl_nvme_queue_pair *q_pair)
{
	uint64_t current_tail = q_pair->sq.tail;
	struct tegrabl_nvme_sq_cmd *current_entry = &q_pair->sq.entries[current_tail];
	memset(current_entry, 0, sizeof(struct tegrabl_nvme_sq_cmd));
	return current_entry;
}

static inline void tegrabl_advance_head_cq(struct tegrabl_nvme_cq *cq)
{
	cq->head++;
	if (cq->head == cq->size) {
		cq->head = 0;
		cq->phase = !cq->phase;
	}
}

static tegrabl_error_t tegrabl_wait_for_command(struct tegrabl_nvme_cq *cq, time_t timeout_ms, uint16_t cid,
												struct tegrabl_nvme_cq_cmd *status)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	time_t starttime = tegrabl_get_timestamp_ms();
	uint16_t status_code, sct;
	uint16_t phase;

	while (true) {
		/* invalidate here */
		/* cq needs a phase bit */
		/* initial state is to 1 */
		tegrabl_arch_invalidate_dcache_range((uintptr_t)&(cq->entries[cq->head]),
											 sizeof(struct tegrabl_nvme_cq_cmd));
		status_code = cq->entries[cq->head].sf.sc;
		sct = cq->entries[cq->head].sf.sct;
		phase = cq->entries[cq->head].sf.p;
		if (phase == cq->phase) {
			if (cq->entries[cq->head].cid == cid) {
				if (!(sct == NVME_SCTYPE_GENERIC && status_code == NVME_STATUS_SUCCESS)) {
					err = TEGRABL_ERR_COMMAND_FAILED;
					pr_error("%s: NVME command failed status code type: 0x%x\n", __func__, sct);
					pr_error("%s: NVME command failed status code: 0x%x\n", __func__, status_code);
				}
				break;
			} else {
				pr_warn("%s: cid does not match. Need cid=%u. Got cid=%u\n", __func__,
																			  cid,
																			  cq->entries[cq->head].cid);
			}
		}
		if (tegrabl_get_timestamp_ms() - starttime > timeout_ms) {
			pr_error("%s: Time out when waiting for command to finish\n", __func__);
			return TEGRABL_ERR_TIMEOUT;
		}
	}
	if (status != NULL) {
		*status = cq->entries[cq->head];
		pr_debug("status->cdw0: %x\n", status->cdw0);
	}

	/* Advance cq head */
	tegrabl_advance_head_cq(cq);

	return err;
}

static inline tegrabl_error_t tegrabl_exec_cmd(struct tegrabl_nvme_context *context,
											   struct tegrabl_nvme_sq_cmd *current_entry,
											   struct tegrabl_nvme_queue_pair *q_pair,
											   struct tegrabl_nvme_cq_cmd *status)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	tegrabl_advance_tail_sq(&q_pair->sq);
	tegrabl_sq_ring_doorbell(&q_pair->sq);
	err = tegrabl_wait_for_command(&q_pair->cq, TIMEOUT_IN_MS, current_entry->cid, status);
	tegrabl_cq_ring_doorbell(&q_pair->cq);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: NVME command error\n", __func__);
		return err;
	}

	return err;
}

static tegrabl_error_t tegrabl_set_feature_numqueue_cmd(struct tegrabl_nvme_context *context, uint16_t q_num,
														struct tegrabl_nvme_cq_cmd *status)
{
	struct tegrabl_nvme_sq_cmd *current_entry = tegrabl_get_current_entry(&context->ctrl.admin_q);

	current_entry->opc = NVME_SET_FEATURES_OPCODE;
	current_entry->cid = context->ctrl.current_cid;
	tegrabl_increment_command_id(context);
	current_entry->cdw10 = NVME_FEAT_NUM_Q;
	current_entry->cdw11 = ((q_num - 1) << 16) | (q_num - 1);

	tegrabl_arch_clean_dcache_range((uintptr_t)current_entry, sizeof(struct tegrabl_nvme_sq_cmd));

	return tegrabl_exec_cmd(context, current_entry, &context->ctrl.admin_q, status);
}

static tegrabl_error_t tegrabl_create_io_queue_cmd(struct tegrabl_nvme_context *context,
												   struct tegrabl_nvme_queue_pair *qpair,
												   enum queue_type type,
												   uint16_t queue_size)
{
	struct tegrabl_nvme_sq_cmd *current_entry = tegrabl_get_current_entry(&context->ctrl.admin_q);

	switch (type) {
	case NVME_SQ_TYPE:
		current_entry->opc = NVME_CREATE_IO_SQ_OPCODE;
		current_entry->cdw11 = (qpair->id << 16) | 0x1;
		current_entry->dptr.prp1 = (uint64_t)qpair->sq.entries;
		break;
	case NVME_CQ_TYPE:
		current_entry->opc = NVME_CREATE_IO_CQ_OPCODE;
		current_entry->cdw11 = 0x1;
		current_entry->dptr.prp1 = (uint64_t)qpair->cq.entries;
		break;
	default:
		return TEGRABL_ERR_INVALID;
	}
	current_entry->cid = context->ctrl.current_cid;
	tegrabl_increment_command_id(context);

	pr_info("%s: queue size: %u\n", __func__, queue_size);
	current_entry->cdw10 = ((queue_size - 1) << 16) | qpair->id;
	tegrabl_arch_clean_dcache_range((uintptr_t)current_entry, sizeof(struct tegrabl_nvme_sq_cmd));
	return tegrabl_exec_cmd(context, current_entry, &context->ctrl.admin_q, NULL);
}

static tegrabl_error_t tegrabl_delete_io_queue_cmd(struct tegrabl_nvme_context *context,
												   struct tegrabl_nvme_queue_pair *qpair,
												   enum queue_type type)
{
	struct tegrabl_nvme_sq_cmd *current_entry = tegrabl_get_current_entry(&context->ctrl.admin_q);

	switch (type) {
	case NVME_SQ_TYPE:
		current_entry->opc = NVME_DELETE_IO_SQ_OPCODE;
		break;
	case NVME_CQ_TYPE:
		current_entry->opc = NVME_DELETE_IO_CQ_OPCODE;
		break;
	default:
		return TEGRABL_ERR_INVALID;
	}
	current_entry->cid = context->ctrl.current_cid;
	tegrabl_increment_command_id(context);

	current_entry->cdw10 = qpair->id;
	tegrabl_arch_clean_dcache_range((uintptr_t)current_entry, sizeof(struct tegrabl_nvme_sq_cmd));
	return tegrabl_exec_cmd(context, current_entry, &context->ctrl.admin_q, NULL);
}

static tegrabl_error_t tegrabl_identify_cmd(struct tegrabl_nvme_context *context, uint32_t nsid,
											enum tegrabl_nvme_identify_cns cmd)
{
	struct tegrabl_nvme_sq_cmd *current_entry = tegrabl_get_current_entry(&context->ctrl.admin_q);

	current_entry->opc = NVME_IDENTIFY_OPCODE;
	current_entry->cid = context->ctrl.current_cid;
	tegrabl_increment_command_id(context);
	current_entry->nsid = nsid;
	current_entry->cdw10 = cmd;

	/* Assign the dptr structure */
	memset(context->ctrl.prp_list.prp1, 0, context->page_size);
	tegrabl_arch_clean_dcache_range((uintptr_t)context->ctrl.prp_list.prp1, context->page_size);
	TEGRABL_ASSERT(context->page_size >= sizeof(struct tegrabl_nvme_ctrlr_data));
	TEGRABL_ASSERT(context->page_size >= sizeof(struct tegrabl_nvme_ns_data));

	current_entry->dptr.prp1 = (uint64_t)context->ctrl.prp_list.prp1;
	current_entry->dptr.prp2 = 0;
	tegrabl_arch_clean_dcache_range((uintptr_t)current_entry, sizeof(struct tegrabl_nvme_sq_cmd));

	return tegrabl_exec_cmd(context, current_entry, &context->ctrl.admin_q, NULL);
}

static tegrabl_error_t tegrabl_read_cmd(struct tegrabl_nvme_context *context, bnum_t blknr, bnum_t blkcnt,
										uint64_t prp1, uint64_t prp2)
{
	struct tegrabl_nvme_sq_cmd *current_entry = tegrabl_get_current_entry(&context->ctrl.io_q);

	current_entry->opc = NVME_READ_OPCODE;
	current_entry->cid = context->ctrl.current_cid;
	tegrabl_increment_command_id(context);
	current_entry->nsid = context->ctrl.chosen_nsid;

	current_entry->cdw10 = blknr;
	current_entry->cdw12 = 0xffff & (blkcnt - 1);

	current_entry->dptr.prp1 = prp1;
	current_entry->dptr.prp2 = prp2;
	tegrabl_arch_clean_dcache_range((uintptr_t)current_entry, sizeof(struct tegrabl_nvme_sq_cmd));

	return tegrabl_exec_cmd(context, current_entry, &context->ctrl.io_q, NULL);
}

static tegrabl_error_t tegrabl_write_cmd(struct tegrabl_nvme_context *context, bnum_t blknr, bnum_t blkcnt,
										 uint64_t prp1, uint64_t prp2)
{
	struct tegrabl_nvme_sq_cmd *current_entry = tegrabl_get_current_entry(&context->ctrl.io_q);

	current_entry->opc = NVME_WRITE_OPCODE;
	current_entry->cid = context->ctrl.current_cid;
	tegrabl_increment_command_id(context);
	current_entry->nsid = context->ctrl.chosen_nsid;

	current_entry->cdw10 = blknr;
	current_entry->cdw12 = 0xffff & (blkcnt - 1);

	current_entry->dptr.prp1 = prp1;
	current_entry->dptr.prp2 = prp2;
	tegrabl_arch_clean_dcache_range((uintptr_t)current_entry, sizeof(struct tegrabl_nvme_sq_cmd));

	return tegrabl_exec_cmd(context, current_entry, &context->ctrl.io_q, NULL);
}

static tegrabl_error_t tegrabl_create_io_queue(struct tegrabl_nvme_context *context, uint16_t queue_size,
											   uint16_t entry)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t nsqa, ncqa;
	struct tegrabl_nvme_cq_cmd status;

	err = tegrabl_construct_nvme_qpair(context, &context->ctrl.io_q, queue_size, context->page_size, entry);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed construct nvme io queue; error=0x%x\n", __func__, err);
		return err;
	}

	context->ctrl.rgst->cc.iocqes = 4; /* log of sizeof(tegrabl_nvme_cq_cmd); minimum value */
	context->ctrl.rgst->cc.iosqes = 6; /* log of sizeof(tegrabl_nvme_sq_cmd); minimum value */

	err = tegrabl_set_feature_numqueue_cmd(context, DEFAULT_IO_Q, &status);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_set_feature_numqueue; error=0x%x\n", __func__, err);
		goto fail;
	}

	nsqa = (status.cdw0 & 0xFFFF) + 1;
	ncqa = (status.cdw0 >> 16) + 1;

	if (DEFAULT_IO_Q > MIN(nsqa, ncqa)) {
		err = TEGRABL_ERR_NO_RESOURCE;
		pr_error("%s: Failed not enough io queue\n", __func__);
		goto fail;
	}

	pr_debug("%s: Create IO completion queue\n", __func__);
	err = tegrabl_create_io_queue_cmd(context, &context->ctrl.io_q, NVME_CQ_TYPE, queue_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_create_io_queue_cmd(NVME_CQ_TYPE) ; error=0x%x\n", __func__, err);
		goto fail;
	}

	pr_debug("%s: Create IO submission queue\n", __func__);
	err = tegrabl_create_io_queue_cmd(context, &context->ctrl.io_q, NVME_SQ_TYPE, queue_size);
	if (err != TEGRABL_NO_ERROR) {
		tegrabl_delete_io_queue_cmd(context, &context->ctrl.io_q, NVME_CQ_TYPE);
		pr_error("%s: Failed tegrabl_create_io_queue_cmd(NVME_SQ_TYPE) ; error=0x%x\n", __func__, err);
		goto fail;
	}
	goto success;

fail:
	nvme_free_buffer(context, (void *)context->ctrl.io_q.cq.entries, context->ctrl.io_q.cq.msize);
	nvme_free_buffer(context, (void *)context->ctrl.io_q.sq.entries, context->ctrl.io_q.sq.msize);

success:
	return err;
}

static tegrabl_error_t tegrabl_identify_namepace(struct tegrabl_nvme_context *context,
												 struct tegrabl_nvme_ns_data *nsdata,
												 uint32_t nsid)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_identify_cmd(context, nsid, NVME_IDENTIFY_NAMESPACE);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_identify_cmd; error=0x%x\n", __func__, err);
		return err;
	}

	tegrabl_arch_invalidate_dcache_range((uintptr_t)context->ctrl.prp_list.prp1, context->page_size);
	TEGRABL_ASSERT(context->page_size >= sizeof(struct tegrabl_nvme_ns_data));
	memcpy(nsdata, context->ctrl.prp_list.prp1, sizeof(struct tegrabl_nvme_ns_data));

	return err;
}

static tegrabl_error_t tegrabl_choose_namespace(struct tegrabl_nvme_context *context, uint32_t nsid)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_identify_namepace(context, &context->ctrl.nsdata, nsid);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_identify_namepace; error=0x%x\n", __func__, err);
		return err;
	}
	context->ctrl.chosen_nsid = nsid;
	context->block_count = context->ctrl.nsdata.nsze;
	uint8_t supported_lba = context->ctrl.nsdata.flbas.format;
	context->block_size_log2 = context->ctrl.nsdata.lbaf[supported_lba].lbads;
	context->max_transfer_blk = context->ctrl.prp_list.max_size / (1 << context->block_size_log2);
	pr_info("block_count: %u\n", context->block_count);
	pr_info("supported_lba: %u\n", supported_lba);
	pr_info("block_size_log2: 0x%lx\n", context->block_size_log2);
	return err;
}

static tegrabl_error_t tegrabl_identify_controller(struct tegrabl_nvme_context *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_identify_cmd(context, 0, NVME_IDENTIFY_CONTROLLER);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_identify_cmd; error=0x%x\n", __func__, err);
		return err;
	}

	struct tegrabl_nvme_ctrlr_data *ctrl_data = &context->ctrl.cdata;
	tegrabl_arch_invalidate_dcache_range((uintptr_t)context->ctrl.prp_list.prp1, context->page_size);
	TEGRABL_ASSERT(context->page_size >= sizeof(struct tegrabl_nvme_ctrlr_data));
	memcpy(ctrl_data, context->ctrl.prp_list.prp1, sizeof(struct tegrabl_nvme_ctrlr_data));

	pr_debug("vid: 0x%x\n", ctrl_data->vid);
	pr_debug("ssvid: 0x%x\n", ctrl_data->ssvid);
	pr_debug("ieee: %x %x %x\n", ctrl_data->ieee[0], ctrl_data->ieee[1], ctrl_data->ieee[2]);
	pr_debug("vwc: 0x%x\n", ctrl_data->vwc.present);
	pr_debug("mnan: 0x%x\n", ctrl_data->mnan);
	tegrabl_nvme_print_serial(ctrl_data->sn, NVME_SERIAL_NUM_CHAR, "NVMe serial number:");
	tegrabl_nvme_print_serial(ctrl_data->mn, NVME_MODEL_NUM_CHAR, "NVMe model number:");
	tegrabl_nvme_print_serial(ctrl_data->fr, NVME_FR_REV_NUM_CHAR, "NVMe firmware revision:");

	return err;
}

/**
 * @brief initializes the nvme controller and context
 *
 * @param context nvme context
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_nvme_init(struct tegrabl_nvme_context *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_pcie_dev *pcie_dev;
	uint8_t pcie_ctlr_num = context->instance;

	/* Initiate SMMU driver */
	err = tegrabl_smmu_init();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_smmu_init; error=0x%x\n", __func__, err);
		goto fail;
	}

	/* Initiate pcie device */
	err = tegrabl_pcie_init(pcie_ctlr_num, 1);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_pcie_init(%u); error=0x%x\n", __func__, pcie_ctlr_num, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto smmu_deinit;
	}

	/* get the NVME pcie device */
	pcie_dev = tegrabl_pcie_get_dev(NVME_CLASS_CODE, PCIE_ID_TYPE_CLASS);
	if (!pcie_dev) {
		pr_error("%s: Failed tegrabl_pcie_get_dev(%u); error=0x%x\n", __func__, pcie_ctlr_num, err);
		err = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, TEGRABL_ERR_NVME_CTLR_INIT);
		goto reset;
	}
	context->pcie_dev = pcie_dev;
	pr_info("pcie_dev=%p\n", pcie_dev);
	pr_info("Found NVMe pcie device\n");

	{
		void *fdt;
		int32_t pcie_node_offset = 0;

		err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
		if (err == TEGRABL_NO_ERROR) {
			pcie_node_offset = tegrabl_get_pcie_ctrl_node_offset(pcie_ctlr_num);
			if (pcie_node_offset != 0) {
				err = tegrabl_smmu_add_device(fdt, pcie_node_offset, &pcie_dev->smmu_cookie);
				if (err == TEGRABL_NO_ERROR) {
					pr_debug("%s: Successfully add ctrl %u to SMMU prot.\n", __func__, pcie_ctlr_num);
					context->smmu_en = true;
				} else {
					pr_warn("%s: Cannot add ctrl %u to SMMU prot.\n", __func__, pcie_ctlr_num);
					/* ignore the error, treat it as no SMMU. */
				}
			}
		}
		err = TEGRABL_NO_ERROR;
	}

	/* get bar0 address from PCIe device */
	uint64_t bar = pcie_dev->bar[0].start;
	pr_debug("NVME: bar=0x%lx\n", bar);

	/* initialize cap register from bar0 PCIe */
	context->ctrl.rgst = (struct tegrabl_nvme_registers volatile *)bar;
	struct tegrabl_nvme_registers volatile *const rgst = context->ctrl.rgst;
	pr_debug("register mqes: %d\n", context->ctrl.rgst->cap.mqes);
	pr_debug("register to: %d\n", context->ctrl.rgst->cap.to);

	context->page_size_log2 = MAX(rgst->cap.mpsmin + 12, PAGE_SIZE_LOG2);
	context->page_size_log2 = MIN(rgst->cap.mpsmax + 12, context->page_size_log2);
	context->page_size = 1 << context->page_size_log2;
	pr_info("NVME page size: %lu\n", context->page_size);

	if (rgst->cap.css_nvm == 0 || rgst->cap.css_no_io == 1) {
		pr_error("%s: Device do not support I/O operations (%u)\n", __func__, pcie_ctlr_num);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, TEGRABL_ERR_NVME_CTLR_INIT);
		goto reset;
	}

	/* Enable PCIe bus */
	uint32_t cmd_status = 0;
	tegrabl_pcie_conf_read(pcie_dev, 4, &cmd_status);
	cmd_status |= 0x6;
	tegrabl_pcie_conf_write(pcie_dev, 4, cmd_status);

	/* Disable the controller */
	err = tegrabl_change_ctrl_status(rgst, 0);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Change controller status failed (%u); error=0x%x\n", __func__, pcie_ctlr_num, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto reset;
	}

	/* set up admin queue aqa register */
	uint16_t queue_size = MIN(rgst->cap.mqes, QUEUE_SIZE);
	rgst->aqa.acqs = queue_size - 1; /* zero based value */
	rgst->aqa.asqs = queue_size - 1; /* zero based value */

	/* allocate admin queue */
	err = tegrabl_construct_nvme_qpair(context, &context->ctrl.admin_q, queue_size, context->page_size, 0);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed construct nvme admin queue(%u); error=0x%x\n", __func__, pcie_ctlr_num, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto reset;
	}

	/* set up admin queue asq and acq register */
	rgst->asq = (uint64_t)context->ctrl.admin_q.sq.entries;
	rgst->acq = (uint64_t)context->ctrl.admin_q.cq.entries;

	/* set up page size mps, arbitration mechanism ams and I/O command set */
	rgst->cc.mps = context->page_size_log2 - 12;
	rgst->cc.ams = 0;
	rgst->cc.css = 0;

	/* Reenable the controller */
	err = tegrabl_change_ctrl_status(rgst, 1);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Change controller status failed (%u); error=0x%x\n", __func__, pcie_ctlr_num, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto adminq;
	}

	/* Construct first prp list */
	err = tegrabl_create_prp1(context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_create_prp; error=0x%x\n", __func__, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto adminq;
	}

	/* Submit identify controller command */
	err = tegrabl_identify_controller(context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_identify_controller; error=0x%x\n", __func__, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto adminq;
	}

	/* Create second prp list */
	err = tegrabl_create_prp_list(context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_create_prp_list; error=0x%x\n", __func__, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto prp1;
	}

	/* Pick namespace as boot device */
	err = tegrabl_choose_namespace(context, DEFAULT_NSID);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_choose_namespace; error=0x%x\n", __func__, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto prplist;
	}

	err = tegrabl_create_io_queue(context, QUEUE_SIZE, 1);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed tegrabl_create_io_queue; error=0x%x\n", __func__, err);
		err = TEGRABL_ERROR(err, TEGRABL_ERR_NVME_CTLR_INIT);
		goto prplist;
	}

	goto success;

prplist:
	nvme_free_buffer(context, (void *)context->ctrl.prp_list.prp_list, context->ctrl.prp_list.prp_list_msize);

prp1:
	nvme_free_buffer(context, (void *)context->ctrl.prp_list.prp1, context->ctrl.prp_list.prp1_msize);

adminq:
	nvme_free_buffer(context, (void *)context->ctrl.admin_q.cq.entries, context->ctrl.admin_q.cq.msize);
	nvme_free_buffer(context, (void *)context->ctrl.admin_q.sq.entries, context->ctrl.admin_q.sq.msize);

	if (context->smmu_en)
		tegrabl_smmu_remove_device(context->pcie_dev->smmu_cookie);

reset:
	tegrabl_nvme_reset_pcie(context);

smmu_deinit:
	tegrabl_smmu_deinit();

fail:
success:
	return err;
}

static tegrabl_error_t tegrabl_prepare_prp2(struct tegrabl_nvme_context *context, size_t len,
											dma_addr_t buffer, uint64_t *prp2)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	size_t page_size = context->page_size;
	uint64_t *prp_list;
	int32_t length = len;
	uint32_t i;
	uint32_t prp_entries;
	dma_addr_t dma_address = buffer;
	int offset = dma_address & (page_size - 1);

	length -= (page_size - offset);

	if (length <= 0) {
		*prp2 = 0;
		goto exit;
	}

	dma_address += (page_size - offset);

	if ((size_t)length <= page_size) {
		*prp2 = dma_address;
		goto exit;
	}

	/* Divide round up */
	prp_entries = 1 + ((length - 1) / page_size);

	pr_debug("prp_entries: 0x%x\n", prp_entries);
	pr_debug("max_entries: 0x%lx\n", context->ctrl.prp_list.max_entries);
	pr_debug("page_size: 0x%lx\n", page_size);

	if (prp_entries > context->ctrl.prp_list.max_entries) {
		pr_error("%s: not enough PRP entries (prp_entries=%u, max_entries=%lu)\n", __func__, prp_entries,
		context->ctrl.prp_list.max_entries);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_ERR_NVME_PRPS);
		goto exit;
	}

	prp_list = context->ctrl.prp_list.prp_list;
	memset(prp_list, 0, context->ctrl.prp_list.max_entries * sizeof(uint64_t));
	for (i = 0; i < prp_entries; i++) {
		prp_list[i] = dma_address;
		dma_address += page_size;
	}
	*prp2 = (uint64_t)context->ctrl.prp_list.prp_list;

	tegrabl_arch_clean_dcache_range((uintptr_t)context->ctrl.prp_list.prp_list,
									(size_t)(prp_entries * sizeof(uint64_t)));

exit:
	pr_trace("%s: return *prp2=0x%lx, error=0x%x\n", __func__, *prp2, err);
	return err;
}

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
									   bnum_t blknr, bnum_t blkcnt, bool write)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	size_t total_len = blkcnt << context->block_size_log2;
	uint64_t prp2 = 0;
	bnum_t startblock = blknr;
	bnum_t count = blkcnt;
	dma_addr_t buf = (dma_addr_t)buffer;
	size_t bulk_count;
	uint32_t tsize;

	pr_debug("%s: %s blknr=0x%x, blkcnt=0x%x\n", __func__, write ? "Write" : "Read", blknr, blkcnt);
	pr_debug("total_len=0x%lx\n", total_len);

	pr_debug("SMMU: protection on rw_blocks @(%p, 0x%lx)\n", buffer, total_len);
	tsize = total_len;
	err = nvme_smmu_protect(context,
							buffer,
							&tsize,
							SMMU_READ | SMMU_WRITE);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("SMMU: Failed protection on rw_blocks. @%p\n", buffer);
		err = TEGRABL_NO_ERROR;
	}

	if (write) {
		tegrabl_arch_clean_dcache_range((uintptr_t)buffer, total_len);
	}

	while (count > 0UL) {
		bulk_count = MIN(count, context->max_transfer_blk);

		err = tegrabl_prepare_prp2(context, bulk_count << context->block_size_log2, buf, &prp2);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: Failed tegrabl_prepare_prp2; error=0x%x\n", __func__, err);
			goto fail;
		}

		if (write) {
			err = tegrabl_write_cmd(context, startblock, bulk_count, buf, prp2);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s: Failed tegrabl_write_cmd; error=0x%x\n", __func__, err);
				goto fail;
			}
		} else {
			err = tegrabl_read_cmd(context, startblock, bulk_count, buf, prp2);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s: Failed tegrabl_read_cmd; error=0x%x\n", __func__, err);
				goto fail;
			}
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		startblock += bulk_count;
	}

	if (!write) {
		tegrabl_arch_invalidate_dcache_range((uintptr_t)buffer, (size_t)total_len);
	}

	pr_debug("SMMU: free rw_blocks @(%p, 0x%lx)\n", buffer, total_len);
	if (context->smmu_en && tsize) {
		nvme_smmu_unprotect(context, buffer, (uint32_t)total_len);
	}

fail:
	return err;
}

/**
 * @brief frees all buffers used by nvme controller
 *
 * @param context nvme context
 * @return void
 */
void tegrabl_nvme_free_buffers(struct tegrabl_nvme_context *context)
{
	nvme_free_buffer(context, (void *)context->ctrl.prp_list.prp1, context->ctrl.prp_list.prp1_msize);
	nvme_free_buffer(context, (void *)context->ctrl.prp_list.prp_list, context->ctrl.prp_list.prp_list_msize);
	nvme_free_buffer(context, (void *)context->ctrl.admin_q.cq.entries, context->ctrl.admin_q.cq.msize);
	nvme_free_buffer(context, (void *)context->ctrl.admin_q.sq.entries, context->ctrl.admin_q.sq.msize);
	nvme_free_buffer(context, (void *)context->ctrl.io_q.cq.entries, context->ctrl.io_q.cq.msize);
	nvme_free_buffer(context, (void *)context->ctrl.io_q.sq.entries, context->ctrl.io_q.sq.msize);

	if (context->smmu_en)
		tegrabl_smmu_remove_device(context->pcie_dev->smmu_cookie);
}

/**
 * @brief resets the pcie controller
 *
 * @param context nvme context
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
tegrabl_error_t tegrabl_nvme_reset_pcie(struct tegrabl_nvme_context *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	err = tegrabl_pcie_reset_state(context->instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: tegrabl_pcie_reset_state; error=0x%x\n", __func__, err);
		goto fail;
	}

fail:
	return err;
}
