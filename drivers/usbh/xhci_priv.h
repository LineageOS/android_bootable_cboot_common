/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __XHCI_PRIV_H
#define __XHCI_PRIV_H

#include <string.h>
#include <stdint.h>
#include <tegrabl_dmamap.h>
#include <usbh_protocol.h>

#define ARU_C11_CSBRANGE    0x0000041C

#define CAP_REG0        0x00000000
#define CAP_HCSPARAMS1      0x00000004
#define CAP_HCSPARAMS2      0x00000008
#define CAP_HCSPARAMS3      0x0000000C
#define CAP_HCCPARAMS1      0x00000010
#define CAP_DBOFF       0x00000014
#define CAP_RTSOFF      0x00000018
#define CAP_HCCPARAM2       0x0000001C

#define OP_USBCMD       0x00000020
#define OP_USBSTS       0x00000024
#define OP_PGSZ         0x00000028
#define OP_DNCTRL       0x00000034
#define OP_CRCR0        0x00000038
#define OP_CRCR1        0x0000003C
#define OP_DCBAAP0      0x00000050
#define OP_DCBAAP1      0x00000054
#define OP_CONFIG       0x00000058
#define OP_PORTSC(x)        (0x00000420+(x)*16)
#define OP_PORTPMSCHS(x)    (0x00000424+(x)*16)
#define OP_PORTLISC(x)      (0x00000428+(x)*16)
#define OP_PORTHLPMC(x)     (0x0000042C+(x)*16)

#define EC_USBLEGSUP        0x00000600
#define EC_USBLEGSCTLSTS    0x00000604
#define EC_SUPPORT_USB3_0   0x00000610
#define EC_SUPPORT_USB3_1   0x00000614
#define EC_SUPPORT_USB3_2   0x00000618
#define EC_SUPPORT_USB3_3   0x0000061C
#define EC_SUPPORT_USB3_PSI_G1  0x00000620
#define EC_SUPPORT_USB3_PSI_G2  0x00000624
#define EC_SUPPORT_USB2_0   0x00000628
#define EC_SUPPORT_USB2_1   0x0000062C
#define EC_SUPPORT_USB2_2   0x00000630
#define EC_SUPPORT_USB2_3   0x00000634
#define EC_XHCIIOV_HDR      0x00000638
#define EC_XHCIIOV_INT_RANGE(x) (0x0000063C+(x)*4)
#define EC_XHCIIOV_SLOT_ASSIGN(x)   (0x0000064C+(x)*4)

/* Below offsets are chip specific */
#define RT_MFINDEX      0x00020000
#define RT_IMAN(x)      (0x00020020+(x)*32)
#define RT_IMOD(x)      (0x00020024+(x)*32)
#define RT_ERSTSZ(x)        (0x00020028+(x)*32)
#define RT_ERRSVD(x)        (0x0002002C+(x)*32)
#define RT_ERSTBA0(x)       (0x00020030+(x)*32)
#define RT_ERSTBA1(x)       (0x00020034+(x)*32)
#define RT_ERDP0(x)     (0x00020038+(x)*32)
#define RT_ERDP1(x)     (0x0002003C+(x)*32)
#define DB(x)           (0x00030000+(x)*4)

#define MAX_PORTS   3

#define PORT_MUX_SHIFT(x)           ((x) * 2)
#define PORT_MUX_MASK               (0x3)
#define PORT_CAP_SHIFT(x)           ((x) * 4)
#define PORT_CAP_MASK               (0x3)
#define   PORT_CAP_DISABLED         (0x0)
#define   PORT_CAP_HOST             (0x1)
#define   PORT_CAP_DEVICE           (0x2)
#define   PORT_CAP_OTG              (0x3)
#define USB2_OTG_PAD_CTL0(x)            (0x88 + (x) * 0x40)
#define USB2_OTG_PAD_CTL1(x)            (0x8c + (x) * 0x40)
#define USB2_BATTERY_CHRG_OTGPAD_CTL0(x)    (0x80 + (x) * 0x40)
#define USB2_BATTERY_CHRG_OTGPAD_CTL1(x)    (0x84 + (x) * 0x40)
#define   PORT_OC_PIN_SHIFT(x)          ((x) * 4)


#define TERM_RANGE_ADJ_MASK     0x00000780
#define TERM_RANGE_ADJ_SHIFT    7
#define HS_CURR_LEVEL_MASK      0x0000003F
#define HS_CURR_LEVEL_SHIFT(x)  ((x) ? (11 + (x - 1) * 6) : 0)
#define HS_SQUELCH_LEVEL_MASK   0xE0000000U
#define HS_SQUELCH_LEVEL_SHIFT  29
#define RPD_CTRL_MASK           0x0000001F
#define RPD_CTRL_SHIFT          0

/* USB Setup Packet size */
#define USB_SETUP_PKT_SIZE  8
/* USB device decriptor size */
#define USB_DEV_DESCRIPTOR_SIZE 18
/* USB Configuration decriptor size */
#define USB_CONFIG_DESCRIPTOR_SIZE  9
/* Define for maximum usb Manufacturer String Length.*/
#define USB_MANF_STRING_LENGTH  26
/* Define for maximum usb Product String Length */
#define USB_PRODUCT_STRING_LENGTH   8
/* Define for maximum usb Product String Length */
#define USB_SERIAL_NUM_LENGTH   12
/* Define for Language ID String Length */
#define USB_LANGUAGE_ID_LENGTH  4
/* Define for Device Qualifier String Length */
#define USB_DEV_QUALIFIER_LENGTH    10
/* Define for Device Status Length */
#define USB_DEV_STATUS_LENGTH   2
/* Define for usb logical unit Length.*/
#define USB_GET_MAX_LUN_LENGTH  1

/* Define for HS control bulk max packetsize Length */
enum {
	USB_HS_CONTROL_MAX_PACKETSIZE = 64,
	USB_HS_BULK_MAX_PACKETSIZE = 512
};

/* Define for TRB average Length */
enum {
	USB_TRB_AVERAGE_CONTROL_LENGTH = 8,
	USB_TRB_AVERAGE_INTERRUPT_LENGTH = 64,
	USB_TRB_AVERAGE_BULK_LENGTH = 512
};

/* Define for TR and data bufer memory location */
enum {
	SYSTEM_MEM = 0, /* iram */
	DDIRECT = 1 /* dram */
};

enum {
	VBUS_ENABLE_0 = 0,
	VBUS_ENABLE_1 = 1
};

enum {
	CTRL_OUT = 0x00,
	DCI_CTRL = 0x01,
	CTRL_IN = 0x80
};

enum {
	BULK_OUT = 0x01,
	BULK_IN = 0x81
};

enum {
	FULL_SPEED = 1,
	HIGH_SPEED = 3
};

#define USB_MAX_TXFR_RETRIES    3
#define XUSB_CFG_PAGE_SIZE  0x200
#define XUSB_DEV_ADDR   1

/* This needs to be more than the BCT struct info */
enum {
	MSC_MAX_PAGE_SIZE_LOG_2 = 9,
	XUSB_MSC_BOT_PAGE_SIZE_LOG2 = 11,   /* 2K */
	NVBOOT_MSC_BLOCK_SIZE_LOG2 = 14
};

/* For High-Speed control and bulk endpoints
 * this field shall be cleared to 0. */
#define MAX_BURST_SIZE  0
#define ONE_MILISEC 3   /* 2^3 * 125us = 8 * 125us = 1ms */
#define PORT_INSTANCE_OFFSET    0x10

/* retries */
enum {
	CONTROLLER_HW_RETRIES_100 = 100,
	CONTROLLER_HW_RETRIES_2000 = 2000,
	CONTROLLER_HW_RETRIES_1SEC = 1000000
};

/* Wait time for controller H/W status to change
 * before giving up in terms of mircoseconds */
enum {
	TIMEOUT_1US = 1,    /* 1 micro seconds */
	TIMEOUT_10US = 10,  /* 10 micro seconds */
	TIMEOUT_1MS = 1000, /* 1 milli sec */
	TIMEOUT_10MS = 10000,   /* 10 milli sec */
	TIMEOUT_50MS = 50000,   /* 50 milli sec */
	TIMEOUT_100MS = 100000, /* 100 milli sec */
	TIMEOUT_200MS = 200000, /* 200 milli sec */
	TIMEOUT_500MS = 500000, /* 500 milli sec */
	TIMEOUT_1S = 1000000    /* 1000 milli seconds */
};

enum usb_dir {
	USB_DIR_OUT = 0, /* matches HOST2DEV enum */
	USB_DIR_IN, /* matches DEV2HOST enum */
};

/**
 * Defines USB descriptor types.
 * As per USB2.0 Specification.
 */
enum usb_dev_desc_type {
	/* Specifies a device descriptor type */
	USB_DT_DEVICE = 1,
	/* Specifies a device configuration type */
	USB_DT_CONFIG = 2,
	/* Specifies a device string type */
	USB_DT_STRING = 3,
	/* Specifies a device interface type */
	USB_DT_INTERFACE = 4,
	/* Specifies a device endpoint type */
	USB_DT_ENDPOINT = 5,
	/* Specifies a device qualifier type */
	USB_DT_DEVICE_QUALIFIER = 6,
	/* Specifies the other speed configuration type */
	USB_DT_OTHER_SPEED_CONFIG = 7
};

/**
 * Specifies the device request type to be sent across the USB.
 */
enum usb_device_request_type {
	GET_STATUS = 0,
	CLEAR_FEATURE = 1,  /* clear feature request */
	ENDPOINT_CLEAR_FEATURE = 1, /* endpoint clear feature request */
	SET_FEATURE = 3,    /* set feature request */
	SET_ADDRESS = 5,    /* set address request */
	GET_DESCRIPTOR = 6, /* get device descriptor request */
	SET_DESCRIPTOR = 7, /* set descriptor request */
	GET_CONFIGURATION = 8,  /* get configuration request */
	SET_CONFIGURATION = 9,  /* set configuration request */
	GET_INTERFACE = 10, /* get interface request */
	SET_INTERFACE = 11, /* set interface request */
	SYNC_FRAME = 12,    /* sync frame request */
	RESET_PORT = 13,    /* reset port request */
	POWERON_VBUS = 14,  /* power on VBUS request */
	POWEROFF_VBUS = 15, /* power oFF VBUS request */
	PORT_SPEED = 16,    /* port speed request */
	PORT_STATUS = 17,   /* port status request */
	GET_MAX_LUN = 0xFE  /* CLASS specific request for GET_MAX_LUN */
};

/**
 * Defines the direction field in USB device request codes
 */
enum usb_req_dir {
	/* host to device */
	HOST2DEV = 0,
	/* device to host */
	DEV2HOST = 1
};

/**
 * Defines the type field in USB device request codes
 *
 */
enum usb_req_type {
	/* standard request type, required in all devices */
	REQ_STANDARD = 0,
	/* class-specific request type */
	REQ_CLASS = 1,
	/* vendor-specific request type */
	REQ_VENDOR = 2,
	/* reserved request */
	REQ_RESERVED = 3
};

/**
 * Defines the recipient field in USB device request codes
 */
enum usb_req_target {
	/* Specifies the  USB device is the target of the request */
	TARGET_DEVICE = 0,
	/* Specifies an interface on the USB device is the target of the request */
	TARGET_INTERFACE = 1,
	/* Specifies an endpoint on the USB device is the target of the request */
	TARGET_ENDPOINT = 2,
	/* Specifies an unspecified target on the
	   USB device is specified by the request */
	TARGET_OTHER = 3,
	/* Specifies all other targets are reserved */
	TARGET_RESERVED = 4
};

/**
 * Defines the request type in standard device requests
 */
enum {
	/* Specifies the data transfer direction:
	   host-to-device, recipient: device */
	HOST2DEV_DEVICE = 0x00,
	/* Specifies the data transfer direction:
	   host-to-device, recipient: interface */
	HOST2DEV_INTERFACE = 0x01,
	/* Specifies the data transfer direction:
	   host-to-device, recipient: endpoint */
	HOST2DEV_ENDPOINT = 0x02,
	/* Specifies the data transfer direction:
	   host-to-device, recipient: endpoint */
	HOST2DEV_OTHER = 0x03,
	/* Specifies the data transfer direction:
	   device-to-host, transmitter: device */
	DEV2HOST_DEVICE = 0x80,
	/* Specifies the data transfer direction:
	   device-to-host, transmitter: interface */
	DEV2HOST_INTERFACE = 0x81,
	/* Specifies the data transfer direction:
	   device-to-host, transmitter: endpoint */
	DEV2HOST_ENDPOINT = 0x82,
	/* Specifies the data transfer direction:
	   device-to-host, transmitter: endpoint */
	DEV2HOST_OTHER = 0x83,
	/* Specifies the data transfer direction:
	   device-to-host, transmitter: interface */
	CLASS_SPECIFIC_REQUEST = 0xA1
};

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define HCS_MAX_SCRATCHPAD(p)   ((((p) >> 16) & 0x3e0) | (((p) >> 27) & 0x1f))
#define PORT_ID(p)      (((p) & (0xff << 24)) >> 24)
#define PORT_CONNECT        (1 << 0)
#define PORT_PE         (1 << 1)
#define PORT_RESET      (1 << 4)
#define DEV_SPEED_MASK      (0xf << 10)
#define XDEV_FS         (0x1 << 10)
#define XDEV_LS         (0x2 << 10)
#define XDEV_HS         (0x3 << 10)
#define DEV_UNDEFSPEED(p)   (((p) & DEV_SPEED_MASK) == (0x0<<10))
#define DEV_FULLSPEED(p)    (((p) & DEV_SPEED_MASK) == XDEV_FS)
#define DEV_LOWSPEED(p)     (((p) & DEV_SPEED_MASK) == XDEV_LS)
#define DEV_HIGHSPEED(p)    (((p) & DEV_SPEED_MASK) == XDEV_HS)
#define DEV_PORT_SPEED(p)   (((p) >> 10) & 0x0f)

/* Bits 20:23 in the Slot Context are the speed for the device */
#define PORT_POWER  (1 << 9)

#define PORT_CSC    (1 << 17)
#define PORT_PEC    (1 << 18)
#define PORT_WRC    (1 << 19)
#define PORT_RC     (1 << 21)
#define PORT_PLC    (1 << 22)
#define PORT_CEC    (1 << 23)
#define PORT_CAS    (1 << 24)
/* device speed:
 * 1 - full
 * 2 - low
 * 3 - high
 * 4 - SS
 */
#define PORT_PLS_MASK   (0xf << 5)
#define XDEV_U0     (0x0 << 5)
#define XDEV_U2     (0x2 << 5)
#define XDEV_U3     (0x3 << 5)
#define XDEV_RXDETECT   (0x5 << 5)
#define XDEV_INACTIVE   (0x6 << 5)
#define XDEV_POLLING    (0x7 << 5)
#define XDEV_RESUME (0xf << 5)

#define CMD_RUN     (1 << 0)
#define CMD_RESET   (1 << 1)
#define CMD_EIE     (1 << 2)
#define CMD_HSEIE   (1 << 3)
#define CMD_LRESET  (1 << 7)
#define CMD_CSS     (1 << 8)
#define CMD_CRS     (1 << 9)
#define CMD_EWE     (1 << 10)
#define XHCI_IRQS   (CMD_EIE | CMD_HSEIE | CMD_EWE)

#define STS_HALT    (1 << 0)
#define STS_FATAL   (1 << 2)
#define STS_EINT    (1 << 3)
#define STS_PORT    (1 << 4)
#define STS_CNR     (1 << 11)
#define IMAN_IP     (1 << 0)
#define ER_IRQ_PENDING(p)   ((p) & 0x1)
#define ER_IRQ_CLEAR(P)     ((p) & 0xffffffffe)
#define ER_IRQ_ENABLE(p)    ((p) | 0x2)
#define ER_IRQ_DISABLE(p)   ((p) | ~(0x2))
#define ER_IRQ_INTERVAL_MASK    (0xffff)

#define ERST_SIZE_MASK  (0xffff << 16)
#define ERST_DESI_MASK  (0x7)
#define ERST_EHB    (1 << 3)
#define ERST_PTR_MASK   (0xf)

#define DB_VALUE(ep, stream)    ((((ep) + 1) & 0xff) | ((stream) << 16))
#define DB_VALUE_HOST   0x00000000

#define COMP_CODE_MASK  (0xff << 24)
#define COMP_CODE(p)    (((p) & COMP_CODE_MASK) >> 24)
/* Add all possible COMPLETION CODES to help w/debugging */
#define COMP_INVALID				0
#define COMP_SUCCESS				1
#define COMP_DATA_BUFFER_ERROR			2
#define COMP_BABBLE_DETECTED_ERROR		3
#define COMP_USB_TRANSACTION_ERROR		4
#define COMP_TRB_ERROR				5
#define COMP_STALL_ERROR			6
#define COMP_RESOURCE_ERROR			7
#define COMP_BANDWIDTH_ERROR			8
#define COMP_NO_SLOTS_AVAILABLE_ERROR		9
#define COMP_INVALID_STREAM_TYPE_ERROR		10
#define COMP_SLOT_NOT_ENABLED_ERROR		11
#define COMP_ENDPOINT_NOT_ENABLED_ERROR		12
#define COMP_SHORT_PACKET			13
#define COMP_RING_UNDERRUN			14
#define COMP_RING_OVERRUN			15
#define COMP_VF_EVENT_RING_FULL_ERROR		16
#define COMP_PARAMETER_ERROR			17
#define COMP_BANDWIDTH_OVERRUN_ERROR		18
#define COMP_CONTEXT_STATE_ERROR		19
#define COMP_NO_PING_RESPONSE_ERROR		20
#define COMP_EVENT_RING_FULL_ERROR		21
#define COMP_INCOMPATIBLE_DEVICE_ERROR		22
#define COMP_MISSED_SERVICE_ERROR		23
#define COMP_COMMAND_RING_STOPPED		24
#define COMP_COMMAND_ABORTED			25
#define COMP_STOPPED				26
#define COMP_STOPPED_LENGTH_INVALID		27
#define COMP_STOPPED_SHORT_PACKET		28
#define COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR	29
#define COMP_ISOCH_BUFFER_OVERRUN		31
#define COMP_EVENT_LOST_ERROR			32
#define COMP_UNDEFINED_ERROR			33
#define COMP_INVALID_STREAM_ID_ERROR		34
#define COMP_SECONDARY_BANDWIDTH_ERROR		35
#define COMP_SPLIT_TRANSACTION_ERROR		36

#define TRB_TYPE_BITMASK    (0xfc00)
#define TRB_TYPE(p)     ((p) << 10)
#define TRB_FIELD_TO_TYPE(p)    (((p) & TRB_TYPE_BITMASK) >> 10)
/* TRB type IDs */
#define TRB_NORMAL      1
#define TRB_SETUP       2
#define TRB_DATA        3
#define TRB_STATUS      4
#define TRB_ISOC        5
#define TRB_LINK        6
#define TRB_EVENT_DATA      7
#define TRB_TR_NOOP     8
/* Command TRBs */
#define TRB_ENABLE_SLOT     9
#define TRB_DISABLE_SLOT    10
#define TRB_ADDR_DEV        11
#define   ADDR_DEV_BSR      (1 << 9)
#define TRB_CONFIG_EP       12
#define TRB_EVAL_CONTEXT    13
#define TRB_RESET_EP        14
#define TRB_STOP_RING       15
#define TRB_SET_DEQ     16
#define TRB_RESET_DEV       17
#define TRB_FORCE_EVENT     18
#define TRB_NEG_BANDWIDTH   19
#define TRB_SET_LT      20
#define TRB_GET_BW      21
#define TRB_FORCE_HEADER    22
#define TRB_CMD_NOOP        23
/* Event TRBS */
#define TRB_TRANSFER        32
#define TRB_COMPLETION      33
#define TRB_PORT_STATUS     34
#define TRB_BANDWIDTH_EVENT 35
#define TRB_DOORBELL        36
#define TRB_HC_EVENT        37
#define TRB_DEV_NOTE        38
#define TRB_MFINDEX_WRAP    39

#define TRB_TYPE_LINK(x)    (((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))

/* xhci_cap_regs */
/* hc_capbase bitmasks */
#define HC_LENGTH(p)        (((p) >> 00) & 0x00ff)
#define HC_VERSION(p)       (((p) >> 16) & 0xffff)
/* HCSPARAMS1 - hcs_params1 - bitmasks */
#define HCS_MAX_SLOTS(p)    (((p) >> 0) & 0xff)
#define HCS_MAX_INTRS(p)    (((p) >> 8) & 0x7ff)
#define HCS_MAX_PORTS(p)    (((p) >> 24) & 0x7f)
/* HCSPARAMS2 - hcs_params2 - bitmasks */
#define HCS_IST(p)      (((p) >> 0) & 0xf)
#define HCS_ERST_MAX(p)     (((p) >> 4) & 0xf)
/* HCSPARAMS3 - hcs_params3 - bitmasks */
#define HCS_U1_LATENCY(p)   (((p) >> 0) & 0xff)
#define HCS_U2_LATENCY(p)   (((p) >> 16) & 0xffff)
/* HCCPARAMS - hcc_params - bitmasks */
#define HCC_64BIT_ADDR(p)   ((p) & (1 << 0))
#define HCC_64BYTE_CONTEXT(p)   ((p) & (1 << 2))
#define HCC_SPC(p)      ((p) & (1 << 9))
#define HCC_CFC(p)      ((p) & (1 << 11))
/* db_off bitmask - bits 0:1 reserved */
#define DBOFF_MASK  (~0x3)
/* run_regs_off bitmask - bits 0:4 reserved */
#define RTSOFF_MASK (~0x1f)

#define PORTSC      0
#define PORTPMSC    1
#define PORTLI      2
#define PORTHLPMC   3

/* xhci_op_regs */

/* dev_state bitmasks */
/* USB device address - assigned by the HC */
#define DEV_ADDR_MASK   (0xff)
#define HS_BLOCK    4
struct xhci_transfer_event {
	/* 64-bit buffer address, or immediate data */
	uint64_t    buffer;
	uint32_t    transfer_len;
	/* This field is interpreted differently based on the type of TRB */
	uint32_t    flags;
};

/* Transfer event TRB length bit mask */
/* bits 0:23 */
#define EVENT_TRB_LEN(p)        ((p) & 0xffffff)

/** Transfer Event bit fields **/
#define TRB_TO_EP_ID(p) (((p) >> 16) & 0x1f)

struct xhci_link_trb {
	uint64_t segment_ptr;
	uint32_t intr_target;
	uint32_t control;
};
/* control bitfields */
#define LINK_TOGGLE (0x1 << 1)

/* Command completion event TRB */
struct xhci_event_cmd {
	uint64_t cmd_trb;
	uint32_t status;
	uint32_t flags;
};

/* Address device - disable SetAddress */
#define TRB_BSR     (1<<9)
enum xhci_setup_dev {
	SETUP_CONTEXT_ONLY,
	SETUP_CONTEXT_ADDRESS,
};

/* Stop Endpoint TRB - ep_index to endpoint ID for this TRB */
#define TRB_TO_EP_INDEX(p)      ((((p) & (0x1f << 16)) >> 16) - 1)
#define EP_ID_FOR_TRB(p)        ((((p) + 1) & 0x1f) << 16)

#define SUSPEND_PORT_FOR_TRB(p)     (((p) & 1) << 23)
#define TRB_TO_SUSPEND_PORT(p)      (((p) & (1 << 23)) >> 23)
#define LAST_EP_INDEX           30

/* Set TR Dequeue Pointer command TRB fields, 6.4.3.9 */
#define TRB_TO_STREAM_ID(p)     ((((p) & (0xffff << 16)) >> 16))
#define STREAM_ID_FOR_TRB(p)        ((((p)) & 0xffff) << 16)
#define SCT_FOR_TRB(p)          (((p) << 1) & 0x7)


/* Port Status Change Event TRB fields */
/* Port ID - bits 31:24 */
#define GET_PORT_ID(p)      (((p) & (0xff << 24)) >> 24)

/* Normal TRB fields */
#define TRB_LEN(p)      ((p) & 0x1ffff)
#define TRB_TD_SIZE(p)          (min((p), (uint32_t)31) << 17)
#define TRB_INTR_TARGET(p)  (((p) & 0x3ff) << 22)
#define GET_INTR_TARGET(p)  (((p) >> 22) & 0x3ff)
#define TRB_TBC(p)      (((p) & 0x3) << 7)
#define TRB_TLBPC(p)        (((p) & 0xf) << 16)
#define TRB_CYCLE       (1 << 0)
#define TRB_ENT         (1 << 1)
#define TRB_ISP         (1 << 2)
#define TRB_NO_SNOOP        (1 << 3)
#define TRB_CHAIN       (1 << 4)
#define TRB_IOC         (1 << 5)
#define TRB_IDT         (1 << 6)
#define TRB_BEI         (1 << 9)
#define TRB_TSP         (1 << 9)
#define TRB_DIR_IN      (1 << 16)
#define TRB_TX_TYPE(p)      ((p) << 16)
#define TRB_DATA_OUT        2
#define TRB_DATA_IN     3
#define TRB_SIA         (1 << 31)
#define TRB_FRAME_ID(p)     (((p) & 0x7ff) << 20)

struct xhci_generic_trb {
	uint32_t field[4];
};

union xhci_trb {
	struct xhci_link_trb        link;
	struct xhci_transfer_event  trans_event;
	struct xhci_event_cmd       event_cmd;
	struct xhci_generic_trb     generic;
};

/* command descriptor */
struct xhci_cd {
	struct xhci_command *command;
	union xhci_trb      *cmd_trb;
};

struct xhci_erst {
	struct xhci_erst_entry  *entries;
	unsigned int        num_entries;
	/* xhci->event_ring keeps track of segment dma addresses */
	dma_addr_t      erst_dma_addr;
	/* Num entries the ERST can contain */
	unsigned int        erst_size;
};

tegrabl_error_t xhci_init_bias_pad(void);

tegrabl_error_t xhci_init_usb2_padn(void);

void xhci_init_pinmux(void);

void xhci_vbus_override(void);

void xhci_enable_vbus(void);

void xusbh_xhci_writel(uint32_t reg, uint32_t value);

uint32_t xusbh_xhci_readl(uint32_t reg);

bool xhci_set_root_port(struct xusb_host_context *ctx);

void xhci_release_ss_wakestate_latch(void);

tegrabl_error_t xusbh_load_firmware(void);

void xhci_dump_fw_log(void);

void xhci_dump_padctl(void);
#endif
