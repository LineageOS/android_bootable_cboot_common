/*
 * Copyright (c) 2015 - 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __TEGRABL_XUSB_PRIV_H
#define __TEGRABL_XUSB_PRIV_H

#include <stdint.h>

/* Desc macros */
#define USB_DEV_DESCRIPTOR_SIZE 18
#define USB_BOS_DESCRIPTOR_SIZE 22
#define USB_MANF_STRING_LENGTH 26
#define USB_PRODUCT_STRING_LENGTH 8
#define USB_SERIAL_NUM_LENGTH  12
#define USB_LANGUAGE_ID_LENGTH 4
#define USB_DEV_QUALIFIER_LENGTH 10
#define USB_DEV_STATUS_LENGTH

#define USB_LANGUAGE_ID   0
#define USB_MANF_ID       1
#define USB_PROD_ID       2
#define USB_SERIAL_ID     3
#define USB_DEVICE_SELF_POWERED 1

/* Feature Select */
#define ENDPOINT_HALT        0
#define DEVICE_REMOTE_WAKEUP 1
#define TEST_MODE            2
/* USB 3.0 defined */
#define U1_ENABLE           48
#define U2_ENABLE           49
#define LTM_ENABLE          50

/* USB Setup Packet Byte Offsets */
#define USB_SETUP_REQUEST_TYPE  0
#define USB_SETUP_REQUEST       1
#define USB_SETUP_VALUE         2
#define USB_SETUP_DESCRIPTOR    3
#define USB_SETUP_INDEX         4
#define USB_SETUP_LENGTH        6

/* USB Setup Packet Request Type */
#define HOST2DEV_DEVICE     0x00
#define HOST2DEV_INTERFACE  0x01
#define HOST2DEV_ENDPOINT   0x02
#define DEV2HOST_DEVICE     0x80
#define DEV2HOST_INTERFACE  0x81
#define DEV2HOST_ENDPOINT   0x82

/*  USB Setup Packet Request */
#define GET_STATUS        0
#define CLEAR_FEATURE     1
#define SET_FEATURE       3
#define SET_ADDRESS       5
#define GET_DESCRIPTOR    6
#define SET_DESCRIPTOR    7
#define GET_CONFIGURATION 8
#define SET_CONFIGURATION 9
#define GET_INTERFACE     10
#define SET_INTERFACE     11
#define SYNCH_FRAME       12
#define SET_SEL           48
#define SET_ISOCH_DELAY   49

/* USB Descriptor Type */
#define USB_DT_DEVICE             1
#define USB_DT_CONFIG             2
#define USB_DT_STRING             3
#define USB_DT_INTERFACE          4
#define USB_DT_ENDPOINT           5
#define USB_DT_DEVICE_QUALIFIER   6
#define USB_DT_OTHER_SPEED_CONFIG 7
#define USB_DT_INTERFACE_POWER    8
#define USB_DT_INTERFACE_ASSOCIATION 11
#define USB_DT_BOS                15
#define USB_DT_DEVICE_CAPABILITY  16
#define USB_DT_SS_USB_EP_COMPANION 48

#define BCDUSB_VERSION_LSB 0
#define BCDUSB_VERSION_MSB 2

#define BCDUSB3_VERSION_LSB 0
#define BCDUSB3_VERSION_MSB 3
#define EP0_PKT_SIZE 9

/* Misc macros */
#define EP_RUNNING          1
#define EP_DISABLED         0

#define SETUP_PACKET_BUFFER_NUM 2

#define DIR_OUT                 0
#define DIR_IN                     1

/* descriptor gets only 4 low bits */
#define NVTBOOT_USBF_DESCRIPTOR_SKU_MASK  0xF

/* Usb speed */
#define XUSB_FULL_SPEED  1
#define XUSB_HIGH_SPEED  3
#define XUSB_SUPER_SPEED 4

/* EndPoint types */
#define EP_TYPE_CNTRL         4
#define EP_TYPE_BULK_OUT     2
#define EP_TYPE_BULK_IN     6

/* TRB Types */
#define NONE_TRB                0
#define NORMAL_TRB              1
#define DATA_STAGE_TRB          3
#define STATUS_STAGE_TRB        4
#define LINK_TRB                6
#define TRANSFER_EVENT_TRB      32
#define PORT_STATUS_CHANGE_TRB  34
#define SETUP_EVENT_TRB         63

/* Error codes */
#define TRB_ERR_CODE            5
#define SUCCESS_ERR_CODE        1
#define DATA_BUF_ERR_CODE       2
#define SHORT_PKT_ERR_CODE      13
#define CTRL_SEQ_NUM_ERR_CODE  223
#define CTRL_DIR_ERR_CODE      222

/* XUSB speed */
#define XUSB_SUPERSPEED 0x4
#define XUSB_HIGHSPEED  0x3
#define XUSB_FULLSPEED  0x2


/**
 * @brief Defines the enum for endpoint number
 */
enum  endpoint {
	EP0_IN = 0, /* Bi-directional */
	EP0_OUT, /* Note: This is not used. */
	EP1_OUT,
	EP1_IN,
	EPX_MAX = 0xFFFF, /* half word size */
};

/**
 * @brief Defines the device state
 */
enum device_state {
	DEFAULT,
	CONNECTED,
	DISCONNECTED,
	RESET,
	ADDRESSED_STATUS_PENDING,
	ADDRESSED,
	CONFIGURED_STATUS_PENDING,
	CONFIGURED,
	SUSPENDED,
};

/**
 * @brief USB function interface structure
 */
struct xusb_device_context {
	/* As Producer (of Control TRBs) EP0_IN*/
	uintptr_t cntrl_epenqueue_ptr;
	uintptr_t cntrl_epdequeue_ptr;
	uint32_t cntrl_pcs; /* Producer Cycle State */
	/* As Producer (of Transfer TRBs) for EP1_OUT*/
	uintptr_t bulkout_epenqueue_ptr;
	uintptr_t bulkout_epdequeue_ptr;
	uint32_t bulkout_pcs; /* Producer Cycle State */
	/* As Producer (of Transfer TRBs) for EP1_IN*/
	uintptr_t bulkin_epenqueue_ptr;
	uintptr_t bulkin_epdequeue_ptr;
	uint32_t bulkin_pcs; /* Producer Cycle State */
	/* As Consumer (of Event TRBs) */
	uintptr_t event_enqueue_ptr;
	uintptr_t event_dequeue_ptr;
	uint32_t event_ccs; /* Consumer Cycle State */
	dma_addr_t dma_er_start_address; /* DMA addr for endpoint ring start ptr*/
	dma_addr_t dma_ep_context_start_addr; /* DMA addr for ep context start ptr*/
	enum device_state device_state;
	uint32_t initialized;
	uint32_t enumerated;
	uint32_t bytes_txfred;
	uint32_t tx_count;
	uint32_t cntrl_seq_num;
	uint32_t setup_pkt_index;
	uint32_t config_num;
	uint32_t interface_num;
	uint32_t wait_for_eventt;
	uint32_t port_speed;
};

/**
 * @brief Template used to parse event TRBs
 */
struct event_trb {
	uint32_t rsvd0;
	uint32_t rsvd1;
	/* DWord2 begin */
	uint32_t rsvd2:24;
	uint32_t comp_code:8;     /* Completion Code */
	/* DWord2 end */
	uint32_t c:1;          /* Cycle Bit */
	uint32_t rsvd3:9;
	uint32_t trb_type:6;    /* Identifies the type of TRB */
	/* (Setup Event TRB, Port status Change TRB etc) */
	uint32_t emp_id:5;         /* Endpoint ID. */
	uint32_t rsvd4:11;
	/* DWord3 end */
};

/**
 * @brief Defines the structure for Setup Event TRB
 */
struct setup_event_trb {
	uint32_t data[2];
	/* DWord2 begin */
	uint32_t ctrl_seq_num:16;    /* Control Sequence Number */
	uint32_t rsvddw2_0:8;
	uint32_t comp_code:8;       /* Completion Code */
	/* DWord2 end */
	uint32_t c:1;              /* Cycle bit */
	uint32_t rsvddw3_0:9;
	uint32_t trb_type:6;        /* TrbType = 63 for Setup Event TRB */
	uint32_t emp_id:5;        /* Endpoint ID. */
	uint32_t rsvddw3_1:11;
	/* DWord3 end */
};

/**
 * @brief Defines the structure for status TRB
 */
struct status_trb {
	uint32_t rsvdw0;
	uint32_t rsvddw1;
	/* DWord2 begin */
	uint32_t rsvddw2_0:22;
	uint32_t int_target:10; /* Interrupter Target */
	/* DWord2 end */
	uint32_t c:1; /* Cycle bit */
	uint32_t ent:1; /* Evaluate Next TRB */
	uint32_t rsvddw3_0:2;
	uint32_t ch:1; /* Chain bit */
	uint32_t ioc:1; /* Interrupt on Completion */
	uint32_t rsvd4:4;
	uint32_t trb_type:6;
	uint32_t dir:1;
	uint32_t rsvddw3_1:15;
	/* DWord3 end */
};

/**
 * @brief Defines the structure for data status TRB
 */
struct data_trb {
	uint32_t databufptr_lo;
	uint32_t databufptr_hi;
	/* DWord2 begin */
	uint32_t trb_tx_len:17;
	uint32_t tdsize:5;
	uint32_t int_target:10;
	/* DWord2 end */
	uint32_t c:1; /* Cycle bit */
	uint32_t ent:1; /* Evaluate Next TRB */
	uint32_t isp:1; /* Interrupt on Short Packet */
	uint32_t ns:1; /* No Snoop */
	uint32_t ch:1; /* Chain bit */
	uint32_t ioc:1; /* Interrupt on Completion */
	uint32_t rsvddw3_0:4;
	uint32_t trb_type:6;
	uint32_t dir:1;
	uint32_t rsvddw3_1:15;
	/* DWord3 end */
};

/**
 * @brief Defines the structure for Normal TRB
 */
struct normal_trb {
	uint32_t databufptr_lo;
	uint32_t databufptr_hi;
	/* DWord2 begin */
	uint32_t trb_tx_len:17;
	uint32_t tdsize:5;
	uint32_t int_target:10;
	/* DWord2 end */
	uint32_t c:1; /* Cycle bit */
	uint32_t ent:1; /* Evaluate Next TRB */
	uint32_t isp:1; /* Interrupt on Short Packet */
	uint32_t ns:1; /* No Snoop */
	uint32_t ch:1; /* Chain bit */
	uint32_t ioc:1; /* Interrupt on Completion */
	uint32_t idt:1; /* Immediate data */
	uint32_t rsvddw3_0:2;
	uint32_t bei:1; /* Block Event Interrupt */
	uint32_t trb_type:6;
	uint32_t rsvddw3_1:16;
	/* DWord3 end */
};

/**
 * @brief Defines the structure for Transfer event TRB
 */
struct transfer_event_trb {
	uint32_t trb_pointer_lo;
	uint32_t trb_pointer_hi;
	/* DWord1 end */
	uint32_t trb_tx_len:24;
	uint32_t comp_code:8;     /* Completion Code */
	/* DWord2 end */
	uint32_t c:1;          /* Cycle Bit */
	uint32_t rsvddw3_0:1;
	uint32_t ed:1;   /* Event data. (Immediate data in 1st 2 words or not) */
	uint32_t rsvddw3_1:7;
	uint32_t trb_type:6;    /* Identifies the type of TRB */
	/* (Setup Event TRB, Port status Change TRB etc) */
	uint32_t emp_id:5;         /* Endpoint ID. */
	uint32_t rsvddw3_2:11;
	/* DWord3 end */
};

/**
 * @brief Defines the structure for Link TRB
 */
struct link_trb {
	/* DWORD0 */
	uint32_t rsvddW0_0:4;
	uint32_t ring_seg_ptrlo:28;
	/* DWORD1 */
	uint32_t ring_seg_ptrhi;
	/* DWORD2 */
	uint32_t rsvddw2_0:22;
	uint32_t int_target:10;
	/* DWORD3 */
	uint32_t c:1;
	uint32_t tc:1;
	uint32_t rsvddw3_0:2;
	uint32_t ch:1;
	uint32_t ioc:1;
	uint32_t rsvddw3_1:4;
	uint32_t trb_type:6;
	uint32_t rsvddw3_2:16;
};

/**
 * @brief Defines the structure for endpoint context
 */
struct ep_context {
	/* DWORD0 */
	uint32_t ep_state:3;
	uint32_t rsvddW0_0:5;
	uint32_t mult:2;
	uint32_t max_pstreams:5;
	uint32_t lsa:1;
	uint32_t interval:8;
	uint32_t rsvddW0_1:8;
	/* DWORD1 */
	uint32_t rsvddw1_0:1;
	uint32_t cerr:2;
	uint32_t ep_type:3;
	uint32_t rsvddw1_1:1;
	uint32_t hid:1;
	uint32_t max_burst_size:8;
	uint32_t max_packet_size:16;
	/* DWORD2 */
	uint32_t dcs:1;
	uint32_t rsvddw2_0:3;
	uint32_t trd_dequeueptr_lo:28;
	/* DWORD3 */
	uint32_t trd_dequeueptr_hi;
	/* DWORD4 */
	uint32_t avg_trb_len:16;
	uint32_t max_esit_payload:16;
	/******* Nvidia specific from here ******/
	/* DWORD5 */
	uint32_t event_data_txlen_acc:24;
	uint32_t rsvddw5_0:1;
	uint32_t ptd:1;
	uint32_t sxs:1;
	uint32_t seq_num:5;
	/* DWORD6 */
	uint32_t cprog:8;
	uint32_t sbyte:7;
	uint32_t tp:2;
	uint32_t rec:1;
	uint32_t cec:2;
	uint32_t ced:1;
	uint32_t hsp1:1;
	uint32_t rty1:1;
	uint32_t std:1;
	uint32_t status:8;
	/* DWORD7 */
	uint32_t data_offset:17;
	uint32_t rsvddw6_0:4;
	uint32_t lpa:1;
	uint32_t num_trb:5;
	uint32_t num_p:5;
	/* DWORD8 */
	uint32_t scratch_pad0;
	/* DWORD8 */
	uint32_t scratch_pad1;
	/* DWORD10 */
	uint32_t cping:8;
	uint32_t sping:8;
	uint32_t tc:2;
	uint32_t ns:1;
	uint32_t ro:1;
	uint32_t tlm:1;
	uint32_t dlm:1;
	uint32_t hsp2:1;
	uint32_t rty2:1;
	uint32_t stop_rec_req:8;
	/* DWORD11 */
	uint32_t device_addr:8;
	uint32_t hub_addr:8;
	uint32_t root_port_num:8;
	uint32_t slot_id:8;
	/* DWORD12 */
	uint32_t routing_string:20;
	uint32_t speed:4;
	uint32_t lpu:1;
	uint32_t mtt:1;
	uint32_t hub:1;
	uint32_t dci:5;
	/* DWORD13 */
	uint32_t tthub_slot_id:8;
	uint32_t ttport_num:8;
	uint32_t ssf:4;
	uint32_t sps:2;
	uint32_t int_target:10;
	/* DWORD14 */
	uint32_t frz:1;
	uint32_t end:1;
	uint32_t elm:1;
	uint32_t mrx:1;
	uint32_t ep_linklo:28;
	/* DWORD15 */
	uint32_t ep_linkhi;
};

#endif
