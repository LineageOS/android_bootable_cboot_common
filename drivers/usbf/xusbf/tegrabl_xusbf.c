/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_XUSBF

#include "build_config.h"
#include <string.h>
#include <stdbool.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <tegrabl_usbf.h>
#include <tegrabl_xusb_priv.h>
#include <tegrabl_xusb_priv_desc.h>
#include <arxusb_padctl.h>
#include <armiscreg.h>
#include <arclk_rst.h>
#include <ardev_t_fpci_xusb_dev_0.h>
#include <ardev_t_xusb_dev_xhci.h>

#if defined(CONFIG_ENABLE_XUSBF_SS)
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_regulator.h>

#define XUDC_DT_COMPATIBLE "nvidia,tegra186-xudc"
#endif

#define USB_REG_DUMP 0

#define SHIFT(PERIPH, REG, FIELD) \
		NV_FIELD_SHIFT(PERIPH##_##REG##_0_##FIELD##_##RANGE)
#define SHIFTMASK(PERIPH, REG, FIELD) \
		NV_FIELD_SHIFTMASK(PERIPH##_##REG##_0_##FIELD##_##RANGE)

#define XUSB_BASE NV_ADDRESS_MAP_XUSB_DEV_BASE
#define USB_DESCRIPTOR_SKU_MASK	0xF

/** We need 2 ring segments of size 16 each for event ring.
 *  Use 1 contiguous segment for simplicity.
 */
#define NUM_TRB_EVENT_RING 32
#define NUM_TRB_TRANSFER_RING 16
#define NUM_EP_CONTEXT  4

/* 512 bytes. */
#define SETUP_DATA_BUFFER_SIZE     (0x200)

uint8_t setup_data[8] TEGRABL_ALIGN(16);

#define TERM_RANGE_ADJ_MASK     0x00000780
#define TERM_RANGE_ADJ_SHIFT    7
#define HS_CURR_LEVEL_MASK      0x0000003F
#define HS_CURR_LEVEL_SHIFT     0
#define HS_SQUELCH_LEVEL_MASK   0xE0000000U
#define HS_SQUELCH_LEVEL_SHIFT  29
#define RPD_CTRL_MASK           0x0000001F
#define RPD_CTRL_SHIFT          0

#if defined(CONFIG_ENABLE_XUSBF_UNCACHED_STRUCT)

#define DRAM_MAP_USB_START      0x80000000LLU

#define EVENT_RING_START    (DRAM_MAP_USB_START)

#define EVENT_RING_SIZE     (NUM_TRB_EVENT_RING * sizeof(struct event_trb))
#define TX_RING_EP0_START   (EVENT_RING_START+EVENT_RING_SIZE)
#define TX_RING_EP0_SIZE    (NUM_TRB_TRANSFER_RING * sizeof(struct data_trb))
#define TX_RING_EP1_OUT_START (TX_RING_EP0_START+TX_RING_EP0_SIZE)
#define TX_RING_EP1_OUT_SIZE  (NUM_TRB_TRANSFER_RING * sizeof(struct data_trb))
#define TX_RING_EP1_IN_START   (TX_RING_EP1_OUT_START+TX_RING_EP1_OUT_SIZE)
#define TX_RING_EP1_IN_SIZE    (NUM_TRB_TRANSFER_RING * sizeof(struct data_trb))
#define EP_CONTEXT_START    (TX_RING_EP1_IN_START+TX_RING_EP1_IN_SIZE)
#define EP_CONTEXT_SIZE     (NUM_EP_CONTEXT*sizeof(struct ep_context))
#define SETUP_DATA_BUFFER_START     (EP_CONTEXT_START+EP_CONTEXT_SIZE)
#define SETUP_DATA_BUFFER_SIZE     (0x200)
#define DRAM_MAP_USB_SIZE ((SETUP_DATA_BUFFER_START + SETUP_DATA_BUFFER_SIZE) \
							- EVENT_RING_START)

static struct event_trb *p_event_ring = (struct event_trb*)EVENT_RING_START;
/* Transfer Ring for Control Endpoint */
static struct data_trb *p_txringep0 = (struct data_trb*)TX_RING_EP0_START;

/* Transfer Ring for Bulk Out Endpoint */
static struct data_trb *p_txringep1out =
			(struct data_trb*)TX_RING_EP1_OUT_START;
/* Transfer Ring for Bulk In Endpoint */
static struct data_trb *p_txringep1in = (struct data_trb*)TX_RING_EP1_IN_START;

static uint8_t *p_setup_buffer = (uint8_t*)SETUP_DATA_BUFFER_START;

/* Endpoint descriptor */
static struct ep_context *p_ep_context = (struct ep_context *)EP_CONTEXT_START;

#else

static struct event_trb event_ring[NUM_TRB_EVENT_RING] TEGRABL_ALIGN(16);
static struct event_trb *p_event_ring = (struct event_trb *)&event_ring[0];

/* Transfer Ring for Control Endpoint */
static struct data_trb tx_ringep0[NUM_TRB_TRANSFER_RING] TEGRABL_ALIGN(16);
static struct data_trb *p_txringep0 = (struct data_trb *)&tx_ringep0[0];

/* Transfer Ring for Bulk Out Endpoint */
static struct data_trb tx_ringep1out[NUM_TRB_TRANSFER_RING] TEGRABL_ALIGN(16);
static struct data_trb *p_txringep1out =
			(struct data_trb *)&tx_ringep1out[0];

/* Transfer Ring for Bulk In Endpoint */
static struct data_trb tx_ringep1[NUM_TRB_TRANSFER_RING] TEGRABL_ALIGN(16);
static struct data_trb *p_txringep1in = (struct data_trb *)&tx_ringep1[0];

static uint8_t setup_buffer[SETUP_DATA_BUFFER_SIZE] TEGRABL_ALIGN(16);
static uint8_t *p_setup_buffer = (uint8_t *)&setup_buffer[0];

/* Endpoint descriptor */
static struct ep_context s_ep_context[NUM_EP_CONTEXT] TEGRABL_ALIGN(64);
static struct ep_context *p_ep_context =
			(struct ep_context *)&s_ep_context[0];
#endif

/* GetStatus() Request to an Interface is always 0 */
uint8_t interface_status[2] = {0, 0};
/* GetStatus() Request to an Interface is always 0 */
uint8_t endpoint_status[2] = {0, 0};

static struct xusb_device_context s_xusb_device_context;

static struct usbf_config *g_usbconfig;

#if USB_REG_DUMP
static void rdump(uint32_t base, uint32_t start, uint32_t end)
{
	uint32_t i = 0;

	pr_info("\n");
	for (; start <= end; i++) {
		pr_info("0x%08x=0x%08x\n", (base + start), NV_READ32(base + start));
		start = start + 4;
	}
}

static void register_dump(void)
{
	uint32_t base = NV_ADDRESS_MAP_CAR_BASE;
	/* Clock register dump
		UFS
		 0x40000 to 0x40014

		XUSB:
		 0x470000 to 0x470008
		 0x471000 to 0x471008
		 0x472000
		 0x473000 t0 0x473010
		 0x474000
	*/

	pr_info("\t UHY CLOCK reg:\n");
	rdump(base, 0x40000, 0x40014);

	pr_info("\t PLLE CLOCK reg:\n");
	rdump(base, 0x43000, 0x4301c);

	pr_info("\t PLLREFE CLOCK reg:\n");
	rdump(base, 0x420000, 0x420018);

	pr_info("\t USB CLOCK reg:\n");
	rdump(base, 0x470000, 0x470008);
	rdump(base, 0x471000, 0x471008);
	rdump(base, 0x472000, 0x472000);
	rdump(base, 0x473000, 0x473010);
	rdump(base, 0x474000, 0x474000);

	pr_info("\t PEX USB CLOCK reg:\n");
	rdump(base, 0x5F0000, 0x5F0008);

	pr_info("\t USB PADCTL reg reg:\n");
	base  = NV_ADDRESS_MAP_XUSB_PADCTL_BASE;

	rdump(base, 0x0, 0x28);
	rdump(base, 0x80, 0x94);
	rdump(base, 0xc0, 0xd4);
	rdump(base, 0x100, 0x114);
	rdump(base, 0x284, 0x288);
	rdump(base, 0x360, 0x360);
}
#endif

static void tegrabl_init_ep_event_ring(void)
{
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	uint32_t reg_data;
	dma_addr_t dma_buf, tbuf;

	/* zero out event ring */
	memset((void *)&p_event_ring[0], 0,
		   NUM_TRB_EVENT_RING * sizeof(struct event_trb));

	/* Initialize Enqueue and Dequeue pointer of consumer context*/
	p_xusb_dev_context->event_dequeue_ptr =
	p_xusb_dev_context->event_enqueue_ptr = (uintptr_t)&p_event_ring[0];
	p_xusb_dev_context->event_ccs = 1;

	/* Set event ring segment 0 and segment 1 */
	/* Segment 0 */
	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
			(void *)&p_event_ring[0],
			(NUM_TRB_EVENT_RING * sizeof(struct event_trb)),
			TEGRABL_DMA_TO_DEVICE);

	p_xusb_dev_context->dma_er_start_address = dma_buf;

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERST0BALO_0, U64_TO_U32_LO(dma_buf));
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERST0BAHI_0, U64_TO_U32_HI(dma_buf));

	/* Segment 1 */
	tbuf = (dma_buf + (NUM_TRB_EVENT_RING/2) * sizeof(struct event_trb));
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERST1BALO_0, U64_TO_U32_LO(tbuf));

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERST1BAHI_0, U64_TO_U32_HI(tbuf));

	/* Write segment sizes */
	reg_data =
		NV_DRF_NUM(XUSB_DEV_XHCI, ERSTSZ, ERST0SZ, NUM_TRB_EVENT_RING/2) |
		NV_DRF_NUM(XUSB_DEV_XHCI, ERSTSZ, ERST1SZ, NUM_TRB_EVENT_RING/2);
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERSTSZ_0, reg_data);

	/* Set Enqueue/Producer Cycle State for controller */
	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EREPLO_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					EREPLO,
					ECS,
					p_xusb_dev_context->event_ccs,
					reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					EREPLO,
					SEGI,
					0,
					reg_data);

	/* Bits 3:0 are not used to indicate 16 byte aligned.
	 * Shift the Enqueue Pointer before using DRF macro.
	 */
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					EREPLO,
					ADDRLO, (U64_TO_U32_LO(dma_buf) >> 4),
					reg_data);

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EREPLO_0, reg_data);

	/* Set 63:32 bits of enqueue pointer. */
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EREPHI_0, U64_TO_U32_HI(dma_buf));

	/* Set the Dequeue Pointer */
	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_ERDPLO_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					ERDPLO,
					ADDRLO, (U64_TO_U32_LO(dma_buf) >> 4),
					reg_data);

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERDPLO_0, reg_data);

	/* Set bits 63:32 of Dequeue pointer. */
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERDPHI_0, U64_TO_U32_HI(dma_buf));
}

static tegrabl_error_t tegrabl_init_transfer_ring(enum endpoint ep_index)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	/* zero out tx ring */
	if (ep_index == EP0_IN || ep_index == EP0_OUT)
		memset((void *)&p_txringep0[0], 0,
			   NUM_TRB_TRANSFER_RING * sizeof(struct event_trb));
	else if (ep_index == EP1_IN)
		memset((void *)&p_txringep1in[0], 0,
			   NUM_TRB_TRANSFER_RING * sizeof(struct event_trb));
	else if (ep_index == EP1_OUT)
		memset((void *)&p_txringep1out[0], 0,
			   NUM_TRB_TRANSFER_RING * sizeof(struct event_trb));
	else {
		e = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		pr_critical("Given endpoint is not supported\n");
	}

	return e;
}

static tegrabl_error_t tegrabl_poll_field(uint32_t reg_addr, uint32_t mask,
		uint32_t expected_value, uint32_t timeout)
{
	uint32_t reg_data;

	do {
		reg_data = NV_READ32(reg_addr);

		if ((reg_data & mask) == expected_value) {
			return TEGRABL_NO_ERROR;
		}
		tegrabl_udelay(1);
		timeout--;
	} while (timeout != 0);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
}

static tegrabl_error_t tegrabl_disable_ep(enum endpoint ep_index)
{
	uint32_t reg_data, expected_value, mask;
	tegrabl_error_t e;
	struct ep_context *ep_info;

	/* Cannot disable endpoint 0. */
	if (ep_index == EP0_IN || ep_index == EP0_OUT) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	ep_info = &p_ep_context[ep_index];

	/* Disable Endpoint */
	ep_info->ep_state = EP_DISABLED;
	reg_data = NV_DRF_NUM(XUSB_DEV_XHCI,
			EP_RELOAD,
			DCI,
			1 << ep_index);

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_RELOAD_0, reg_data);
	/* TODO timeout for polling */
	mask = 1UL << ep_index;
	expected_value = 0;
	e = tegrabl_poll_field(XUSB_BASE+XUSB_DEV_XHCI_EP_RELOAD_0,
		mask,
		expected_value,
		1000);

	return e;
}

static void tegrabl_create_status_trb(
		struct status_trb *p_status_trb, uint32_t dir)
{
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	/* Event gen on Completion. */
	p_status_trb->c = p_xusb_dev_context->cntrl_pcs;
	p_status_trb->ioc = 1;
	p_status_trb->trb_type = STATUS_STAGE_TRB;
	p_status_trb->dir = dir;
}

static tegrabl_error_t tegrabl_queue_trb(enum endpoint ep_index,
			struct normal_trb *p_trb, uint32_t ring_doorbell)
{
	struct link_trb *p_link_trb;
	struct data_trb *p_next_trb;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	uint32_t reg_data;
	tegrabl_error_t e =  TEGRABL_NO_ERROR;
	dma_addr_t dma_buf;

	TEGRABL_UNUSED(dma_buf);

	/* If Control EP */
	if (ep_index == EP0_IN) {
		memcpy((void *)p_xusb_dev_context->cntrl_epenqueue_ptr,
			   (void *)p_trb, sizeof(struct normal_trb));

		p_next_trb = (struct data_trb *)p_xusb_dev_context->cntrl_epenqueue_ptr;
		p_next_trb++;
		/* Handle Link TRB */
		if (p_next_trb->trb_type == LINK_TRB) {
			p_link_trb = (struct link_trb *)p_next_trb;
			p_link_trb->c = p_xusb_dev_context->cntrl_pcs;
			p_link_trb->tc = 1;
			/* next trb after link is always index 0 */
			p_next_trb = &p_txringep0[0];

			/* Toggle cycle bit */
			p_xusb_dev_context->cntrl_pcs ^= 1;
		}

		dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
			(void *)p_xusb_dev_context->cntrl_epenqueue_ptr,
			sizeof(struct normal_trb), TEGRABL_DMA_TO_DEVICE);

		/* TODO Add check for full ring */
		p_xusb_dev_context->cntrl_epenqueue_ptr = (uintptr_t)p_next_trb;
	}
	/* Bulk Endpoint */
	else if (ep_index == EP1_OUT) {
		memcpy((void *)p_xusb_dev_context->bulkout_epenqueue_ptr,
			   (void *)(uintptr_t)p_trb, sizeof(struct normal_trb));
		p_next_trb = (struct data_trb *)
						p_xusb_dev_context->bulkout_epenqueue_ptr;
		p_next_trb++;
		/* Handle Link TRB */
		if (p_next_trb->trb_type == LINK_TRB) {
			p_link_trb = (struct link_trb *)p_next_trb;
			p_link_trb->c = p_xusb_dev_context->bulkout_pcs;
			p_link_trb->tc = 1;
			p_next_trb = &p_txringep1out[0];
			p_xusb_dev_context->bulkout_pcs ^= 1;
		}

		dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
			(void *)p_xusb_dev_context->bulkout_epenqueue_ptr,
			sizeof(struct normal_trb), TEGRABL_DMA_TO_DEVICE);

		p_xusb_dev_context->bulkout_epenqueue_ptr = (uintptr_t)p_next_trb;
	}
	/* Bulk Endpoint */
	else if (ep_index == EP1_IN) {
		memcpy((void *)p_xusb_dev_context->bulkin_epenqueue_ptr,
			   (void *)p_trb, sizeof(struct normal_trb));
		p_next_trb = (struct data_trb *)
						p_xusb_dev_context->bulkin_epenqueue_ptr;
		p_next_trb++;
		/* Handle Link TRB */
		if (p_next_trb->trb_type == LINK_TRB) {
			p_link_trb = (struct link_trb *)p_next_trb;
			p_link_trb->c = p_xusb_dev_context->bulkin_pcs;
			p_link_trb->tc = 1;
			p_next_trb = &p_txringep1in[0];
			p_xusb_dev_context->bulkin_pcs ^= 1;
		}
		dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
			(void *)p_xusb_dev_context->bulkin_epenqueue_ptr,
			sizeof(struct normal_trb), TEGRABL_DMA_TO_DEVICE);

		p_xusb_dev_context->bulkin_epenqueue_ptr = (uintptr_t)p_next_trb;
	} else
		e = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	/* Ring Doorbell */
	if (ring_doorbell) {
		reg_data = NV_DRF_NUM(XUSB_DEV_XHCI,
					DB,
					TARGET,
					ep_index);
		if (ep_index ==  EP0_IN) {
			reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
						DB,
						STREAMID,
						p_xusb_dev_context->cntrl_seq_num,
						reg_data);
		}
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_DB_0, reg_data);
	}
	return e;
}

static tegrabl_error_t tegrabl_issue_status_trb(uint32_t direction)
{
	tegrabl_error_t e;
	struct status_trb strb;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	if (p_xusb_dev_context->cntrl_epenqueue_ptr !=
			p_xusb_dev_context->cntrl_epdequeue_ptr && direction == DIR_IN) {
		return TEGRABL_NO_ERROR;
	}
	memset((void *)&strb, 0, sizeof(struct status_trb));
	tegrabl_create_status_trb(&strb, direction);
	/* Note EP0_IN is bi-directional. */

	e = tegrabl_queue_trb(EP0_IN, (struct normal_trb *)&strb, 1);

	p_xusb_dev_context->wait_for_eventt = STATUS_STAGE_TRB;
	return e;
}

static tegrabl_error_t tegrabl_init_epcontext(enum endpoint ep_index)
{
	struct ep_context *ep_info;
	struct xusb_device_context *p_xusb_dev_context;
	struct link_trb *p_link_trb;
	uint32_t reg_data;
	uint32_t expected_value;
	uint32_t mask;
	dma_addr_t dma_buf;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(mask);
	TEGRABL_UNUSED(expected_value);
	TEGRABL_UNUSED(reg_data);

	if (ep_index > EP1_IN) {
		pr_critical("invalid endpoint en no: 0x%d\n", ep_index);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	p_xusb_dev_context = &s_xusb_device_context;

	if (ep_index == EP0_OUT) {
		ep_index = EP0_IN;
	}

	/* Setting Ep Context */
	ep_info = &p_ep_context[ep_index];

	/* Control Endpoint 0. */
	if (ep_index == EP0_IN) {
		memset((void *)ep_info, 0, sizeof(struct ep_context));
		/* Set Endpoint State to running. */

		ep_info->ep_state = EP_RUNNING;
		/* Set error count to 3 */
		ep_info->cerr = 3;
		/* Set Burst size 0 */
		ep_info->max_burst_size = 0;
		/* Set Packet size as 64 bytes. USB 2.0 */
		/* TODO: Change to 512 if supporting USB 3.0 */

#if defined(CONFIG_ENABLE_XUSBF_SS)
#define EP0_MAX_PACKET_SIZE 512
#else
#define EP0_MAX_PACKET_SIZE 64
#endif

		ep_info->max_packet_size = EP0_MAX_PACKET_SIZE;
		/* Set CCS for controller to 1. Cycle bit should be set to 1. */
		ep_info->dcs = 1;
		/* cerr Count */
		ep_info->cec = 0x3;
		/* Initialize Producer Cycle State to 1. */
		p_xusb_dev_context->cntrl_pcs = 1;
		/* SW copy of Dequeue pointer for control endpoint. */
		p_xusb_dev_context->cntrl_epdequeue_ptr =
			p_xusb_dev_context->cntrl_epenqueue_ptr =
			(uintptr_t)&p_txringep0[0];

		/* EP specific Context
		 Set endpoint type to Control. */
#define EP_TYPE_CNTRL 4
		/* Average TRB length. Setup data always 8 bytes. */
		ep_info->avg_trb_len = 8;
		ep_info->ep_type = EP_TYPE_CNTRL;

		/* Set the dequeue pointer for the consumer (i.e. XUSB Controller) */
		dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
			(void *)&p_txringep0[0], sizeof(struct data_trb),
			TEGRABL_DMA_TO_DEVICE);

		ep_info->trd_dequeueptr_lo = (U64_TO_U32_LO(dma_buf) >> 4);

		ep_info->trd_dequeueptr_hi = U64_TO_U32_HI(dma_buf);

		/* Setup Link TRB. Last TRB of ring. */
		p_link_trb = (struct link_trb *)(uintptr_t)
					&p_txringep0[NUM_TRB_TRANSFER_RING-1];
		p_link_trb->tc = 1;
		p_link_trb->ring_seg_ptrlo = (U64_TO_U32_LO(dma_buf) >> 4);

		p_link_trb->ring_seg_ptrhi = U64_TO_U32_HI(dma_buf);
		p_link_trb->trb_type = LINK_TRB;

	} else {
		if (ep_index == EP1_OUT) {
			memset((void *)ep_info, 0, sizeof(struct ep_context));
			ep_info->ep_state = EP_RUNNING;
			/* Set error count to 3 */
			ep_info->cerr = 3;
			/* Set Burst size 0 */
			ep_info->max_burst_size = 0;
			/* Set Packet size as 64 bytes. USB 2.0
			 Set CCS for controller to 1. Cycle bit should be set to 1. */
			ep_info->dcs = 1;
			/* Set CCS for controller to 1. Cycle bit should be set to 1.
			 cerr Count */
			ep_info->cec = 0x3;

			/* Initialize Producer Cycle State to 1. */
			p_xusb_dev_context->bulkout_pcs = 1;

			/* SW copy of Dequeue pointer for control endpoint. */
			p_xusb_dev_context->bulkout_epdequeue_ptr =
			p_xusb_dev_context->bulkout_epenqueue_ptr =
				(uintptr_t)&p_txringep1out[0];

			/* EP specific Context. Set endpoint type to Bulk. */
#define EP_TYPE_BULK_OUT 2
			/* Average TRB length */
#if defined(CONFIG_ENABLE_XUSBF_SS)
			if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
				ep_info->avg_trb_len = 1024;
				ep_info->max_packet_size = 1024;
			} else /* All other cases, use HS.*/
#endif
			if (p_xusb_dev_context->port_speed == XUSB_HIGH_SPEED) {
				ep_info->avg_trb_len = 512;
				ep_info->max_packet_size = 512;
			} else {
				ep_info->avg_trb_len = 512;
				ep_info->max_packet_size = 64;
			}
			ep_info->ep_type = EP_TYPE_BULK_OUT;

			dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
						(void *)&p_txringep1out[0], sizeof(struct data_trb),
						TEGRABL_DMA_TO_DEVICE);

			ep_info->trd_dequeueptr_lo = (U64_TO_U32_LO(dma_buf) >> 4);
			ep_info->trd_dequeueptr_hi = U64_TO_U32_HI(dma_buf);

			/* Setup Link TRB. Last TRB of ring. */
			p_link_trb = (struct link_trb *)
						&p_txringep1out[NUM_TRB_TRANSFER_RING - 1];
			p_link_trb->tc = 1;
			p_link_trb->ring_seg_ptrlo = (U64_TO_U32_LO(dma_buf) >> 4);

			p_link_trb->ring_seg_ptrhi = U64_TO_U32_HI(dma_buf);
			p_link_trb->trb_type = LINK_TRB;
		} else { /* EP IN */
			memset((void *)ep_info, 0, sizeof(struct ep_context));
			ep_info->ep_state = EP_RUNNING;
			/* Set error count to 3 */
			ep_info->cerr = 3;
			/* Set Burst size 0 */
			ep_info->max_burst_size = 0;
			/* Set Packet size as 64 bytes. USB 2.0
			 * Set CCS for controller to 1. Cycle bit should be set to 1.
			 */
			ep_info->dcs = 1;
			/* cerr Count */
			ep_info->cec = 0x3;

			/* Initialize Producer Cycle State to 1. */
			p_xusb_dev_context->bulkin_pcs  = 1;

			/* SW copy of Dequeue pointer for control endpoint. */
			p_xusb_dev_context->bulkin_epdequeue_ptr =
			p_xusb_dev_context->bulkin_epenqueue_ptr =
					(uintptr_t)&p_txringep1in[0];

			/* EP specific Context
			 * Set endpoint type to Bulk.
			 */

#define EP_TYPE_BULK_IN 6
			ep_info->ep_type = EP_TYPE_BULK_IN;

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
				 (void *)&p_txringep1in[0], sizeof(struct data_trb),
				 TEGRABL_DMA_TO_DEVICE);

			ep_info->trd_dequeueptr_lo = (U64_TO_U32_LO(dma_buf) >> 4);
			ep_info->trd_dequeueptr_hi = U64_TO_U32_HI(dma_buf);

#if defined(CONFIG_ENABLE_XUSBF_SS)
			if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
				ep_info->avg_trb_len = 1024;
				ep_info->max_packet_size = 1024;
			} else /* All other cases, use HS. */
#endif
			if (p_xusb_dev_context->port_speed == XUSB_HIGH_SPEED) {
				ep_info->avg_trb_len = 512;
				ep_info->max_packet_size = 512;
			} else {
				ep_info->avg_trb_len = 512;
				ep_info->max_packet_size = 64;
			}
			/* Setup Link TRB. Last TRB of ring. */
			p_link_trb = (struct link_trb *)
						&p_txringep1in[NUM_TRB_TRANSFER_RING-1];
			p_link_trb->tc = 1;

			p_link_trb->ring_seg_ptrlo = (U64_TO_U32_LO(dma_buf) >> 4);
			p_link_trb->ring_seg_ptrhi = U64_TO_U32_HI(dma_buf);
			p_link_trb->trb_type = LINK_TRB;
		}
	}

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
			(void *)ep_info, sizeof(struct ep_context),
			TEGRABL_DMA_TO_DEVICE);

	if (ep_index == EP0_IN) {
		p_xusb_dev_context->dma_ep_context_start_addr = dma_buf;
	}

	return e;
}

static tegrabl_error_t tegrabl_initep(enum endpoint ep_index, bool reinit)
{
	uint32_t expected_value;
	uint32_t mask;
	uint32_t reg_data;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	e = tegrabl_init_transfer_ring(ep_index);
	if (e != TEGRABL_NO_ERROR) {
		pr_critical("%s:Transfer ring init failed 0x%d\n", __func__, ep_index);
		return e;
	}

	if (ep_index == EP0_IN || ep_index == EP0_OUT) {
		e = tegrabl_init_epcontext(EP0_IN);
		if (e != TEGRABL_NO_ERROR) {
			pr_critical("endpoint init failed ep no: 0x%d\n", ep_index);
			return e;
		}

		if (reinit == true) {
			/* Make sure endpoint is not paused or halted. */
			reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0);
			reg_data &= ~(1UL << ep_index);
			reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0,
								  reg_data);
			reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0);
			reg_data &= ~(1UL << ep_index);
			reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0,
								  reg_data);
		}

	} else if (ep_index == EP1_IN || ep_index == EP1_OUT) {
		e = tegrabl_init_epcontext(ep_index);
		if (e != TEGRABL_NO_ERROR) {
			pr_critical("endpoint init failed ep no: 0x%d\n", ep_index);
			return e;
		}

		if (reinit == false) {
			/* Bit 2 for EP1_OUT , Bit 3 for EP1_IN
			 * Force load context
			 * Steps from device_mode IAS, 5.1.3.1
			 */
			reg_data = NV_DRF_NUM(XUSB_DEV_XHCI,
								EP_RELOAD,
								DCI,
								1 << ep_index);
			NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_RELOAD_0, reg_data);

			mask = 1UL << ep_index;
			expected_value = 0;
			e = tegrabl_poll_field(XUSB_BASE +
								XUSB_DEV_XHCI_EP_RELOAD_0,
								mask,
								expected_value,
								1000);
			if (e != TEGRABL_NO_ERROR) {
				pr_critical("EP init timeout ep index: 0x%d\n", ep_index);
				return e;
			}
		}

		/* Make sure ep is not Npaused or halted. */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0);
		reg_data &= ~(1 << ep_index);
		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0, reg_data);
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0);
		reg_data &= ~(1 << ep_index);
		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0, reg_data);
	} else {
		e = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return e;
}

static tegrabl_error_t tegrabl_set_configuration(uint8_t *psetup_data)
{
	uint16_t wvalue;
	uint32_t reg_data, expected_value, mask;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(e);
	TEGRABL_UNUSED(mask);
	TEGRABL_UNUSED(expected_value);

	/* Last stage of enumeration. */
	wvalue = psetup_data[USB_SETUP_VALUE] +
			(psetup_data[USB_SETUP_VALUE+1] << 8);

	/* If we get a set config 0, then disable endpoints and remain in addressed
	 * state.
	 *  If we had already set a config before this request, then do the same but
	 * also enable bulk endpoints after and set run bit.
	 */

	if (p_xusb_dev_context->config_num || wvalue == 0) {
		tegrabl_disable_ep(EP1_IN);
		tegrabl_disable_ep(EP1_OUT);

		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
					CTRL,
					RUN,
					STOP,
					reg_data);
		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);
	}

	if (wvalue) {
		tegrabl_initep(EP1_OUT, false);
		tegrabl_initep(EP1_IN, false);

		/* Now set run */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
					CTRL,
					RUN,
					RUN,
					reg_data);
		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);
	}

	/* Also clear Run Change bit just in case to enable Doorbell register. */
	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_ST_0);
	reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
				ST,
				RC,
				CLEAR,
				reg_data);
	reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ST_0, reg_data);
	/* Send status */
	tegrabl_issue_status_trb(DIR_IN);
	p_xusb_dev_context->config_num = wvalue;

	/* Change device state only for non-zero configuration number
	 * Otherwise device remains in addressed state.
	 */
	if (wvalue)
		p_xusb_dev_context->device_state = CONFIGURED_STATUS_PENDING;
	else
		p_xusb_dev_context->device_state = ADDRESSED_STATUS_PENDING;
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_set_interface(uint8_t *psetup_data)
{
	uint16_t wvalue;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	wvalue = psetup_data[USB_SETUP_VALUE] +
			(psetup_data[USB_SETUP_VALUE + 1] << 8);

	p_xusb_dev_context->interface_num = wvalue;
	/* Send status */
	tegrabl_issue_status_trb(DIR_IN);
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_set_address(uint8_t *psetup_data)
{
	uint8_t dev_addr;
	uint32_t reg_data;
	struct ep_context *ep_info;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	dev_addr = psetup_data[USB_SETUP_VALUE];
	ep_info = &p_ep_context[EP0_IN];
	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
				CTRL,
				DEVADR,
				dev_addr,
				reg_data);
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);

	ep_info->device_addr = dev_addr;

	/* Send status */
	e = tegrabl_issue_status_trb(DIR_IN);
		if (e != TEGRABL_NO_ERROR) {
			pr_critical("%s Send status failed\n", __func__);
			return e;
		}
	p_xusb_dev_context->device_state = ADDRESSED_STATUS_PENDING;

	return e;
}

static tegrabl_error_t tegrabl_stall_ep(enum endpoint ep_index, bool stall)
{
	uint32_t reg_data, expected_value, int_pending_mask;
	tegrabl_error_t e;
	/* EIndex received from Host is of the form:
	 * Byte 1(Bit7 is dir): Byte 0
	 * 8: In : EpNum
	 * 0: Out: EpNum
	 */

	pr_debug(
					   "Stalling Endpoint Number %d\n", ep_index);
	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0);
	if (stall)
		reg_data |= (1 << ep_index);
	else
		reg_data &= ~(1 << ep_index);

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0, reg_data);
	/* Poll for state change */
	expected_value = 1UL << ep_index;
	int_pending_mask = 1UL << ep_index;

	/* Poll for interrupt pending bit. */
	e = tegrabl_poll_field(XUSB_BASE+XUSB_DEV_XHCI_EP_STCHG_0,
			int_pending_mask,
			expected_value,
			1000);
	if (e != TEGRABL_NO_ERROR) {
		pr_critical("Failed halting endpoint\n");
		return e;
	}
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_STCHG_0, 1 << ep_index);
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_get_desc(uint8_t *psetup_data,
		uint16_t *tx_length, uint8_t *p_setup_buffer)
{
	uint8_t desc_type = 0;
	uint8_t desc_index = 0;
	uint16_t wlength, desc_length;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint8_t *desc = NULL;

	desc_type = psetup_data[USB_SETUP_DESCRIPTOR];
	wlength = *(uint16_t *)&(psetup_data[USB_SETUP_LENGTH]);

	switch (desc_type) {
	case USB_DT_DEVICE:
		if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
			desc = (uint8_t *)g_usbconfig->ss_device.desc;
			desc_length = (uint16_t)g_usbconfig->ss_device.len;
		} else {
			desc = (uint8_t *)g_usbconfig->hs_device.desc;
			desc_length = (uint16_t)g_usbconfig->hs_device.len;
		}

		*tx_length = MIN(wlength, desc_length);
		memcpy((void *)p_setup_buffer, (void *)desc, *tx_length);
		break;

	case USB_DT_CONFIG:
		if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
			desc = (uint8_t *)g_usbconfig->ss_config.desc;
			*tx_length = MIN(wlength, g_usbconfig->ss_config.len);
		} else {
			desc = (uint8_t *)g_usbconfig->hs_config.desc;
			*tx_length = MIN(wlength, g_usbconfig->hs_config.len);
		}
		if (p_xusb_dev_context->port_speed == XUSB_FULL_SPEED) {
		    /* Apply full speed packet size */
			/* EP1_IN */
			desc[22] = 64;
			desc[23] = 0;
			/* EP1_OUT */
			desc[29] = 64;
			desc[30] = 0;
		}
		memcpy((void *)p_setup_buffer, (void *)desc,
			   *tx_length);
		break;

	case USB_DT_STRING:
		desc_index = psetup_data[USB_SETUP_VALUE];
		switch (desc_index) {
		case USB_MANF_ID:
			pr_debug("Get desc. Manf ID\n");
			*tx_length = MIN(wlength, (uint16_t)sizeof(s_usb_manufacturer_id));
			memcpy((void *)p_setup_buffer, (void *)&s_usb_manufacturer_id[0],
				   *tx_length);
			break;
		case USB_PROD_ID:
			pr_debug("Get desc. Prod ID\n");
			desc = (uint8_t *)g_usbconfig->product.desc;
			*tx_length = MIN(wlength, g_usbconfig->product.len);
			memcpy((void *)p_setup_buffer, (void *)desc,
				   *tx_length);
			break;
		case USB_SERIAL_ID:
			pr_debug("Get desc. Serial ID\n");
			desc = (uint8_t *)g_usbconfig->serialno.desc;
			*tx_length = MIN(wlength, g_usbconfig->serialno.len);
			memcpy((void *)p_setup_buffer, (void *)desc,
				   *tx_length);
			break;
		case USB_LANGUAGE_ID:
			pr_debug("Get desc. Lang ID\n");
			*tx_length = MIN(wlength, (uint16_t)sizeof(s_usb_language_id));
			memcpy((void *)p_setup_buffer, (void *)&s_usb_language_id[0],
				   *tx_length);
			break;
		default:
			pr_warn("Get desc. unsupported req:desc_index=%d\n", desc_index);
			break;
		}
		break;

	case USB_DT_DEVICE_QUALIFIER:
		pr_debug("Get desc. Dev qualifier\n");
		*tx_length = MIN(wlength, (uint16_t)sizeof(s_usb_device_qualifier));
		memcpy((void *)p_setup_buffer, (void *)&s_usb_device_qualifier[0],
			   *tx_length);
		break;
	case USB_DT_OTHER_SPEED_CONFIG:
		if (p_xusb_dev_context->port_speed == XUSB_HIGH_SPEED) {
			/* Full speed packet size as other speed. */
			/* EP1_IN */
			s_other_speed_config_desc[22] = 64;
			s_other_speed_config_desc[23] = 0;
			/* EP1_OUT */
			s_other_speed_config_desc[29] = 64;
			s_other_speed_config_desc[30] = 0;
		} else {
			/* High speed packet size as other speed. */
			/* EP1_IN */
			s_other_speed_config_desc[22] = 0;
			s_other_speed_config_desc[23] = 2;
			/* EP1_OUT */
			s_other_speed_config_desc[29] = 0;
			s_other_speed_config_desc[30] = 2;
		}
		*tx_length = MIN(wlength, (uint16_t)sizeof(s_other_speed_config_desc));
		memcpy((void *)p_setup_buffer, (void *)&s_other_speed_config_desc[0],
			   *tx_length);
		break;
	case USB_DT_BOS:
		pr_debug("Get BOS\n");
		*tx_length = MIN(wlength, sizeof(s_bos_descriptor));
		memcpy((void *)p_setup_buffer, (void *)&s_bos_descriptor[0],
			   *tx_length);
		break;
	default:
		pr_warn("Unsupported req: desc_index=%d\n", desc_index);
		/* stall if any Un supported request comes */
		tegrabl_stall_ep(EP0_IN, true);
		break;
	}
	return e;
}

static tegrabl_error_t tegrabl_ep_getstatus(uint16_t ep_index,
		uint16_t *tx_length, uint8_t *p_setup_buffer)
{
	uint32_t ep_status;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	/* ep_index received from Host is of the form:
	* Byte 1(Bit7 is dir): Byte 0
	* 8: In : EpNum
	* 0: Out: EpNum
	*/

	pr_debug("Ep num = %d\n", (uint16_t)ep_index);
	ep_status  = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0) >> ep_index;

	if (ep_status == 1)
		endpoint_status[0] = 1;
	else
		endpoint_status[0] = 0;

	*tx_length = (uint16_t)sizeof(endpoint_status);
	memcpy((void *)p_setup_buffer, (void *)&endpoint_status[0],
		   sizeof(endpoint_status));
	return e;
}

static void tegrabl_create_data_trb(struct data_trb *p_data_trb,
			dma_addr_t buffer, uint32_t bytes, uint32_t dir)
{
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	p_data_trb->databufptr_lo = U64_TO_U32_LO(buffer);
	p_data_trb->databufptr_hi = U64_TO_U32_HI(buffer);
	p_data_trb->trb_tx_len = bytes;
	/* BL will always queue only 1 TRB at a time. */
	p_data_trb->tdsize = 0;
	p_data_trb->c = p_xusb_dev_context->cntrl_pcs;
	p_data_trb->ent = 0;
	/* Make sure to interrupt on short packet i.e generate event. */
	p_data_trb->isp = 1;
	/* and on Completion. */
	p_data_trb->ioc = 1;

	p_data_trb->trb_type = DATA_STAGE_TRB;
	p_data_trb->dir = dir;
}

static tegrabl_error_t tegrabl_issue_data_trb(dma_addr_t buffer,
		uint32_t bytes, uint32_t direction)
{
	struct data_trb dtrb;
	tegrabl_error_t e;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	/* Need to check if empty other wise don't issue
	 * Will result in Seq Num Error
	 */
	if (p_xusb_dev_context->cntrl_epenqueue_ptr !=
			p_xusb_dev_context->cntrl_epdequeue_ptr) {
		return TEGRABL_NO_ERROR;
	}

	memset((void *)&dtrb, 0, sizeof(struct data_trb));
	tegrabl_create_data_trb(&dtrb, buffer, bytes, direction);

	/* Note EP0_IN is bi-directional. */
	e = tegrabl_queue_trb(EP0_IN, (struct normal_trb *)&dtrb, 1);
	if (e != TEGRABL_NO_ERROR) {
		return e;
	}

	p_xusb_dev_context->wait_for_eventt = DATA_STAGE_TRB;
	return e;
}

static tegrabl_error_t tegrabl_handle_setuppkt(uint8_t *psetup_data)
{
	uint32_t reg_data, int_pending_mask, expected_value;
	uint16_t wlength, tx_length = 0;
	tegrabl_error_t e;
	uint8_t ep_index;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t dma_buf;
	uint8_t tdata[6];

	wlength = *(uint16_t *)&psetup_data[USB_SETUP_LENGTH];

	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0);
	reg_data &= ~(1 << EP0_IN);
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0, reg_data);

	expected_value = 0;
	int_pending_mask = 1;
	/* Poll for interrupt pending bit. */
	e = tegrabl_poll_field((XUSB_BASE + XUSB_DEV_XHCI_EP_HALT_0),
										int_pending_mask,
										expected_value,
										1000);

		if (e != TEGRABL_NO_ERROR) {
			pr_critical("%s Handle setup packet failed\n", __func__);
			return e;
		}
	switch (psetup_data[USB_SETUP_REQUEST_TYPE]) {
	case HOST2DEV_DEVICE:
		/* Process the Host -> device Device descriptor */
		switch (psetup_data[USB_SETUP_REQUEST]) {
		case SET_CONFIGURATION:
			e = tegrabl_set_configuration(psetup_data);
			if (e != TEGRABL_NO_ERROR) {
				pr_critical("%s Sent configuration failed\n", __func__);
				return e;
			}
			break;
		case SET_ADDRESS:
			e = tegrabl_set_address(psetup_data);
			if (e != TEGRABL_NO_ERROR) {
				pr_critical(
								   "%s Set address failed\n", __func__);
				return e;
			}
			break;
		case SET_ISOCH_DELAY:
			/* Read timing values and store them */
			/* Send status */
			e = tegrabl_issue_status_trb(DIR_IN);
			if (e != TEGRABL_NO_ERROR) {
				pr_critical("%s set_iscoh delay failed\n", __func__);
				return e;
			}
			break;
		case SET_SEL:
		/* Data stage for receiving 6 bytes */
		e = tegrabl_issue_data_trb(
				(dma_addr_t)(uintptr_t)tdata,
				6,
				DIR_OUT);
		if (e != TEGRABL_NO_ERROR)
			return e;

			/* Send status */
			e = tegrabl_issue_status_trb(DIR_IN);
			if (e != TEGRABL_NO_ERROR) {
				pr_critical("%s: set_sel delay failed\n", __func__);
				return e;
			}
			break;
		case SET_FEATURE:
			pr_debug("SET_FEATURE: value=%d\n", psetup_data[USB_SETUP_VALUE]);

			reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTPM_0);
			if (psetup_data[USB_SETUP_VALUE] == U1_ENABLE) {
					reg_data |=  (1 << 28);
			} else if (psetup_data[USB_SETUP_VALUE] == U2_ENABLE) {
				reg_data |=  (1 << 29);
			}

			NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTPM_0, reg_data);
			/* Send status */
			tegrabl_issue_status_trb(DIR_IN);
			break;
		case CLEAR_FEATURE:
			pr_debug("CLEAR_FEATURE: value=%d\n", psetup_data[USB_SETUP_VALUE]);

			reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTPM_0);
			if (psetup_data[USB_SETUP_VALUE] == U1_ENABLE) {
					reg_data &=  ~(1 << 28);
			} else if (psetup_data[USB_SETUP_VALUE] == U2_ENABLE) {
				reg_data &=  ~(1 << 29);
			}
			NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTPM_0, reg_data);
			/* Send status */
			tegrabl_issue_status_trb(DIR_IN);
			break;

		default:
			pr_debug("HOST2DEV_DEV: unhandled req=%d\n",
					psetup_data[USB_SETUP_REQUEST]);
			break;
		}
		break;

	case HOST2DEV_INTERFACE:
		/* Start the endpoint for zero packet acknowledgment
		 * Store the interface number.
		 */
		tegrabl_set_interface(psetup_data);
		break;

	case DEV2HOST_DEVICE:
		switch (psetup_data[USB_SETUP_REQUEST]) {
		case GET_STATUS:
			pr_debug("Get status\n");
			tx_length = MIN(wlength, (uint16_t)sizeof(s_usb_dev_status));
			memcpy((void *)p_setup_buffer, (void *)&s_usb_dev_status[0],
				   tx_length);
			break;
		case GET_CONFIGURATION:
			pr_debug("Get Config\n");
			tx_length = MIN(wlength,
							(uint16_t)sizeof(p_xusb_dev_context->config_num));
			memcpy((void *)p_setup_buffer, &p_xusb_dev_context->config_num,
				   tx_length);
			break;
		case GET_DESCRIPTOR:
			pr_debug("Get Desc\n");
			/* Get Descriptor Request
			 *  TODO Enact Stall protocol on invalid requests.
			 */
			e =  tegrabl_get_desc(psetup_data, &tx_length, p_setup_buffer);
			if (e != TEGRABL_NO_ERROR) {
				return e;
			}
			break;
		default:
			pr_warn(
							   "Unsupported D2H_D request\n");
			/* Stall if any Un supported request comes */
			tegrabl_stall_ep(EP0_IN, true);
			break;
		}
		break;

	case DEV2HOST_INTERFACE:
		switch (psetup_data[USB_SETUP_REQUEST]) {
		case GET_STATUS:
			/* Just sending 0s. */
			tx_length = MIN(wlength, sizeof(interface_status));
			memcpy((void *)p_setup_buffer, &interface_status[0], tx_length);
			break;
		case GET_INTERFACE:
			/* Just sending 0s. */
			pr_debug("Get Interface D2H_I/F\n");
			tx_length =
				MIN(wlength,
					(uint16_t)sizeof(p_xusb_dev_context->interface_num));
			memcpy((void *)p_setup_buffer, &p_xusb_dev_context->interface_num,
				   tx_length);
			break;
		default:
			/* Stall if any unsupported request comes */
			tegrabl_stall_ep(EP0_IN, true);
			break;
		}
		break;
	/* Stall here, as we don't support endpoint requests here */
	case DEV2HOST_ENDPOINT:
		switch (psetup_data[USB_SETUP_REQUEST]) {
		case GET_STATUS:
			pr_debug("__Get status D2H_Ep\n");
			ep_index = 2*(psetup_data[USB_SETUP_INDEX]&0xF);
			ep_index += (psetup_data[USB_SETUP_INDEX]&0x80) ? 1 : 0;
			e = tegrabl_ep_getstatus(ep_index,
			&tx_length,
			p_setup_buffer);
			if (e != TEGRABL_NO_ERROR) {
				return e;
			}
			break;
		default:
			pr_warn("__Unsupported D2H_Ep\n");
			tegrabl_stall_ep(EP0_IN, true);
			break;
		}
		break;

	case HOST2DEV_ENDPOINT:
		switch (psetup_data[USB_SETUP_REQUEST]) {
		case SET_FEATURE:
			pr_debug("Set Feature H2D_Ep\n");
			switch (psetup_data[USB_SETUP_VALUE]) {
			case ENDPOINT_HALT:

				ep_index = 2 * (psetup_data[USB_SETUP_INDEX]&0xF);
				ep_index += (psetup_data[USB_SETUP_INDEX]&0x80) ? 1 : 0;
				tegrabl_stall_ep(ep_index, true);
				/* Send status */
				tegrabl_issue_status_trb(DIR_IN);
				break;
			default:
				tegrabl_stall_ep(EP0_IN, true);
				break;
			}
			break;
		case CLEAR_FEATURE:
			switch (psetup_data[USB_SETUP_VALUE]) {
			case ENDPOINT_HALT:
				/* Get the EP status, to find wether Txfer is success or not */
				ep_index = 2 * (psetup_data[USB_SETUP_INDEX] & 0xF);
				ep_index += (psetup_data[USB_SETUP_INDEX] & 0x80) ? 1 : 0;
				tegrabl_stall_ep(ep_index, false);
				/* Send status */
				tegrabl_issue_status_trb(DIR_IN);
				break;
			default:
				tegrabl_stall_ep(EP0_IN, true);
				break;
			}
			break;
		default:
			pr_warn("Unsupported H2D_Ep\n");
			/* Stall if any unsupported request comes */
			tegrabl_stall_ep(EP0_IN, true);
			break;
		}
		break;
	default:
		pr_warn("Unsupported setup req = %d\n", psetup_data[USB_SETUP_REQUEST]);
		/* Stall if any Un supported request comes */
		tegrabl_stall_ep(EP0_IN, true);
		break;
	}

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
				(void *)p_setup_buffer, tx_length, TEGRABL_DMA_TO_DEVICE);

	if (tx_length) {
		/* Compensate buffer for xusb device view of sysram */
		e = tegrabl_issue_data_trb(
				dma_buf,
				tx_length,
				DIR_IN);
		if (e != TEGRABL_NO_ERROR)
			return e;
	}

	return e;
}

static tegrabl_error_t tegrabl_handle_port_status(void)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t mask, expected_value, port_speed, reg_halt, status_bits_mask = 0;
	uint32_t reg_data, link_state;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	uint32_t port_status;

	TEGRABL_UNUSED(expected_value);
	TEGRABL_UNUSED(mask);

	/* Let's see why we got here. */
	port_status = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0);
	reg_halt = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0);

	status_bits_mask = NV_DRF_NUM(XUSB_DEV_XHCI, PORTSC, PRC, 1) |
		NV_DRF_NUM(XUSB_DEV_XHCI, PORTSC, PLC, 1) |
		NV_DRF_NUM(XUSB_DEV_XHCI, PORTSC, WRC, 1) |
		NV_DRF_NUM(XUSB_DEV_XHCI, PORTSC, CSC, 1) |
		NV_DRF_NUM(XUSB_DEV_XHCI, PORTSC, CEC, 1);

	/* Handle PORT RESET. PR indicates reset event received.
	 * PR could get cleared (port reset complete) by the time we read it
	 * so check on PRC.
	 *  See device mode IAS 5.1.4.5
	 */
	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, PR, port_status)) {
		/* This is probably a good time to stop the watchdog timer. */
		p_xusb_dev_context->device_state = RESET;
	}
	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, PRC, port_status)) {
		/* Must clear PRC */
		port_status &= (~status_bits_mask);
		port_status = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
						PORTSC,
						PRC,
						1,
						port_status);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0, port_status);
	}

	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, WPR, port_status)) {
		/* This is probably a good time to stop the watchdog timer. */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					PORTHALT,
					HALT_LTSSM,
					0,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0, reg_data);
		p_xusb_dev_context->device_state = RESET;
	}
	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, WRC, port_status)) {
		port_status &= ~status_bits_mask;
		port_status = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
						PORTSC,
						WRC,
						1,
						port_status);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0, port_status);
	}

	port_status = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0);
	/* Connect status Change and Current Connect status should be 1
	 *  to indicate successful connection to downstream port.
	 */
	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, CSC, port_status)) {
		if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, CCS, port_status) == 1) {
			p_xusb_dev_context->device_state = CONNECTED;
			port_speed = NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, PS, port_status);
			p_xusb_dev_context->port_speed = port_speed;

#if defined(CONFIG_ENABLE_XUSBF_SS)
			/* Reload Endpoint Context if not connected in superspeed
			 * after changing packet size.
			 */
			p_ep_context = &p_ep_context[EP0_IN];
			if (p_xusb_dev_context->port_speed != XUSB_SUPER_SPEED) {
				p_ep_context->max_packet_size = 64;
			} else {
				p_ep_context->max_packet_size = 512;
			}
			tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
					(void *)p_ep_context, sizeof(struct ep_context),
					TEGRABL_DMA_TO_DEVICE);
#endif
		} else {
			/* This will never happen because Vbus is overriden to 1.
			 *  if CCS=0, somebody pulled the plug.
			 */
			p_xusb_dev_context->device_state = DISCONNECTED;
		}
		port_status &= ~status_bits_mask;
		port_status = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
						PORTSC,
						CSC,
						1,
						port_status);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0, port_status);
	}

	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTHALT, STCHG_REQ, reg_halt)) {
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					PORTHALT,
					HALT_LTSSM,
					0,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0, reg_data);
	}

	port_status = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0);
	/* Port Link status Change */
	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, PLC, port_status)) {
		pr_debug("__PLC = 0x%x\n", port_status);
		port_status &= ~status_bits_mask;
		link_state = NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, PLS, port_status);
		/* U3 or Suspend */
		if (link_state == 0x3) {
			p_xusb_dev_context->device_state = SUSPENDED;
		} else if (link_state == 0x0 &&
				(p_xusb_dev_context->device_state == SUSPENDED)) {
			p_xusb_dev_context->device_state = CONFIGURED;
			reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0);
			NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0, 0);
			NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_STCHG_0, reg_data);
		} else {
			/* No Action Required */
		}

		port_status = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
						PORTSC,
						PLC,
						1,
						port_status);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0, port_status);
	}
	/* Config Error Change */
	if (NV_DRF_VAL(XUSB_DEV_XHCI, PORTSC, CEC, port_status)) {
		pr_debug("__Error\n");
		port_status = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0);
		port_status &= ~status_bits_mask;
		port_status = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
						PORTSC,
						CEC,
						1,
						port_status);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0, port_status);
		e = TEGRABL_ERROR(TEGRABL_ERR_PORT_ERROR, 0);
	}
	return e;
}

static tegrabl_error_t tegrabl_handle_txfer_event(
		struct transfer_event_trb *p_tx_eventrb)
{
	/* Wait for event to be posted */
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	struct link_trb *p_link_trb;
	struct data_trb *p_next_trb;

	TEGRABL_UNUSED(p_link_trb);

	/* Make sure update local copy for dequeue ptr */
	if (p_tx_eventrb->emp_id == EP0_IN) {
		p_xusb_dev_context->cntrl_epdequeue_ptr +=
						sizeof(struct transfer_event_trb);
		p_next_trb = (struct data_trb *)p_xusb_dev_context->cntrl_epdequeue_ptr;
		/* Handle Link TRB */
		if (p_next_trb->trb_type == LINK_TRB) {
			p_link_trb = (struct link_trb *)p_next_trb;
			p_next_trb = &p_txringep0[0];
		}
	/* TODO Add check for full ring */
		p_xusb_dev_context->cntrl_epdequeue_ptr = (uintptr_t)p_next_trb;
	}
	if (p_tx_eventrb->emp_id == EP1_OUT) {
		p_xusb_dev_context->bulkout_epdequeue_ptr +=
				sizeof(struct transfer_event_trb);
		p_next_trb = (struct data_trb *)
						p_xusb_dev_context->bulkout_epdequeue_ptr;
		/* Handle Link TRB */
		if (p_next_trb->trb_type == LINK_TRB) {
			p_link_trb = (struct link_trb *)p_next_trb;
			p_next_trb = &p_txringep1out[0];
		}
		/* TODO Add check for full ring */
		p_xusb_dev_context->bulkout_epdequeue_ptr = (uintptr_t)p_next_trb;
	}
	if (p_tx_eventrb->emp_id == EP1_IN) {
		p_xusb_dev_context->bulkin_epdequeue_ptr +=
				sizeof(struct transfer_event_trb);
		p_next_trb = (struct data_trb *)
					p_xusb_dev_context->bulkin_epdequeue_ptr;
		/* Handle Link TRB */
		if (p_next_trb->trb_type == LINK_TRB) {
			p_link_trb = (struct link_trb *)p_next_trb;
			p_next_trb = &p_txringep1in[0];
		}
		p_xusb_dev_context->bulkin_epdequeue_ptr = (uintptr_t)p_next_trb;
	}

	/* Check for errors. */
	if ((p_tx_eventrb->comp_code == SUCCESS_ERR_CODE) ||
		(p_tx_eventrb->comp_code == SHORT_PKT_ERR_CODE)) {
		if (p_tx_eventrb->emp_id == EP0_IN) {
			if (p_xusb_dev_context->wait_for_eventt == DATA_STAGE_TRB) {
				/* Send status */
				tegrabl_issue_status_trb(DIR_OUT);
			} else if (p_xusb_dev_context->wait_for_eventt ==
				STATUS_STAGE_TRB) {
				if (p_xusb_dev_context->device_state ==
										ADDRESSED_STATUS_PENDING) {
					p_xusb_dev_context->device_state = ADDRESSED;
				}
				if (p_xusb_dev_context->device_state ==
										CONFIGURED_STATUS_PENDING) {
					p_xusb_dev_context->device_state = CONFIGURED;
				}
			} else {
				/* No Action Required */
			}
		}
		if (p_tx_eventrb->emp_id == EP1_IN) {
			/* TRB Tx Len will be 0 or remaining bytes. */
			p_xusb_dev_context->bytes_txfred -= p_tx_eventrb->trb_tx_len;
			p_xusb_dev_context->tx_count--;
			/* For IN, we should not have remaining bytes. Flag error */
			if (p_tx_eventrb->trb_tx_len) {
				pr_critical(
								   "%s data under flow error\n", __func__);
				return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			}
		}
		if (p_tx_eventrb->emp_id == EP1_OUT) {
			/* TRB Tx Len will be 0 or remaining bytes for short packet. */
			p_xusb_dev_context->bytes_txfred -= p_tx_eventrb->trb_tx_len;
			p_xusb_dev_context->tx_count--;
			/* Short packet is not necessary an error
			* because we prime for 4K bytes. */
		}
		/* This should be zero except in the case of a short packet. */
	} else if (p_tx_eventrb->comp_code == CTRL_DIR_ERR_CODE) {
		pr_critical("%s control dir error\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	} else if (p_tx_eventrb->comp_code == CTRL_SEQ_NUM_ERR_CODE) {
		/* TODO:
		* This could mean 2 things.
		* 1. The seq number in the data/status stage did not match the
		*      setup event seq.
		* 2. A new setup packet was received when sending data/status packet.

		* We have a setup packet to process
		* Setuppacketindex points to next slot. pop out last setup packet.
		*/
		pr_critical("%s, %d Send status failed\n", __func__, __LINE__);
		e = tegrabl_handle_setuppkt(&setup_data[0]);
		return e;
	} else {
		pr_critical("%s transfer failed\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return e;
}

static tegrabl_error_t tegrabl_poll_for_event(uint32_t timeout)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t reg_data, expected_value, int_pending_mask;
	struct event_trb *p_event_trb;
	uint32_t trb_index;
	struct setup_event_trb *p_setup_event_trb;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t tmp_dma_addr;
	dma_addr_t er_dma_start_address;

	p_xusb_dev_context = &s_xusb_device_context;
	expected_value = 1 << SHIFT(XUSB_DEV_XHCI, ST, IP);
	int_pending_mask = SHIFTMASK(XUSB_DEV_XHCI, ST, IP);

	/* Poll for interrupt pending bit. */
	e = tegrabl_poll_field((XUSB_BASE+XUSB_DEV_XHCI_ST_0),
				int_pending_mask,
				expected_value,
				timeout);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("%s:%d poll for event failed\n", __func__, __LINE__);
		return e;
	}

	reg_data = NV_READ32(XUSB_BASE+XUSB_DEV_XHCI_ST_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
				ST,
				IP,
				1,
				reg_data);
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ST_0, reg_data);

	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EREPLO_0);

	reg_data &= SHIFTMASK(XUSB_DEV_XHCI, EREPLO, ADDRLO);

	tmp_dma_addr = U64_FROM_U32(reg_data,
					NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EREPHI_0));

	er_dma_start_address = p_xusb_dev_context->dma_er_start_address;
	trb_index = ((uint32_t)(tmp_dma_addr - er_dma_start_address) /
													  sizeof(struct event_trb));

	p_xusb_dev_context->event_enqueue_ptr = (uintptr_t)
					((struct event_trb *)&p_event_ring[0] + trb_index);

	p_event_trb = (struct event_trb *)p_xusb_dev_context->event_dequeue_ptr;

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSBF, 0, (void *)p_event_trb,
							 sizeof(struct event_trb), TEGRABL_DMA_FROM_DEVICE);

	/* Make sure cycle state matches */
	/* TODO :Need to find suitable error value */
	if (p_event_trb->c !=  p_xusb_dev_context->event_ccs) {
		pr_error("%s:%d cycle state mismatch\n", __func__, __LINE__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_STATE, 0);
	}

	while (p_event_trb->c ==  p_xusb_dev_context->event_ccs) {
		if (p_event_trb->trb_type == SETUP_EVENT_TRB) {
			pr_debug("setup pkt\n");
			/* Check if we are waiting for setup packet */
			p_setup_event_trb = (struct setup_event_trb *)p_event_trb;
			memcpy((void *)&setup_data[0],
				   (void *)&p_setup_event_trb->data[0], 8);
			p_xusb_dev_context->cntrl_seq_num = p_setup_event_trb->ctrl_seq_num;
			e = tegrabl_handle_setuppkt(&setup_data[0]);
		} else if (p_event_trb->trb_type == PORT_STATUS_CHANGE_TRB) {
			/* Handle all port status changes here. */
			e = tegrabl_handle_port_status();
		} else if (p_event_trb->trb_type == TRANSFER_EVENT_TRB) {
			/* Handle tx event changes here. */
			e = tegrabl_handle_txfer_event(
					(struct transfer_event_trb *)p_event_trb);
		} else {
			/* No Action required */
		}

		/* Increment Event Dequeue Ptr.
		 * Check if last element of ring to wrap around and toggle cycle bit.
		 */
		if (p_xusb_dev_context->event_dequeue_ptr ==
				(uintptr_t)&p_event_ring[NUM_TRB_EVENT_RING - 1]) {
			p_xusb_dev_context->event_dequeue_ptr =
					(uintptr_t)&p_event_ring[0];
			p_xusb_dev_context->event_ccs ^= 1;
		} else {
			p_xusb_dev_context->event_dequeue_ptr += sizeof(struct event_trb);
		}
		p_event_trb = (struct event_trb *)
						p_xusb_dev_context->event_dequeue_ptr;

		/* Process only events posted when interrupt was triggered.
		 * New posted events will be handled during the next
		 * interrupt handler call.
		 */
		if (p_xusb_dev_context->event_dequeue_ptr ==
				p_xusb_dev_context->event_enqueue_ptr) {
			break;
		}
	}

	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_ERDPLO_0);
	/* Clear Event Handler Busy bit */
	if (NV_DRF_VAL(XUSB_DEV_XHCI, ERDPLO, EHB, reg_data)) {
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					ERDPLO,
					EHB,
					1,
					reg_data);
	}

	trb_index = ((struct event_trb *)p_xusb_dev_context->event_dequeue_ptr -
					(struct event_trb *)&p_event_ring[0]);

	tmp_dma_addr = (p_xusb_dev_context->dma_er_start_address +
					(trb_index * sizeof(struct event_trb)));

	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
			ERDPLO,
			ADDRLO,
			(U64_TO_U32_LO(tmp_dma_addr) >> 4),
			reg_data);

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERDPLO_0, reg_data);

	/* Set bits 63:32 of Dequeue pointer. */
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ERDPHI_0, U64_TO_U32_HI(tmp_dma_addr));

	return e;
}

static tegrabl_error_t tegrabl_create_normal_trb(
	struct normal_trb *p_normal_trb, dma_addr_t buffer,
	uint32_t bytes, uint32_t dir)
{
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	p_normal_trb->databufptr_lo = U64_TO_U32_LO(buffer);
	p_normal_trb->databufptr_hi = U64_TO_U32_HI(buffer);
	p_normal_trb->trb_tx_len = bytes;

	/* Number of packets remaining.
	 * BL will always queue only 1 TRB at a time.
	 */
	p_normal_trb->tdsize = 0;
	if (dir == DIR_IN)
		p_normal_trb->c = p_xusb_dev_context->bulkin_pcs;
	else
		p_normal_trb->c = p_xusb_dev_context->bulkout_pcs;

	p_normal_trb->ent = 0;
	/* Make sure to interrupt on short packet i.e generate event. */
	p_normal_trb->isp = 1;
	/* and on Completion. */
	p_normal_trb->ioc = 1;

	p_normal_trb->trb_type = NORMAL_TRB;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_issue_normal_trb(dma_addr_t buffer,
			uint32_t bytes, uint32_t direction)
{
	struct normal_trb normal_trb;
	tegrabl_error_t e;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	enum endpoint ep_index;

	memset((void *)&normal_trb, 0, sizeof(struct normal_trb));
	e = tegrabl_create_normal_trb(&normal_trb, buffer, bytes, direction);
	if (e != TEGRABL_NO_ERROR) {
		return e;
	}
	ep_index = (direction == DIR_IN) ? EP1_IN : EP1_OUT;

	e = tegrabl_queue_trb(ep_index, &normal_trb, 1);
	if	(e != TEGRABL_NO_ERROR) {
		return e;
	}

	p_xusb_dev_context->wait_for_eventt = NORMAL_TRB;

	return e;
}

tegrabl_error_t tegrabl_usbf_enumerate(uint8_t *buffer)
{
	tegrabl_error_t e;
	uint32_t reg_data;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	static uint32_t do_once;

	(void)buffer;

	if (do_once == 0U) {
		/* Make interrupt moderation =0 to avoid delay between back
		 * 2 back interrrupts.
		 */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_RT_IMOD_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					RT_IMOD,
					IMODI,
					0,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_RT_IMOD_0, reg_data);

		/* Set ELPG=0 */
		reg_data = 0;
		NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
				XUSB_PADCTL_ELPG_PROGRAM_0_0, reg_data);
		NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
				XUSB_PADCTL_ELPG_PROGRAM_1_0, reg_data);

		/* Set sw override to vbus */
		reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_VBUS_ID_0);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL,
				USB2_VBUS_ID, VBUS_SOURCE_SELECT, VBUS_OVERRIDE, reg_data);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_PADCTL,
					USB2_VBUS_ID, ID_SOURCE_SELECT, ID_OVERRIDE, reg_data);
		NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
				XUSB_PADCTL_USB2_VBUS_ID_0, reg_data);

		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					PORTHALT,
					HALT_LTSSM,
					0,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0, reg_data);

		/* Write Enable for device mode. */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					CTRL,
					ENABLE,
					1,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);

		/* Force port reg */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CFG_DEV_FE_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					CFG_DEV_FE,
					PORTREGSEL,
					2,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CFG_DEV_FE_0, reg_data);

		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					PORTSC,
					LWS,
					1,
					reg_data);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					PORTSC,
					PLS,
					5, /* RxDetect */
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTSC_0, reg_data);

		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CFG_DEV_FE_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					CFG_DEV_FE,
					PORTREGSEL,
					0,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CFG_DEV_FE_0, reg_data);

		/* Bug 1645744. No need of the chirp timer */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_HSFSPI_COUNT16_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
					HSFSPI_COUNT16,
					CHIRP_FAIL,
					0,
					reg_data);
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_HSFSPI_COUNT16_0, reg_data);

		/* Bug 200153856. Change init value of "XUSB_DEV_XHCI_HSFSPI_COUNT0"
		 * register from 0x12c to 0x3E8. This counter is used by xUSB
		 * device to respond to HS detection handshake after the
		 * detection of SE0 from host.
		 */
		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_HSFSPI_COUNT0_0, 0x3E8);

		/* Set sw override to vbus
		 * ID sourced through VPIO is 0 which indicates otg_host.
		 *  Override ID bit alone to 1. Bug 1383185
		 */
		reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_VBUS_ID_0);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
				USB2_VBUS_ID, VBUS_OVERRIDE, 1, reg_data);
		reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
				USB2_VBUS_ID, ID_OVERRIDE, (1 << 3), reg_data);
		NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
			XUSB_PADCTL_USB2_VBUS_ID_0, reg_data);

#if defined(CONFIG_ENABLE_FPGA)
		/* To Unplug- remove vbus */
		NV_WRITE32(NV_ADDRESS_MAP_XUSB_DEV_BASE+0x8200, 0x24000);
		tegrabl_udelay(1000000);
		/* Override Vbus in device side. */
		NV_WRITE32(NV_ADDRESS_MAP_XUSB_DEV_BASE+0x8200, 0x34000);
#endif
		p_xusb_dev_context->device_state = DEFAULT;
		p_xusb_dev_context->wait_for_eventt = SETUP_EVENT_TRB;
		do_once++;
	}

	while (p_xusb_dev_context->device_state != CONFIGURED) {
		e = tegrabl_poll_for_event(0xFFFFFFFF);
		if (e != TEGRABL_NO_ERROR) {
			pr_warn("FAILED IN WAIT FOR EVENT err = 0x%x\n", e);
			return e;
		}
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_usbf_receive(uint8_t *buffer, uint32_t bytes,
		uint32_t *bytes_received)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t direction;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t dma_buf;

	if ((buffer == NULL) || (bytes_received == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	p_xusb_dev_context->bytes_txfred = bytes;
	p_xusb_dev_context->tx_count = 0;
	direction = DIR_OUT;

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
				(void *)buffer, bytes, TEGRABL_DMA_FROM_DEVICE);

	/* Handle difference in sysram view between host and device. */
	tegrabl_issue_normal_trb(dma_buf, bytes, direction);
	p_xusb_dev_context->tx_count++;
	while (p_xusb_dev_context->tx_count) {
		e = tegrabl_poll_for_event(0xFFFFFFFF);
		if (e != TEGRABL_NO_ERROR) {
			break;
		}
	}
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSBF, 0,
				(void *)buffer, bytes, TEGRABL_DMA_FROM_DEVICE);

	*bytes_received = p_xusb_dev_context->bytes_txfred;

	return e;
}

tegrabl_error_t tegrabl_usbf_transmit(uint8_t *buffer, uint32_t bytes,
		uint32_t *bytes_transmitted)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t direction;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t dma_buf;

	if ((buffer == NULL) || (bytes_transmitted == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	p_xusb_dev_context->bytes_txfred = bytes;
	p_xusb_dev_context->tx_count = 0;
	direction = DIR_IN;

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
					(void *)buffer, bytes, TEGRABL_DMA_TO_DEVICE);

	/* Handle difference in sysram view between host and device. */
	tegrabl_issue_normal_trb(dma_buf, bytes, direction);
	p_xusb_dev_context->tx_count++;

	while (p_xusb_dev_context->tx_count) {
		e = tegrabl_poll_for_event(0xFFFFFFFF);
		if (e != TEGRABL_NO_ERROR) {
			break;
		}
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSBF, 0,
					(void *)buffer, bytes, TEGRABL_DMA_TO_DEVICE);

	*bytes_transmitted = p_xusb_dev_context->bytes_txfred;

	return e;
}

tegrabl_error_t tegrabl_usbf_receive_start(uint8_t *buffer, uint32_t bytes)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t direction;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t dma_buf;

	if (buffer == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	p_xusb_dev_context->bytes_txfred = bytes;
	p_xusb_dev_context->tx_count = 0;
	direction = DIR_OUT;

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
					(void *)buffer, bytes, TEGRABL_DMA_TO_DEVICE);

	/* Handle difference in sysram view between host and device. */
	e = tegrabl_issue_normal_trb(dma_buf, bytes, direction);
	p_xusb_dev_context->tx_count++;

	return e;
}

tegrabl_error_t tegrabl_usbf_receive_complete(uint32_t *bytes_received,
		uint32_t timeout_us)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t dma_buf;

	TEGRABL_UNUSED(dma_buf);

	if (bytes_received == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	while (p_xusb_dev_context->tx_count) {
		e = tegrabl_poll_for_event(timeout_us);
		if (e != TEGRABL_NO_ERROR) {
			break;
		}
	}
	*bytes_received = p_xusb_dev_context->bytes_txfred;

	return e;
}

tegrabl_error_t tegrabl_usbf_transmit_start(uint8_t *buffer, uint32_t bytes)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t direction;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;
	dma_addr_t dma_buf;

	if (buffer == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	p_xusb_dev_context->bytes_txfred = bytes;
	p_xusb_dev_context->tx_count = 0;
	direction = DIR_IN;

	dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
					(void *)buffer, bytes, TEGRABL_DMA_TO_DEVICE);

	/* Handle difference in sysram view between host and device. */
	tegrabl_issue_normal_trb(dma_buf, bytes, direction);
	p_xusb_dev_context->tx_count++;

	return e;
}

tegrabl_error_t tegrabl_usbf_transmit_complete(uint32_t *p_bytes_transferred,
											   uint32_t timeout)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

	(void)timeout;

	if (p_bytes_transferred == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	while (p_xusb_dev_context->tx_count) {
		e = tegrabl_poll_for_event(0xFFFFFFFF);
		if (e != TEGRABL_NO_ERROR) {
			break;
		}
		/* If we get a reset or set config 0, we need to indicate to higher
		 * level software so that transfer can be scheduled again.
		 */
		if (p_xusb_dev_context->device_state != CONFIGURED) {
			/* Being suspended is ok. We should get un-suspended when host
			 * initiates a transfer.
			 * Removing cable will also create a suspend condition.
			 * Ideally this should be a disconnect condition but as we set
			 * vbus override, this is not the case. In this case, we will
			 * just wait here to be reconnected at which
			 * point we should receive a reset.
			 */
			if (p_xusb_dev_context->device_state != SUSPENDED) {
				/* This could be a reset or addressed pending state.
				 * Return same error. In any case, we need to go back into
				 * enumerate loop and re-schedule the transfer
				 */
				/*TODO: Need to define error value */
				break;
			}
		}
	}
	*p_bytes_transferred = p_xusb_dev_context->bytes_txfred;

	return e;
}

static tegrabl_error_t tegrabl_usbf_setup_static_params_pad(void)
{
	uint32_t reg_data;
	uint32_t rpd_ctrl;
	uint32_t hsterm_range_adj;
	uint32_t hscurr_level;
	uint32_t hssquelch_level;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	/* Read FUSE_USB_CALIB_EXT_0,FUSE_USB_CALIB_0 register to parse RPD_CTRL,
	 * HS_TERM_RANGE_ADJ, and HS_CURR_LEVEL fields and set corresponding fields
	 * in XUSB_PADCTL_USB2_OTG_PAD0_CTL_0/1 registers
	 */
	e = tegrabl_fuse_read(FUSE_USB_CALIB, &reg_data, sizeof(uint32_t));
	if (e) {
		goto fail;
	}
	hsterm_range_adj = (reg_data & TERM_RANGE_ADJ_MASK) >> TERM_RANGE_ADJ_SHIFT;
	hscurr_level = (reg_data & HS_CURR_LEVEL_MASK) >> HS_CURR_LEVEL_SHIFT;
	hssquelch_level =
		(reg_data & HS_SQUELCH_LEVEL_MASK) >> HS_SQUELCH_LEVEL_SHIFT;

	e = tegrabl_fuse_read(FUSE_USB_CALIB_EXT, &reg_data, sizeof(uint32_t));
	if (e) {
		goto fail;
	}
	rpd_ctrl = (reg_data & RPD_CTRL_MASK) >> RPD_CTRL_SHIFT;

	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0,
					HS_CURR_LEVEL, hscurr_level, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0,
					TERM_SEL, 1, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, reg_data);

	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1,
					TERM_RANGE_ADJ, hsterm_range_adj, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1,
					RPD_CTRL, rpd_ctrl, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0, reg_data);

	/* Program HS Squelch */
	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0,
					HS_SQUELCH_LEVEL, hssquelch_level, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, reg_data);


	/* With 20 nm soc, pads voltage is 1.5v so enable pad protection circuit
	 * against 3.3v
	 */
	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0);

	/* USB Pad protection circuit activation bug #1500052,
	 * T186: 1536486 http://nvbugs/1536486/29
	 * Decided this is not needed.
	 */
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD0_CTL1,
					PD_VREG, 1, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, reg_data);
fail:
	return e;
}

static void tegrabl_usbf_remove_powerdown_pad(void)
{
	uint32_t reg_data;

	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD_ZI, 0,
					reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD, 0,
					reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, reg_data);

	/* PD_DR field in XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 and
	 * XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 registers
	 */
	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, PD_DR, 0,
					reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0, reg_data);

	/* PD fields in XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0 */
	reg_data = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, PD, 0,
					reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
					XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, reg_data);
}

static void tegrabl_usbf_perform_tracking(void)
{
	uint32_t reg_data;
	enum tegrabl_clk_osc_freq osc_freq = tegrabl_get_osc_freq();

	TEGRABL_UNUSED(osc_freq);

	/* Perform tracking Bug 1440206 */
	/* Enable clock to tracking unit */
	tegrabl_usbf_program_tracking_clock(true);

	/* Setup the timing parameters  steps given in section 7 step 2 */

	/* Setting TRK_DONE to PD_TRK assertion/TRK_START de-assertion time */
	reg_data = XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_RESET_VAL;

	/* Setting PD_TRK de-assertion to TRK_START. 30 TRK clock cycles */
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1,
		TRK_START_TIMER, 0x1E, reg_data);

	/* Setting TRK_DONE to PD_TRK assertion. 10 TRK clock cycles */
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1,
		TRK_DONE_RESET_TIMER, 0xA, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, reg_data);

	/* Power up the pads and tracking circuit */
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1,
		PD_TRK, 0x0, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, reg_data);

	tegrabl_udelay(100);

	reg_data = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1,
		PD_TRK, 0x1, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, reg_data);


	/* Giving some time for powerdown.(30 trk clk cycles @0.1us) */
	tegrabl_udelay(3);

	/* Disable the tracking clocks */
	tegrabl_usbf_program_tracking_clock(false);
}

static tegrabl_error_t tegrabl_usbf_pad_setup(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_UTMI_PLL, 0, NULL);
	if (err) {
		goto fail;
	}

	err = tegrabl_usbf_setup_static_params_pad();
	if (err) {
		goto fail;
	}

	tegrabl_usbf_remove_powerdown_pad();

	tegrabl_usbf_perform_tracking();
	tegrabl_udelay(30);

fail:
	return err;
}

#define  XUSB_DEV_CFG_4_BASE_ADDR_SHIFT 15
#define  XUSB_DEV_CFG_4_BASE_ADDR_MASK 0x1FFFF

static tegrabl_error_t tegrabl_usbf_clock_and_pad_programming(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t val;

	/* Enable clock to XUSB pad Unit, Dev, SS clocks */
	err = tegrabl_car_clk_enable(TEGRABL_MODULE_XUSBF, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_XUSB_DEV, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_XUSB_SS, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_XUSB_HOST, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Assert reset to all units */
	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_HOST, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_DEV, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_PADCTL, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_SS, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_HOST, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Deassert padctl */
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_XUSB_PADCTL, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Setup OTG Pad and BIAS Pad ownership to XUSB */
	val = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_PAD_MUX_0);

	val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT0,
		XUSB, val);

	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_PAD_MUX_0, val);

	/* Configure the USB phy stabilization delay setting */
	err = tegrabl_usbf_pad_setup();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s:: %d usbf_pad setup failed\n", __func__,  __LINE__);
		goto fail;
	}

	/* Set port cap to device */
	val = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_PORT_CAP_0);

	val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP,
		PORT0_CAP, DEVICE_ONLY, val);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_USB2_PORT_CAP_0, val);

#if defined(CONFIG_ENABLE_XUSBF_SS)
	/* Set SS port cap to device */
	val = NV_READ32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_SS_PORT_CAP_0);

	val = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_PORT_CAP,
		PORT1_CAP, DEVICE_ONLY, val);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_PADCTL_BASE +
		XUSB_PADCTL_SS_PORT_CAP_0, val);
#endif

	err = tegrabl_usbf_clock_init();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s:: %d usbf clock init failed\n", __func__,  __LINE__);
		goto fail;
	}

	/* BUS_MASTER, MEMORY_SPACE, IO_SPACE ENABLE */
	/* TODO: Fix this hard-coding */
	val = NV_READ32(NV_ADDRESS_MAP_XUSB_DEV_BASE + 0x8000 +
					XUSB_DEV_CFG_1_0);
	val = NV_FLD_SET_DRF_NUM(XUSB_DEV, CFG_1, MEMORY_SPACE, 1, val);
	val = NV_FLD_SET_DRF_NUM(XUSB_DEV, CFG_1, BUS_MASTER, 1, val);
	NV_WRITE32(NV_ADDRESS_MAP_XUSB_DEV_BASE + 0x8000 +
					XUSB_DEV_CFG_1_0, val);

#if defined(CONFIG_ENABLE_XUSBF_SS)
	/* Program BAR0 space */
	val = NV_READ32(NV_ADDRESS_MAP_XUSB_DEV_BASE + 0x8000 +
					XUSB_DEV_CFG_4_0);

	val &= ~(XUSB_DEV_CFG_4_BASE_ADDR_MASK <<
		 XUSB_DEV_CFG_4_BASE_ADDR_SHIFT);
	val |= NV_ADDRESS_MAP_XUSB_DEV_BASE & (XUSB_DEV_CFG_4_BASE_ADDR_MASK <<
				  XUSB_DEV_CFG_4_BASE_ADDR_SHIFT);

	NV_WRITE32(NV_ADDRESS_MAP_XUSB_DEV_BASE + 0x8000 +
					XUSB_DEV_CFG_4_0, val);
	tegrabl_udelay(120);
#endif
	tegrabl_udelay(1);

fail:
	return err;
}

#if defined(CONFIG_ENABLE_XUSBF_SS)
/* FIXME - Use of regulator names directly is a quick temp soln */
static char *xudc_regulator_names[] = {
	"avdd-usb-supply",
	"dvdd-pex-supply",
	"hvdd-pex-supply",
	"dvdd-pex-pll-supply",
	"hvdd-pex-pll-supply"
};

static tegrabl_error_t tegrabl_usbf_regulator_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *fdt;
	const uint32_t *temp;
	int32_t xudc_node_offset = 0;
	int32_t reg_node_offset = 0;
	uint32_t reg_list_item = 0;
	uint32_t reg_voltage = 0;
	int32_t reg_phandle;

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (TEGRABL_NO_ERROR != err) {
		pr_warn("%s %d failed\n", __func__, __LINE__);
		goto fail;
	}

	err = tegrabl_dt_get_node_with_compatible(fdt , 0 ,
										XUDC_DT_COMPATIBLE,
										&xudc_node_offset);
	if (TEGRABL_NO_ERROR != err) {
		pr_warn("%s %d failed\n", __func__, __LINE__);
		goto fail;
	}

	/* TODO - parse all supply names with *-supply and
	 * avoid using xudc_regulator_names */
	for (reg_list_item = 0; reg_list_item < 5; reg_list_item++) {
		temp = fdt_getprop(fdt, xudc_node_offset,
						   xudc_regulator_names[reg_list_item],
						   NULL);

		if (temp != NULL) {
			reg_phandle = fdt32_to_cpu(*temp);
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, 1);
			continue;
		}

		reg_node_offset = fdt_node_offset_by_phandle(fdt, reg_phandle);
		temp = fdt_getprop(fdt, reg_node_offset,
						   "regulator-max-microvolt",
						   NULL);
		if (temp != NULL) {
			reg_voltage = fdt32_to_cpu(*temp);
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, 2);
			continue;
		}

		err = tegrabl_regulator_set_voltage(reg_phandle, reg_voltage,
											STANDARD_VOLTS);
	}

fail:
	if (TEGRABL_NO_ERROR != err) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}
#endif

static tegrabl_error_t tegrabl_usbf_priv_init(bool reinit)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	uint32_t reg_data;
	struct xusb_device_context *p_xusb_dev_context = &s_xusb_device_context;

#if defined(CONFIG_ENABLE_XUSBF_SS)
	/* Enable regulators needed */
	e = tegrabl_usbf_regulator_init();
	if (e != TEGRABL_NO_ERROR) {
		pr_warn("%s enable regulator failed\n", __func__);
	}
#endif

#if defined(CONFIG_ENABLE_XUSBF_UNCACHED_STRUCT)
	/* memset usb buffers to zero and flush them */
	memset((void *)DRAM_MAP_USB_START, 0, DRAM_MAP_USB_SIZE);
	tegrabl_dma_map_buffer(TEGRABL_MODULE_XUSBF, 0,
		(void *)DRAM_MAP_USB_START, DRAM_MAP_USB_SIZE, TEGRABL_DMA_TO_DEVICE);
#endif

	if (reinit == true) {
		p_xusb_dev_context->device_state = CONFIGURED;
		p_xusb_dev_context->port_speed = XUSB_HIGH_SPEED;
#if !defined(CONFIG_ENABLE_XUSBF_REENUMERATION)
		/* Set the EVENT_RING_HALT bit */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
							CTRL,
							EVENT_RING_HALT,
							TRUE,
							reg_data);
		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
#endif
	} else {
		pr_info("%s:%d full init:: ", __func__, __LINE__);

#if defined(CONFIG_ENABLE_XUSBF_SS)
		pr_info(" SUPER SPEED\n");
#else
		pr_info(" HIGH SPEED\n");
#endif

		e = tegrabl_usbf_clock_and_pad_programming();
		if (e != TEGRABL_NO_ERROR) {
			pr_warn("%d clock_pad init failed\n", __LINE__);
			return e;
		}
	}

	/* Initialize Event ring */
	tegrabl_init_ep_event_ring();

	if (reinit == true) {
#if !defined(CONFIG_ENABLE_XUSBF_REENUMERATION)
		/* Clear the EVENT_RING_HALT bit */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
							CTRL,
							EVENT_RING_HALT,
							FALSE,
							reg_data);
		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);

		NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0, 0xd);
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0);
		do {
			reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_EP_PAUSE_0);
		} while (reg_data != 0xd);
#endif
	}

	/* Initialize EP0 */
	e = tegrabl_initep(EP0_IN, reinit);
	if (e != TEGRABL_NO_ERROR) {
		return e;
	}

	if (reinit == true) {
		e = tegrabl_initep(EP1_IN, reinit);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}

		e = tegrabl_initep(EP1_OUT, reinit);
		if (e != TEGRABL_NO_ERROR) {
			return e;
		}
	} else {
		/* Make sure we get events due to port changes. */
		reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0);
		reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
				CTRL,
				LSE,
				EN,
				reg_data);

		reg_data = NV_FLD_SET_DRF_DEF(XUSB_DEV_XHCI,
				CTRL,
				IE,
				TRUE,
				reg_data);

		reg_data = NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_CTRL_0, reg_data);
	}

	/* Initialize EndPoint Context */
	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ECPLO_0,
				U64_TO_U32_LO(p_xusb_dev_context->dma_ep_context_start_addr));

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_ECPHI_0,
			   U64_TO_U32_HI(p_xusb_dev_context->dma_ep_context_start_addr));
#if defined(CONFIG_ENABLE_XUSBF_SS)
	reg_data = NV_READ32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0);
	reg_data = NV_FLD_SET_DRF_NUM(XUSB_DEV_XHCI,
				PORTHALT,
				STCHG_INTR_EN,
				1,
				reg_data);

	NV_WRITE32(XUSB_BASE + XUSB_DEV_XHCI_PORTHALT_0, reg_data);
#endif

	return e;
}

tegrabl_error_t tegrabl_usbf_init(void)
{
	return tegrabl_usbf_priv_init(false);
}

tegrabl_error_t tegrabl_usbf_reinit(void)
{
	return tegrabl_usbf_priv_init(true);
}

tegrabl_error_t tegrabl_usbf_start(void)
{
	return TEGRABL_NO_ERROR;
}

void tegrabl_usbf_setup(struct usbf_config *config)
{
	g_usbconfig = config;

	return;
}

tegrabl_error_t tegrabl_usbf_close(uint32_t instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	/* Put controller in reset state */
	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_SS, instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_critical("%s Failed to deassert XUSB_SS\n", __func__);
		return err;
	}
	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_DEV, instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_critical("%s Failed to deassert XUSB_DEV\n", __func__);
		return err;
	}
	err = tegrabl_car_rst_set(TEGRABL_MODULE_XUSB_PADCTL, instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_critical("%s Failed to deassert XUSB_PADCTL\n", __func__);
		return err;
	}

	return err;
}

