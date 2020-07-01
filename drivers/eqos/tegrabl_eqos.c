/*
 * Copyright (c) 2018-2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_EQOS

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_ar_macro.h>
#include <arpadctl_EQOS.h>
#include <arpadctl_CONN.h>
#include <argpio_sw.h>
//#include <arether_qos.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_utils.h>
#include <string.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_malloc.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_phy.h>
#include <tegrabl_phy_marvell.h>
#include <tegrabl_phy_realtek.h>
#include <tegrabl_eqos.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_gpio.h>
#include <lwip/netif.h>
#include <tegrabl_io.h>

extern err_t low_level_input(void);

/***************************************** Macros ***********************************************************/
#define SET										1
#define RESET									0

#define BIT(p)									(1UL << p)

#define SET_BIT(val, p)							((val) | BIT(p))
#define CLEAR_BIT(val, p)						((val) & (~BIT(p)))

#define SET_BIT_FIELD_DEF(fld, def)				((fld##_##def << fld##_##SHIFT) & fld##_##MASK)
#define SET_BIT_FIELD_NUM(fld, num)				((num << fld##_##SHIFT) & fld##_##MASK)
#define GET_BIT_FIELD_DEF(var, reg, fld)		BITFIELD_GET(var, reg##_##fld##_WIDTH, reg##_##fld##_SHIFT)

#define SET_REG_BIT(reg, pos)					NV_WRITE32(reg, NV_READ32(reg) | BIT(reg##_##pos));
#define CLR_REG_BIT(reg, pos)					NV_WRITE32(reg, NV_READ32(reg) & ~(BIT(reg##_##pos)));

#define SET_REG_BITS(reg, set_val)				NV_WRITE32(reg, NV_READ32(reg) | (set_val));
#define CLR_REG_BITS(reg, clr_val)				NV_WRITE32(reg, NV_READ32(reg) & ~(clr_val));

#define SET_REG_BIT_FIELD_DEF(reg, fld, def)	\
			NV_WRITE32(reg, NV_READ32(reg) | SET_BIT_FIELD_DEF(reg##_##fld, def))
#define SET_REG_BIT_FIELD_NUM(reg, fld, num)	\
			NV_WRITE32(reg, (NV_READ32(reg) & ~(reg##_##fld##_##MASK)) | SET_BIT_FIELD_NUM(reg##_##fld, num))

#define GET_REG_FIELD_MASK(reg, fld)			BITFIELD_MASK(reg##_##fld##_##WIDTH, reg##_##fld##_##SHIFT)

/************************************************************************************************************/
#define PADCTL_CONN_SOC_GPIO09_0_TRISTATE				4
#define PADCTL_CONN_SOC_GPIO08_0_TRISTATE				4

#define CACHE_LINE										32
#define ROUNDUP(a, b)									(((a) + ((b) - 1)) & ~((b) - 1))
#define ALIGN_DMA_SIZE(size)							ROUNDUP(size, CACHE_LINE)
#define MAX_PACKET_SIZE									ALIGN_DMA_SIZE(1518)

#define MAX_RD_OUTSTANDING_REQS							2
#define SPEED_1000										2
#define SPEED_100										1
#define SPEED_10										0

#define MDIO_TRANSFER_TIMEOUT_USEC						2000U
#define DMA_RESET_TIMEOUT_USEC							2000U
#define MTL_TXQ_FLUSH_TIMEOUT_USEC						1000U
#define AUTO_CALIB_TIMEOUT_USEC							2000U

#define GPIO_PROP_PHANDLE								0U
#define GPIO_PROP_NUM									1U
#define GPIO_PROP_STATE									2U
#define GPIO_PROP_MAX									3U

/***************************************** MAC registers ****************************************************/
/*
 * MAC configuration register:
 * Establishes the operating mode of the MAC.
 */
#define MAC_CONFIGURATION								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x0)
#define MAC_CONFIGURATION_IPG_WIDTH						((26 - 24) + 1)
#define MAC_CONFIGURATION_IPG_SHIFT						24
#define MAC_CONFIGURATION_IPG_MASK						GET_REG_FIELD_MASK(MAC_CONFIGURATION, IPG)
#define MAC_CONFIGURATION_IPG_40_BIT_TIMES				7
#define MAC_CONFIGURATION_GPSLCE						23
#define MAC_CONFIGURATION_CST							21
#define MAC_CONFIGURATION_ACS							20
#define MAC_CONFIGURATION_WD							19
#define MAC_CONFIGURATION_JD							17
#define MAC_CONFIGURATION_JE							16
#define MAC_CONFIGURATION_PS							15
#define MAC_CONFIGURATION_FES							14
#define MAC_CONFIGURATION_DM							13
#define MAC_CONFIGURATION_LM							12
#define MAC_CONFIGURATION_TE							1
#define MAC_CONFIGURATION_RE							0

/*
 * MAC packet filter register:
 * Contains the filter controls for receiving packets.
 */
#define MAC_PACKET_FILTER								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x8)
#define MAC_PACKET_FILTER_HPF							10
#define MAC_PACKET_FILTER_DBF							5
#define MAC_PACKET_FILTER_PM							4

#define MAC_Q0_TX_FLOW_CTRL								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x70)
#define MAC_Q0_TX_FLOW_CTRL_PT_WIDTH					((31 - 16) + 1)
#define MAC_Q0_TX_FLOW_CTRL_PT_SHIFT					16
#define MAC_Q0_TX_FLOW_CTRL_TFE							1

#define MAC_RX_FLOW_CTRL								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x90)
#define MAC_RX_FLOW_CTRL_RFE							0

#define MAC_TXQ_PRTY_MAP0								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x98)
#define MAC_TXQ_PRTY_MAP0_PSTQ0_WIDTH					((7 - 0) + 1)
#define MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT					0

/*
 * MAC receive queue control 0 register:
 * Controls the queue management in the MAC receiver.
 */
#define MAC_RXQ_CTRL0									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xA0)
#define MAC_RXQ_CTRL0_RXQ0EN_WIDTH						((1 - 0) + 1)
#define MAC_RXQ_CTRL0_RXQ0EN_SHIFT						0
#define MAC_RXQ_CTRL0_RXQ0EN_MASK						GET_REG_FIELD_MASK(MAC_RXQ_CTRL0, RXQ0EN)
#define MAC_RXQ_CTRL0_RXQ0EN_AV_ENABLED					1
#define MAC_RXQ_CTRL0_RXQ0EN_DCB_ENABLED				2

/*
 * MAC receive queue control 1 register:
 * Controls the routing of multicast, broadcast, AV, DCB, and untagged packets to the Rx queues.
 */
#define MAC_RXQ_CTRL1									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xA4)

#define MAC_RXQ_CTRL2									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xA8)
#define MAC_RXQ_CTRL2_PSRQ0_WIDTH						((7 - 0) + 1)
#define MAC_RXQ_CTRL2_PSRQ0_SHIFT						0

#define MAC_US_TIC_COUNTER								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xDC)

#define MAC_ADDR0_HIGH									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x300)
#define MAC_ADDR0_HIGH_ADDRHI_WIDTH						((15 - 0) + 1)
#define MAC_ADDR0_HIGH_ADDRHI_SHIFT						0
#define MAC_ADDR0_HIGH_ADDRHI_MASK						GET_REG_FIELD_MASK(MAC_ADDR0_HIGH, ADDRHI)

#define MAC_ADDR0_LOW									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x304)
#define MAC_TIMESTAMP_CONTROL							(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xB00)

#define MAC_HW_FEATURE1									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x120)
#define MAC_HW_FEATURE1_TXFIFOSIZE_WIDTH				(10 - 6)
#define MAC_HW_FEATURE1_TXFIFOSIZE_SHIFT				6
#define MAC_HW_FEATURE1_RXFIFOSIZE_WIDTH				(4 - 0)
#define MAC_HW_FEATURE1_RXFIFOSIZE_SHIFT				0

#define MAC_MDIO_ADDR									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x200)
#define MAC_MDIO_ADDR_PA_SHIFT							21
#define MAC_MDIO_ADDR_RDA_SHIFT							16
#define MAC_MDIO_ADDR_CR_SHIFT							8
#define MAC_MDIO_ADDR_CR_20_35							2
#define MAC_MDIO_ADDR_GOC_SHIFT							2
#define MAC_MDIO_ADDR_GOC_READ							3
#define MAC_MDIO_ADDR_GOC_WRITE							1
#define MAC_MDIO_ADDR_GB								0

#define MAC_MDIO_DATA									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x204)

/***************************************** MTL registers ****************************************************/
/*
 * MTL operation mode register:
 * Establishes the transmit and receive operating modes and commands.
 */
#define MTL_OPERATION_MODE							(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xC00)
#define MTL_OPERATION_MODE_SCHALG_WIDTH					((6 - 5) + 1)
#define MTL_OPERATION_MODE_SCHALG_SHIFT					5
#define MTL_OPERATION_MODE_SCHALG_MASK					GET_REG_FIELD_MASK(MTL_OPERATION_MODE, SCHALG)
#define MTL_OPERATION_MODE_SCHALG_WRR_ALGO				0
#define MTL_OPERATION_MODE_SCHALG_WFQ_ALGO				1
#define MTL_OPERATION_MODE_SCHALG_DWRR_ALGO				2
#define MTL_OPERATION_MODE_SCHALG_STRICT_PRIORITY_ALGO	3

/*
 * MTL receive queue and DMA channel mapping register
 */
#define MTL_RXQ_DMA_MAP0								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xC30)
#define MTL_RXQ_DMA_MAP0_Q3MDMACH_WIDTH					((27 - 24) + 1)
#define MTL_RXQ_DMA_MAP0_Q3MDMACH_SHIFT					24
#define MTL_RXQ_DMA_MAP0_Q3MDMACH_MASK					GET_REG_FIELD_MASK(MTL_RXQ_DMA_MAP0, Q3MDMACH)
#define MTL_RXQ_DMA_MAP0_Q2MDMACH_WIDTH					((19 - 16) + 1)
#define MTL_RXQ_DMA_MAP0_Q2MDMACH_SHIFT					16
#define MTL_RXQ_DMA_MAP0_Q2MDMACH_MASK					GET_REG_FIELD_MASK(MTL_RXQ_DMA_MAP0, Q2MDMACH)
#define MTL_RXQ_DMA_MAP0_Q1MDMACH_WIDTH					((11 - 8) + 1)
#define MTL_RXQ_DMA_MAP0_Q1MDMACH_SHIFT					8
#define MTL_RXQ_DMA_MAP0_Q1MDMACH_MASK					GET_REG_FIELD_MASK(MTL_RXQ_DMA_MAP0, Q1MDMACH)
#define MTL_RXQ_DMA_MAP0_Q0MDMACH_WIDTH					((3 - 0) + 1)
#define MTL_RXQ_DMA_MAP0_Q0MDMACH_SHIFT					0
#define MTL_RXQ_DMA_MAP0_Q0MDMACH_MASK					GET_REG_FIELD_MASK(MTL_RXQ_DMA_MAP0, Q0MDMACH)
#define MTL_RXQ_DMA_MAP0_Q3MDMACH_DMA_CHANNEL_3			3
#define MTL_RXQ_DMA_MAP0_Q2MDMACH_DMA_CHANNEL_2			2
#define MTL_RXQ_DMA_MAP0_Q1MDMACH_DMA_CHANNEL_1			1
#define MTL_RXQ_DMA_MAP0_Q0MDMACH_DMA_CHANNEL_0			0

/*
 * MTL queue 0 transmit operation mode register:
 * Establishes the transmit queue operating modes and registers.
 */
#define MTL_TXQ0_OPERATION_MODE							(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xD00)
#define MTL_TXQ0_OPERATION_MODE_TQS_WIDTH				((31 - 16) + 1)
#define MTL_TXQ0_OPERATION_MODE_TQS_SHIFT				16
#define MTL_TXQ0_OPERATION_MODE_TQS_MASK				GET_REG_FIELD_MASK(MTL_TXQ0_OPERATION_MODE, TQS)
#define MTL_TXQ0_OPERATION_MODE_TXQEN_WIDTH				((3 - 2) + 1)
#define MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT				2
#define MTL_TXQ0_OPERATION_MODE_TXQEN_MASK				GET_REG_FIELD_MASK(MTL_TXQ0_OPERATION_MODE, TXQEN)
#define MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED			0x2
#define MTL_TXQ0_OPERATION_MODE_TSF						1
#define MTL_TXQ0_OPERATION_MODE_FTQ						0

#define MTL_TXQ0_QUANTUM_WEIGHT							(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xD18)

#define MTL_RXQ0_OPERATION_MODE							(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0xD30)
#define MTL_RXQ0_OPERATION_MODE_RQS_WIDTH				((31 - 20) + 1)
#define MTL_RXQ0_OPERATION_MODE_RQS_SHIFT				20
#define MTL_RXQ0_OPERATION_MODE_RQS_MASK				GET_REG_FIELD_MASK(MTL_RXQ0_OPERATION_MODE, RQS)
#define MTL_RXQ0_OPERATION_MODE_RFD_WIDTH				((19 - 14) + 1)
#define MTL_RXQ0_OPERATION_MODE_RFD_SHIFT				14
#define MTL_RXQ0_OPERATION_MODE_RFA_WIDTH				((13 - 8) + 1)
#define MTL_RXQ0_OPERATION_MODE_RFA_SHIFT				8
#define MTL_RXQ0_OPERATION_MODE_EHFC					7
#define MTL_RXQ0_OPERATION_MODE_RSF						5

/***************************************** DMA registers ****************************************************/
/*
 * DMA bus mode register;
 * Establishes the bus operating modes for the DMA
 */
#define DMA_MODE										(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1000)
#define DMA_MODE_SWR									0

/*
 * DMA system bus mode regitser:
 * Controls the behaviour of the AHB or AXI master. It mainly controls burst splitting and number of
 * outstanding requests.
 */
#define DMA_SYSBUS_MODE									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1004)
#define DMA_SYSBUS_MODE_WR_OSR_LMT_WIDTH				((29 - 24) + 1)
#define DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT				24
#define DMA_SYSBUS_MODE_WR_OSR_LMT_MASK					GET_REG_FIELD_MASK(DMA_SYSBUS_MODE, WR_OSR_LMT)
#define DMA_SYSBUS_MODE_RD_OSR_LMT_WIDTH				((23 - 16) + 1)
#define DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT				16
#define DMA_SYSBUS_MODE_RD_OSR_LMT_MASK					GET_REG_FIELD_MASK(DMA_SYSBUS_MODE, RD_OSR_LMT)
#define DMA_SYSBUS_MODE_EAME							11
#define DMA_SYSBUS_MODE_BLEN256							7
#define DMA_SYSBUS_MODE_BLEN16							3
#define DMA_SYSBUS_MODE_BLEN8							2
#define DMA_SYSBUS_MODE_BLEN4							1

#define DMA_DEBUG_STATUS0								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x100C)

#define DMA_CH0_CONTROL									(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1100)
#define DMA_CH0_CONTROL_PBLX8							16

#define DMA_CH0_TX_CONTROL								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1104)
#define DMA_CH0_TX_CONTROL_TXPBL_WIDTH					((21 - 16) + 1)
#define DMA_CH0_TX_CONTROL_TXPBL_SHIFT					16
#define DMA_CH0_TX_CONTROL_TXPBL_MASK					GET_REG_FIELD_MASK(DMA_CH0_TX_CONTROL, TXPBL)
#define DMA_CH0_TX_CONTROL_OSP							4
#define DMA_CH0_TX_CONTROL_ST							0

#define DMA_CH0_RX_CONTROL								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1108)
#define DMA_CH0_RX_CONTROL_RXPBL_WIDTH					((21 - 16) + 1)
#define DMA_CH0_RX_CONTROL_RXPBL_SHIFT					16
#define DMA_CH0_RX_CONTROL_RXPBL_MASK					GET_REG_FIELD_MASK(DMA_CH0_RX_CONTROL, RXPBL)
#define DMA_CH0_RX_CONTROL_RBSZ_WIDTH					((14 - 1) + 1)
#define DMA_CH0_RX_CONTROL_RBSZ_SHIFT					1
#define DMA_CH0_RX_CONTROL_RBSZ_MASK					GET_REG_FIELD_MASK(DMA_CH0_RX_CONTROL, RBSZ)
#define DMA_CH0_RX_CONTROL_SR							0

#define DMA_CH0_TXDESC_LIST_HIGH_ADDR				(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1110)
#define DMA_CH0_TXDESC_LIST_ADDR					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1114)
#define DMA_CH0_RXDESC_LIST_HIGH_ADDR				(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1118)
#define DMA_CH0_RXDESC_LIST_ADDR					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x111C)

#define DMA_CH0_TXDESC_TAIL_POINTER					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1120)
#define DMA_CH0_RXDESC_TAIL_POINTER					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1128)

#define DMA_CH0_TXDESC_RING_LENGTH					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x112C)
#define DMA_CH0_RXDESC_RING_LENGTH					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1130)

#define DMA_CH0_INTERRUPT_ENABLE					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1134)
#define DMA_CH0_INTERRUPT_ENABLE_NIE					15
#define DMA_CH0_INTERRUPT_ENABLE_RIE					6

#define DMA_CH0_CURR_APP_TXDESC						(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1144)
#define DMA_CH0_CURR_APP_RXDESC						(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x114C)

#define DMA_CH0_CURR_APP_TXBUFFER_H					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1150)
#define DMA_CH0_CURR_APP_TXBUFFER					(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1154)

#define DMA_CH0_STATUS								(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x1160)
#define DMA_CH0_STATUS_RI								6U
#define DMA_CH0_STATUS_TI								0U

/***************************************** Other registers **************************************************/
#define REG_ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0				(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x8604)
#define ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0_RX				1
#define ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0_RX_RANGE		1:1
#define ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0_RX_SW_CLEAR		1
#define ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0_TX				0

#define REG_ETHER_QOS_AUTO_CAL_CONFIG_0						(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x8804)
#define ETHER_QOS_AUTO_CAL_CONFIG_0_AUTO_CAL_START			31
#define ETHER_QOS_AUTO_CAL_CONFIG_0_AUTO_CAL_ENABLE			29

#define REG_ETHER_QOS_AUTO_CAL_STATUS_0						(NV_ADDRESS_MAP_ETHER_QOS_BASE + 0x880C)
#define ETHER_QOS_AUTO_CAL_STATUS_0_AUTO_CAL_ACTIVE			31

#define REG_INTR_CHANNEL0_SLICE5_IEP_CLASS_0				0x154
#define REG_INTR_CHANNEL0_SLICE6_IEP_CLASS_0				0x194

/***************************************** PHY registers ****************************************************/
#define REG_PHY_PAGE										22

/************************************************************************************************************/

#define EQOS_DEBUG			0

#if (EQOS_DEBUG)
#undef pr_trace
#define pr_trace pr_info
#endif

/************************************************************************************************************/

#define DESCRIPTORS_TX			4
#define DESCRIPTORS_RX			4
#define DESCRIPTORS_NUM			(DESCRIPTORS_TX + DESCRIPTORS_RX)
#define DESCRIPTOR_SIZE			sizeof(struct eqos_desc)
#define DESCRIPTORS_NUM			(DESCRIPTORS_TX + DESCRIPTORS_RX)
#define TX_DESCRIPTORS_SIZE		(DESCRIPTORS_TX * DESCRIPTOR_SIZE)
#define RX_DESCRIPTORS_SIZE		(DESCRIPTORS_RX * DESCRIPTOR_SIZE)
#define DESCRIPTORS_SIZE		(DESCRIPTORS_NUM * DESCRIPTOR_SIZE)

#define TDES3_OWN_DMA					BIT(31)
#define TDES3_FD						BIT(29)
#define TDES3_LD						BIT(28)
/* Bits 27 and 26 should be 0 for automatically appending padding and CRC to a packet (>= 60 bytes) */
#define TDES3_CPC_INSERT_CRC_AND_PAD	0

#define RDES3_OWN_DMA					BIT(31)
#define RDES3_IOC						BIT(30)
#define RDES3_BUF1V						BIT(24)

#define RX_BUFFER_SIZE			(DESCRIPTORS_RX * MAX_PACKET_SIZE)

#define EQOS_GET_BIT(val, pos)		BITFIELD_GET(val, 1, pos)

#define CALIBRATE_EQOS_PAD(reg)																	\
	do {																						\
		val = NV_READ32(NV_ADDRESS_MAP_PADCTL_A21_BASE + PADCTL_EQOS_EQOS_##reg##_0);			\
		val = NV_FLD_SET_DRF_DEF(PADCTL_EQOS, EQOS_##reg, IO_RESET, NORMAL, val);				\
		val = NV_FLD_SET_DRF_DEF(PADCTL_EQOS, EQOS_##reg, DRV_TYPE, COMP_DRIVE_2X, val);		\
		val = NV_FLD_SET_DRF_DEF(PADCTL_EQOS, EQOS_##reg, GPIO_SF_SEL, SFIO, val);				\
		val = NV_FLD_SET_DRF_DEF(PADCTL_EQOS, EQOS_##reg, E_INPUT, ENABLE, val);				\
		val = NV_FLD_SET_DRF_DEF(PADCTL_EQOS, EQOS_##reg, TRISTATE, PASSTHROUGH, val);			\
		if (PADCTL_EQOS_EQOS_##reg##_0 == PADCTL_EQOS_EQOS_SMA_MDIO_0) {						\
			val = NV_FLD_SET_DRF_DEF(PADCTL_EQOS, EQOS_##reg, PUPD, PULL_UP, val);				\
		}																						\
		NV_WRITE32(NV_ADDRESS_MAP_PADCTL_A21_BASE + PADCTL_EQOS_EQOS_##reg##_0, val);			\
	} while (0)
/************************************************************************************************************/

struct eqos_desc {
	uint32_t des0;
	uint32_t des1;
	uint32_t des2;
	uint32_t des3;
};

struct eqos_dev {
	struct eqos_desc *tx_descs;
	struct eqos_desc *rx_descs;
	uint32_t tx_desc_id;
	uint32_t rx_desc_id;
	void *tx_dma_buf[DESCRIPTORS_TX];
	void *rx_dma_buf[DESCRIPTORS_RX];
	uint32_t tx_fifo_sz_bytes;
	struct phy_dev phy;
};

static struct eqos_dev eqos = {0};
static void print_reg(char *str, uint32_t addr)
{
#if EQOS_DEBUG
	uint32_t val;
	val = NV_READ32(addr);
	pr_info("---> %s: addr: 0x%08x, val: 0x%08x\n", str, addr, val);
#endif
}

#define DEBUG_DMA		(1 << 0)
#define DEBUG_TX_DESC	(1 << 1)
#define DEBUG_RX_DESC	(1 << 2)
#define DEBUG_ALL		(0xFF)

#if EQOS_DEBUG
static void print_debug_registers(uint32_t flags)
{
	struct eqos_desc *desc = NULL;
	uint32_t i =0;

	pr_info("\n");
	pr_info("<--------------------- Debug registers --------------------->\n");

	if ((flags & DEBUG_DMA) != 0) {
		print_reg("DMA: ch0 status   ", DMA_CH0_STATUS);
		print_reg("DMA: debug status ", DMA_DEBUG_STATUS0);
		print_reg("EQoS: virtual intr", REG_ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0);
	}

	/* print Tx descriptors */
	if ((flags & DEBUG_TX_DESC) != 0) {

		pr_info("\n");
		print_reg("Tx desc: base addr", DMA_CH0_TXDESC_LIST_ADDR);
		print_reg("Tx desc: tail ptr ", DMA_CH0_TXDESC_TAIL_POINTER);
		pr_info("\n");
		print_reg("Curr App: Tx desc ", DMA_CH0_CURR_APP_TXDESC);
		print_reg("Curr App: Tx buff ", DMA_CH0_CURR_APP_TXBUFFER);

		for (i = 0; i < 4; i++) {
			pr_info("\n");
			pr_info("Tx desc %u\n", i);
			desc = &(eqos.tx_descs[i]);
			tegrabl_dma_unmap_buffer(TEGRABL_MODULE_EQOS, 0, (void *)desc, DESCRIPTOR_SIZE,
									 TEGRABL_DMA_FROM_DEVICE);
			print_reg("Tx desc: des0     ", (uint32_t)(uintptr_t)&desc->des0);
			print_reg("Tx desc: des3     ", (uint32_t)(uintptr_t)&desc->des3);
		}
	}

	/* print Rx descriptors */
	if ((flags & DEBUG_RX_DESC) != 0) {

		pr_info("\n");
		print_reg("Rx desc: base addr", 0x02490000 + 0x111C);
		print_reg("Rx desc: tail ptr ", DMA_CH0_RXDESC_TAIL_POINTER);
		pr_info("\n");
		print_reg("Curr App: Rx desc ", 0x02490000 + 0x114C);
		print_reg("Curr App: Rx buff ", 0x02490000 + 0x115C);

		for (i = 0; i < DESCRIPTORS_RX; i++) {
			pr_info("\n");
			pr_info("Rx desc %u\n", i);
			desc = &(eqos.rx_descs[i]);
			tegrabl_dma_unmap_buffer(TEGRABL_MODULE_EQOS, 0, (void *)desc, DESCRIPTOR_SIZE,
									 TEGRABL_DMA_FROM_DEVICE);
			print_reg("RX desc: des0     ", (uint32_t)(uintptr_t)&desc->des0);
			print_reg("RX desc: des3     ", (uint32_t)(uintptr_t)&desc->des3);
		}
	}

	pr_info("<------------------------- End  ---------------------------->\n\n");
}
#endif

void print_buffer(void *buffer, size_t len, char *str)
{
#if EQOS_DEBUG
	uint8_t *cp = NULL;
	size_t i;

	pr_info("\n\n");

	cp = (uint8_t *)buffer;
	pr_info("------------------ %s ------------------\n", str);

	for (i = 1; i <= len; i++) {
		if (i == len) {
			tegrabl_printf("%02x\n\n", *cp);
		} else if ((i % 30) == 0) {
			tegrabl_printf("%02x\n", *cp);
		} else {
			tegrabl_printf("%02x ", *cp);
		}
		++cp;
	}
#endif
}


static tegrabl_error_t wait_for_bit(uint32_t addr, uint32_t pos, bool set, uint32_t timeout_us)
{
	uint32_t val;
	time_t elapsed_time_us;
	time_t start_time_us;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	elapsed_time_us = 0;
	start_time_us = tegrabl_get_timestamp_us();

	while (true) {

		elapsed_time_us = tegrabl_get_timestamp_us() - start_time_us;
		if (elapsed_time_us > timeout_us) {
			pr_error("timeout\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}

		val = NV_READ32(addr);
		if ((!!(val & BIT(pos))) == set) {
			break;
		}
		tegrabl_udelay(10);
	}

fail:
	return err;
}

static void tegrabl_eqos_mdio_write(uint32_t phy_addr, uint32_t phy_reg, uint32_t data,
									uint32_t csr_clock_range)
{
	uint32_t mdio_addr_val;

	/* Write data into MDIO data/buffer register */
	NV_WRITE32(MAC_MDIO_DATA, data);

	/* Prepare MDIO address register */
	mdio_addr_val = (phy_addr					<<	MAC_MDIO_ADDR_PA_SHIFT)	 |
					(phy_reg					<<	MAC_MDIO_ADDR_RDA_SHIFT) |
					(csr_clock_range			<<	MAC_MDIO_ADDR_CR_SHIFT)	 |
					(MAC_MDIO_ADDR_GOC_WRITE	<<	MAC_MDIO_ADDR_GOC_SHIFT);
	/* Start MDIO transfer */
	mdio_addr_val = mdio_addr_val | BIT(MAC_MDIO_ADDR_GB);
	NV_WRITE32(MAC_MDIO_ADDR, mdio_addr_val);

	/* Wait for MDIO transfer/operation to complete */
	wait_for_bit(MAC_MDIO_ADDR, MAC_MDIO_ADDR_GB, RESET, MDIO_TRANSFER_TIMEOUT_USEC);

	pr_trace("phy addr: %u, phy reg: %u, MDIO addr reg: 0x%08x, MDIO data reg: 0x%08x\n",
			 phy_addr, phy_reg, mdio_addr_val, data);
}

static uint16_t tegrabl_eqos_mdio_read(uint32_t phy_addr, uint32_t phy_reg, uint32_t csr_clock_range)
{
	uint32_t mdio_addr_val;
	uint16_t mdio_data_val;
	uint32_t val;

	/* Prepare MDIO address register */
	mdio_addr_val = (phy_addr				<<	MAC_MDIO_ADDR_PA_SHIFT)	 |
					(phy_reg				<<	MAC_MDIO_ADDR_RDA_SHIFT) |
					(csr_clock_range		<<	MAC_MDIO_ADDR_CR_SHIFT)	 |
					(MAC_MDIO_ADDR_GOC_READ	<<	MAC_MDIO_ADDR_GOC_SHIFT);
	/* Start MDIO transfer */
	mdio_addr_val = mdio_addr_val | BIT(MAC_MDIO_ADDR_GB);
	NV_WRITE32(MAC_MDIO_ADDR, mdio_addr_val);

	/* Wait for MDIO transfer/operation to complete */
	wait_for_bit(MAC_MDIO_ADDR, MAC_MDIO_ADDR_GB, RESET, MDIO_TRANSFER_TIMEOUT_USEC);

	/* Read the data which transferred from PHY */
	val = NV_READ32(MAC_MDIO_DATA);
	mdio_data_val = (uint16_t)(val & 0xFFFF);

	pr_trace("phy addr: %u, phy reg: %u, MDIO addr reg: 0x%08x, MDIO data reg: 0x%08x\n",
			 phy_addr, phy_reg, mdio_addr_val, mdio_data_val);

	return mdio_data_val;
}

static uint16_t tegrabl_eqos_phy_read(uint32_t phy_addr, uint32_t page, uint32_t reg)
{
	uint16_t val;
	uint32_t csr_clock_range;

	pr_trace("MDIO  Read: phy page: %u, reg: %u\n", page, reg);

	/* TODO: remove hardcoding */
	csr_clock_range = 4; /* axi_cbb clk rate is 204 Mhz so the value is 4 */

	if (eqos.phy.curr_page != page) {
		pr_trace("Set phy page: 0x%08x\n", page);
		tegrabl_eqos_mdio_write(phy_addr, eqos.phy.page_sel_reg, page, csr_clock_range);
		eqos.phy.curr_page = page;
	}

	val = tegrabl_eqos_mdio_read(phy_addr, reg, csr_clock_range);
	pr_trace("\n");

	return val;
}

static void tegrabl_eqos_phy_write(uint32_t phy_addr, uint32_t page, uint32_t reg, uint32_t data)
{
	uint32_t csr_clock_range;

	pr_trace("MDIO Write: phy page: %u, reg: %u, data: 0x%08x\n", page, reg, data);

	/* TODO: remove hardcoding */
	csr_clock_range = 4; /* axi_cbb clk rate is 204 Mhz so the value is 4 */

	if (eqos.phy.curr_page != page) {
		pr_trace("Set phy page: 0x%08x\n", page);
		tegrabl_eqos_mdio_write(phy_addr, eqos.phy.page_sel_reg, page, csr_clock_range);
		tegrabl_mdelay(20);
		eqos.phy.curr_page = page;
	}

	tegrabl_eqos_mdio_write(phy_addr, reg, data, csr_clock_range);
	pr_trace("\n");
}

static tegrabl_error_t tegrabl_eqos_program_pll(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	err = tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_PLLE, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to init plle clk\n");
		goto fail;
	}
	err = tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_PLLREFE, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable pllrefe\n");
		goto fail;
	}
	err = tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_UTMI_PLL, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable utmi pll\n");
		goto fail;
	}

fail:
	tegrabl_mdelay(1000);
	return err;
}

static tegrabl_error_t tegrabl_eqos_enable_clks(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	err = tegrabl_car_rst_set(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_RST);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to set EQoS: rst\n");
		goto fail;
	}
	err = tegrabl_car_clk_enable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_AXI, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable EQoS: axi clk\n");
		goto fail;
	}
	err = tegrabl_car_clk_enable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_PTP_REF, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable EQoS: ptp ref clk\n");
		goto fail;
	}
	err = tegrabl_car_clk_enable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_RX, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable EQoS: rx clk\n");
		goto fail;
	}
	err = tegrabl_car_clk_enable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_TX, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable EQoS: tx clk\n");
		goto fail;
	}
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_RST);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to clear EQoS: rst\n");
		goto fail;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_AXI_CBB, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to enable EQoS: axi_cbb clk\n");
		goto fail;
	}

fail:
	return err;
}

static void tegrabl_eqos_disable_clks(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	err = tegrabl_car_clk_disable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_AXI);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to disable EQoS: axi clk\n");
	}
	err = tegrabl_car_clk_disable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_PTP_REF);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to disable EQoS: ptp ref clk\n");
	}
	err = tegrabl_car_clk_disable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_RX);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to disable EQoS: rx clk\n");
	}
	err = tegrabl_car_clk_disable(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_TX);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to disable EQoS: tx clk\n");
	}
	err = tegrabl_car_clk_disable(TEGRABL_MODULE_AXI_CBB, 0);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to disable EQoS: axi_cbb clk\n");
	}
}

static void tegrabl_calibrate_pads(void)
{
	uint32_t val;

	pr_trace("%s()\n", __func__);

	/* PAD settings for EQoS */
	CALIBRATE_EQOS_PAD(TXC);
	CALIBRATE_EQOS_PAD(RD0);
	CALIBRATE_EQOS_PAD(RD1);
	CALIBRATE_EQOS_PAD(RD2);
	CALIBRATE_EQOS_PAD(RD3);
	CALIBRATE_EQOS_PAD(RXC);
	CALIBRATE_EQOS_PAD(RX_CTL);
	CALIBRATE_EQOS_PAD(TD0);
	CALIBRATE_EQOS_PAD(TD1);
	CALIBRATE_EQOS_PAD(TD2);
	CALIBRATE_EQOS_PAD(TD3);
	CALIBRATE_EQOS_PAD(SMA_MDIO);
	CALIBRATE_EQOS_PAD(SMA_MDC);

	/* PAD settings for PHY */
	val = NV_READ32(NV_ADDRESS_MAP_PADCTL_A4_BASE + PADCTL_CONN_SOC_GPIO09_0);
	val = CLEAR_BIT(val, PADCTL_CONN_SOC_GPIO09_0_TRISTATE);
	NV_WRITE32(NV_ADDRESS_MAP_PADCTL_A4_BASE + PADCTL_CONN_SOC_GPIO09_0, val);

	val = NV_READ32(NV_ADDRESS_MAP_PADCTL_A4_BASE + PADCTL_CONN_SOC_GPIO08_0);
	val = CLEAR_BIT(val, PADCTL_CONN_SOC_GPIO08_0_TRISTATE);
	NV_WRITE32(NV_ADDRESS_MAP_PADCTL_A4_BASE + PADCTL_CONN_SOC_GPIO08_0, val);
}

static tegrabl_error_t tegrabl_eqos_reset_phy(void)
{
	void *fdt = NULL;
	int32_t offset;
	uint32_t gpio_chip_id;
	uint8_t state;
	uint32_t property[GPIO_PROP_MAX] = {0};
	struct gpio_driver *gpio_drv = NULL;
	const uint32_t *temp;
	uint32_t delay;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	/* Get reset gpio from dtb */
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get dtb handle\n");
		goto fail;
	}
	offset = fdt_node_offset_by_compatible(fdt, -1, "nvidia,eqos");
	if (offset < 0) {
		pr_error("Failed to find eqos node in dtb\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}
	err = tegrabl_dt_get_prop_u32_array(fdt, offset, "nvidia,phy-reset-gpio", GPIO_PROP_MAX, property, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get gpio_num val of phy-reset-gpio property from dtb\n");
		goto fail;
	}

	/* Configure reset gpio as output */
	err = tegrabl_gpio_get_chipid_with_phandle(property[GPIO_PROP_PHANDLE], &gpio_chip_id);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get chip id from gpio_phandle\n");
		goto fail;
	}
	err = tegrabl_gpio_driver_get(gpio_chip_id, &gpio_drv);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get gpio driver handle\n");
		goto fail;
	}
	err = gpio_config(gpio_drv, property[GPIO_PROP_NUM], GPIO_PINMODE_OUTPUT);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to configure gpio mode\n");
		goto fail;
	}

	/*
	 * Reset PHY
	 * active high 1->0
	 * in-between delay (if given in dtb)
	 * active low  0->1
	 * post delay (if given in dtb)
	 */
	state = !(GPIO_PIN_STATE_HIGH ^ property[GPIO_PROP_STATE]);
	err = gpio_write(gpio_drv, property[GPIO_PROP_NUM], state);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to configure gpio state to %u\n", state);
		goto fail;
	}
	/* Add in-between reset delay */
	temp = fdt_getprop(fdt, offset, "nvidia,phy-reset-duration", NULL);
	if (temp != NULL) {
		delay = fdt32_to_cpu(*temp);
		pr_trace("in-between reset delay = %u\n", delay);
		tegrabl_udelay(delay);
	} else {
		pr_trace("Failed to get property \"nvidia,phy-reset-duration\"\n");
	}
	err = gpio_write(gpio_drv, property[GPIO_PROP_NUM], !state);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to configure gpio state to %u\n", !state);
		goto fail;
	}
	/* Add post reset delay */
	temp = fdt_getprop(fdt, offset, "nvidia,phy-reset-post-delay", NULL);
	if (temp != NULL) {
		delay = fdt32_to_cpu(*temp);
		pr_trace("post delay = %u\n", delay);
		tegrabl_mdelay(delay);
	} else {
		pr_trace("Failed to get property \"nvidia,phy-reset-post-delay\"\n");
	}

fail:
	return err;
}

static tegrabl_error_t tegrabl_eqos_get_phy_addr(uint32_t *phy_mdio_addr)
{
	void *fdt = NULL;
	int32_t offset;
	const uint32_t *temp;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get dtb handle\n");
		goto fail;
	}
	offset = fdt_node_offset_by_compatible(fdt, -1, "nvidia,eqos-mdio");
	if (offset < 0) {
		pr_error("Failed to find eqos-mdio node in dtb\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}
	offset = fdt_subnode_offset(fdt, offset, "ethernet-phy");
	if (offset < 0) {
		pr_error("ethernet-phy subnode not found\n");
		goto fail;
	}
	temp = fdt_getprop(fdt, offset, "reg", NULL);
	if (temp == NULL) {
		pr_error("Failed to get \"ethernet-phy\" property \"reg\"\n");
		goto fail;
	}
	*phy_mdio_addr = fdt32_to_cpu(*temp);
	pr_trace("phy mdio addr = %u\n", *phy_mdio_addr);

fail:
	return err;
}

static tegrabl_error_t tegrabl_eqos_auto_calib(void)
{
	uint32_t val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	val = NV_READ32(REG_ETHER_QOS_AUTO_CAL_CONFIG_0);
	val = val												|
		  BIT(ETHER_QOS_AUTO_CAL_CONFIG_0_AUTO_CAL_START)	|
		  BIT(ETHER_QOS_AUTO_CAL_CONFIG_0_AUTO_CAL_ENABLE);
	NV_WRITE32(REG_ETHER_QOS_AUTO_CAL_CONFIG_0, val);
	tegrabl_mdelay(1000);

	pr_info("Wait till auto-calibration completes...\n");
	err = wait_for_bit(REG_ETHER_QOS_AUTO_CAL_STATUS_0, ETHER_QOS_AUTO_CAL_STATUS_0_AUTO_CAL_ACTIVE, RESET,
					   AUTO_CALIB_TIMEOUT_USEC);

	return err;
}

static tegrabl_error_t tegrabl_eqos_alloc_resources(void)
{
	uint32_t i;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	eqos.tx_descs = tegrabl_alloc_align(TEGRABL_HEAP_DMA, CACHE_LINE, TX_DESCRIPTORS_SIZE);
	if (eqos.tx_descs == NULL) {
		pr_error("Failed to alloc memory for desciptors\n");
		goto done;
	}
	memset(eqos.tx_descs, 0, TX_DESCRIPTORS_SIZE);

	eqos.rx_descs = tegrabl_alloc_align(TEGRABL_HEAP_DMA, CACHE_LINE, RX_DESCRIPTORS_SIZE);
	if (eqos.rx_descs == NULL) {
		pr_error("Failed to alloc memory for desciptors\n");
		goto fail_free_tx_descs;
	}
	memset(eqos.rx_descs, 0, RX_DESCRIPTORS_SIZE);

	for (i = 0; i < DESCRIPTORS_TX; i++) {
		eqos.tx_dma_buf[i] = tegrabl_alloc_align(TEGRABL_HEAP_DMA, CACHE_LINE, MAX_PACKET_SIZE);
		if (eqos.tx_dma_buf[i] == NULL) {
			pr_error("Failed to alloc memory for Tx buffer: %u\n", i);
			goto fail_free_rx_descs;
		}
	}

	for (i = 0; i < DESCRIPTORS_RX; i++) {
		eqos.rx_dma_buf[i] = tegrabl_alloc_align(TEGRABL_HEAP_DMA, CACHE_LINE, MAX_PACKET_SIZE);
		if (eqos.rx_dma_buf[i] == NULL) {
			pr_error("Failed to alloc memory for Rx buffer: %u\n", i);
			goto fail_free_tx_dma_buf;
		}
	}

	pr_trace("tx descs addr: %p\n", eqos.tx_descs);
	pr_trace("rx descs addr: %p\n", eqos.rx_descs);
	pr_trace("tx buf addr  :\n");
	for (i = 0; i < DESCRIPTORS_TX; i++) {
		pr_trace("%p\n", eqos.tx_dma_buf[i]);
	}
	pr_trace("rx buf addr  :\n");
	for (i = 0; i < DESCRIPTORS_RX; i++) {
		pr_trace("%p\n", eqos.rx_dma_buf[i]);
	}

	goto done;

fail_free_tx_dma_buf:
	for (i = 0; i < DESCRIPTORS_TX; i++) {
		tegrabl_free(eqos.tx_dma_buf[i]);
	}
fail_free_rx_descs:
	tegrabl_free(eqos.rx_descs);
fail_free_tx_descs:
	tegrabl_free(eqos.tx_descs);
done:
	return err;
}

static tegrabl_error_t tegrabl_eqos_adjust_link(void)
{
	uint32_t clk_rate_khz = 0;
	uint32_t val = 0;
	uint32_t mac_config;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	pr_trace("speed = %u Mbps\n", eqos.phy.speed);
	pr_trace("duplex mode = %s\n", eqos.phy.duplex_mode ? "full-duplex" : "half-duplex");

	/* Ethernet line speed:
	 * bits MAC_CONFIGURATION_PS, MAC_CONFIGURATION_FES
	 * 00: 1000 Mbps
	 * 01:  2.5 Gbps
	 * 10:   10 Mbps
	 * 11:  100 Mbps
	 */
	mac_config = 0;
	if (eqos.phy.speed == 1000) {
		clk_rate_khz = 125 * 1000;
		mac_config = 0;
	} else if (eqos.phy.speed == 100) {
		clk_rate_khz = 25 * 1000;
		mac_config = SET_BIT(mac_config, MAC_CONFIGURATION_PS);
		mac_config = SET_BIT(mac_config, MAC_CONFIGURATION_FES);
	} else if (eqos.phy.speed == 10) {
		clk_rate_khz = 2.5 * 1000;
		mac_config = SET_BIT(mac_config, MAC_CONFIGURATION_PS);
	}

	/* Set speed and duplex mode */
	if (eqos.phy.duplex_mode == 1) {
		mac_config = SET_BIT(mac_config, MAC_CONFIGURATION_DM);
	}
	NV_WRITE32(MAC_CONFIGURATION, mac_config);

	/* Set Tx clock rate */
	err = tegrabl_car_set_clk_rate(TEGRABL_MODULE_EQOS, TEGRABL_CLK_EQOS_TX, clk_rate_khz, &val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to set tx clk rate\n");
		goto fail;
	}
	pr_trace("tx clk set to %u khz\n", val);

fail:
	return err;
}

static tegrabl_error_t tegrabl_eqos_dma_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	/* Reset DMA */
	SET_REG_BIT(DMA_MODE, SWR);
	/* Wait till DMA reset completes */
	err = wait_for_bit(DMA_MODE, DMA_MODE_SWR, RESET, DMA_RESET_TIMEOUT_USEC);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Program AXI interface burst length and read, write outstanding request limit */
	SET_REG_BITS(DMA_SYSBUS_MODE,
				 BIT(DMA_SYSBUS_MODE_EAME)							|
				 BIT(DMA_SYSBUS_MODE_BLEN16)						|
				 BIT(DMA_SYSBUS_MODE_BLEN8)							|
				 BIT(DMA_SYSBUS_MODE_BLEN256)						|
				 SET_BIT_FIELD_NUM(DMA_SYSBUS_MODE_WR_OSR_LMT, 0xF)	| /* TODO: add reason for harcoding */
				 SET_BIT_FIELD_NUM(DMA_SYSBUS_MODE_RD_OSR_LMT, 0xF));

	/* Set receive buffer size */
	SET_REG_BIT_FIELD_NUM(DMA_CH0_RX_CONTROL, RBSZ, MAX_PACKET_SIZE);

	/* Enable OSP mode */
	SET_REG_BIT(DMA_CH0_TX_CONTROL, OSP);

	SET_REG_BIT_FIELD_NUM(DMA_CH0_TX_CONTROL, TXPBL, 32); /* TODO: add reason for harcoding */
	SET_REG_BIT_FIELD_NUM(DMA_CH0_RX_CONTROL, RXPBL, 8);  /* TODO: add reason for harcoding */

fail:
	return err;
}

static tegrabl_error_t tegrabl_eqos_mtl_init(void)
{
	uint32_t val;
	uint32_t tx_fifo_sz;
	uint32_t rx_fifo_sz;
	uint32_t rx_fifo_sz_bytes;
	uint32_t tqs;
	uint32_t rqs;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s()\n", __func__);

	/* Tx scheduling algorithm */
	SET_REG_BIT_FIELD_DEF(MTL_OPERATION_MODE, SCHALG, STRICT_PRIORITY_ALGO);

	/* Rx queue and DMA channel mapping */
	SET_REG_BITS(MTL_RXQ_DMA_MAP0,
				 SET_BIT_FIELD_DEF(MTL_RXQ_DMA_MAP0_Q0MDMACH, DMA_CHANNEL_0) |
				 SET_BIT_FIELD_DEF(MTL_RXQ_DMA_MAP0_Q1MDMACH, DMA_CHANNEL_1) |
				 SET_BIT_FIELD_DEF(MTL_RXQ_DMA_MAP0_Q2MDMACH, DMA_CHANNEL_2) |
				 SET_BIT_FIELD_DEF(MTL_RXQ_DMA_MAP0_Q3MDMACH, DMA_CHANNEL_3));

	/* Flush transmit queue */
	SET_REG_BIT(MTL_TXQ0_OPERATION_MODE, FTQ);
	/* Wait till flush completes */
	err = wait_for_bit(MTL_TXQ0_OPERATION_MODE,
					   MTL_TXQ0_OPERATION_MODE_FTQ,
					   RESET,
					   MTL_TXQ_FLUSH_TIMEOUT_USEC);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Transmit queue operating modes */
	/* Start transmission only when a full packet resides in the Tx queue */
	/* Enable the transmit queue */
	SET_REG_BITS(MTL_TXQ0_OPERATION_MODE,
				 BIT(MTL_TXQ0_OPERATION_MODE_TSF) |
				 SET_BIT_FIELD_DEF(MTL_TXQ0_OPERATION_MODE_TXQEN, ENABLED));

	/* Receive queue operating modes */
	/* Read a packet from the Rx queue only after the complete packet has been wriiten to it */
	SET_REG_BIT(MTL_RXQ0_OPERATION_MODE, RSF);

	/* Get Transmit and Receive FIFO size */
	val = NV_READ32(MAC_HW_FEATURE1);
	tx_fifo_sz = GET_BIT_FIELD_DEF(val, MAC_HW_FEATURE1, TXFIFOSIZE);
	rx_fifo_sz = GET_BIT_FIELD_DEF(val, MAC_HW_FEATURE1, RXFIFOSIZE);

	/* t/rx_fifo_sz is encoded as log2(n / 128). Undo that by shifting. */
	eqos.tx_fifo_sz_bytes = (128 << tx_fifo_sz);
	rx_fifo_sz_bytes = (128 << rx_fifo_sz);
	/* t/rqs is encoded as log2(t/rx_fifo_size_bytes / 256) */
	tqs = (eqos.tx_fifo_sz_bytes/256) - 1;
	rqs = (rx_fifo_sz_bytes/256) - 1;

	/* Set Transmit and Receive FIFO size */
	SET_REG_BIT_FIELD_NUM(MTL_TXQ0_OPERATION_MODE, TQS, tqs);
	SET_REG_BIT_FIELD_NUM(MTL_RXQ0_OPERATION_MODE, RQS, rqs);

	SET_REG_BIT(MTL_RXQ0_OPERATION_MODE, EHFC);

fail:
	return err;
}

static void tegrabl_eqos_mac_init(void)
{
	pr_trace("%s()\n", __func__);

	/*
	 * Configure MAC:
	 * enabe full duplex mode
	 * allow jumbo packets of 9018 bytes
	 * disbale jabber timer
	 * minimum inter-packet gap between packets during transmission
	 */
	SET_REG_BITS(MAC_CONFIGURATION,
				 BIT(MAC_CONFIGURATION_DM) |
				 BIT(MAC_CONFIGURATION_JE) |
				 BIT(MAC_CONFIGURATION_JD) |
				 SET_BIT_FIELD_DEF(MAC_CONFIGURATION_IPG, 40_BIT_TIMES));

	/* Filter broadcast packets */
	SET_REG_BITS(MAC_PACKET_FILTER, BIT(MAC_PACKET_FILTER_HPF) | BIT(MAC_PACKET_FILTER_PM));

	/* Enable RX queue 0 for DCB or generic */
	SET_REG_BIT_FIELD_DEF(MAC_RXQ_CTRL0, RXQ0EN, DCB_ENABLED);

	/* Route all multicast, broadcast, AV, DCB and untagged packets to the Rx queue 0 */
	NV_WRITE32(MAC_RXQ_CTRL1, 0);

	/* Enable tx and rx */
	SET_REG_BITS(MAC_CONFIGURATION, BIT(MAC_CONFIGURATION_TE) | BIT(MAC_CONFIGURATION_RE));
}

static void tegrabl_eqos_prepare_tx_desc(size_t len)
{
	struct eqos_desc *tx_desc = NULL;
	dma_addr_t p_tx_desc;
	dma_addr_t p_tx_dma_buf;

	p_tx_dma_buf = tegrabl_dma_map_buffer(TEGRABL_MODULE_EQOS, 0, (void *)eqos.tx_dma_buf[eqos.tx_desc_id],
										  len, TEGRABL_DMA_TO_DEVICE);

	/* Program descriptor */
	tx_desc = &(eqos.tx_descs[eqos.tx_desc_id]);
	tx_desc->des0 = (uintptr_t)p_tx_dma_buf;
	tx_desc->des1 = 0;
	tx_desc->des2 = len;
	tx_desc->des3 = TDES3_OWN_DMA | TDES3_FD | TDES3_LD | TDES3_CPC_INSERT_CRC_AND_PAD | len;

	/* Flush cache */
	p_tx_desc = tegrabl_dma_map_buffer(TEGRABL_MODULE_EQOS, 0, (void *)tx_desc, DESCRIPTOR_SIZE,
									   TEGRABL_DMA_TO_DEVICE);

	/* Program descriptor registers */
	NV_WRITE32(DMA_CH0_TXDESC_LIST_HIGH_ADDR, 0x0);
	NV_WRITE32(DMA_CH0_TXDESC_LIST_ADDR, (uintptr_t)p_tx_desc);
	NV_WRITE32(DMA_CH0_TXDESC_TAIL_POINTER, (uintptr_t)((struct eqos_desc *)p_tx_desc + 1));

}

static void tegrabl_eqos_prepare_rx_desc(void)
{
	struct eqos_desc *rx_desc = NULL;
	dma_addr_t p_rx_desc;

	/* Setup descriptor */
	rx_desc = &(eqos.rx_descs[eqos.rx_desc_id]);
	rx_desc->des0 = (uintptr_t)eqos.rx_dma_buf[eqos.rx_desc_id];
	rx_desc->des1 = 0;
	rx_desc->des2 = 0;
	rx_desc->des3 = RDES3_OWN_DMA | RDES3_IOC | RDES3_BUF1V;

	p_rx_desc = tegrabl_dma_map_buffer(TEGRABL_MODULE_EQOS, 0, (void *)rx_desc, DESCRIPTOR_SIZE,
									   TEGRABL_DMA_TO_DEVICE);
	TEGRABL_UNUSED(p_rx_desc);

	/* Setup descriptor registers */
	NV_WRITE32(DMA_CH0_RXDESC_LIST_HIGH_ADDR, 0x0);
	NV_WRITE32(DMA_CH0_RXDESC_LIST_ADDR, (uintptr_t)p_rx_desc);
	NV_WRITE32(DMA_CH0_RXDESC_TAIL_POINTER, (uintptr_t)((struct eqos_desc *)p_rx_desc + 1));
}

tegrabl_error_t tegrabl_eqos_init(void)
{
	uint32_t phy_oui;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("EQoS: Init\n");

	eqos.phy.read = tegrabl_eqos_phy_read;
	eqos.phy.write = tegrabl_eqos_phy_write;
	eqos.phy.page_sel_reg = 0;
	/* Set current page to some random value so that first mdio rd/wr operation uses the given page no */
	eqos.phy.curr_page = ~(0);

	/* Program clocks */
	err = tegrabl_eqos_program_pll();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_eqos_enable_clks();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Pad settings */
	tegrabl_calibrate_pads();
	err = tegrabl_eqos_auto_calib();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* TODO: add comment */
	NV_WRITE32(NV_ADDRESS_MAP_LIC_BASE + REG_INTR_CHANNEL0_SLICE5_IEP_CLASS_0, 0xF);
	NV_WRITE32(NV_ADDRESS_MAP_LIC_BASE + REG_INTR_CHANNEL0_SLICE6_IEP_CLASS_0, 0xF);

	err = tegrabl_eqos_reset_phy();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_eqos_get_phy_addr(&eqos.phy.mdio_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Register PHY */
	phy_oui = tegrabl_phy_get_oui(&eqos.phy);
	if (phy_oui == PHY_MARVELL_OUI) {
		eqos.phy.config = tegrabl_phy_marvell_config;
		eqos.phy.auto_neg = tegrabl_phy_marvell_auto_neg;
		eqos.phy.detect_link = tegrabl_phy_marvell_detect_link;
	} else if (phy_oui == PHY_REALTEK_OUI) {
		eqos.phy.config = tegrabl_phy_realtek_config;
		eqos.phy.auto_neg = tegrabl_phy_realtek_auto_neg;
		eqos.phy.detect_link = tegrabl_phy_realtek_detect_link;
	} else {
		pr_error("Unsupported PHY, OUI: 0x%08x\n", phy_oui);
		goto fail;
	}

	/* Program PHY */
	eqos.phy.config(&eqos.phy);
	err = eqos.phy.auto_neg(&eqos.phy);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Detect link */
	eqos.phy.detect_link(&eqos.phy);
	if (eqos.phy.is_link_up == false) {
		pr_error("EQoS: link is down\n");
		goto fail;
	}

	err = tegrabl_eqos_adjust_link();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Initialize EQoS */
	err = tegrabl_eqos_dma_init();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_eqos_mtl_init();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_eqos_mac_init();

	err = tegrabl_eqos_alloc_resources();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to allocate resources\n");
		goto fail;
	}
	eqos.tx_desc_id = 0;
	eqos.rx_desc_id = 0;
	NV_WRITE32(DMA_CH0_TXDESC_RING_LENGTH, DESCRIPTORS_TX-1);
	NV_WRITE32(DMA_CH0_RXDESC_RING_LENGTH, DESCRIPTORS_RX-1);

	tegrabl_eqos_prepare_rx_desc();

	/* Start Rx of DMA */
	SET_REG_BIT(DMA_CH0_RX_CONTROL, SR);

fail:
	return err;
}

void tegrabl_eqos_send(void *packet, size_t len)
{
	struct eqos_desc *tx_desc = NULL;

	/* Stop Tx of DMA */
	CLR_REG_BIT(DMA_CH0_TX_CONTROL, ST);

	/* Copy packet to DMA buffer */
	memset(eqos.tx_dma_buf[eqos.tx_desc_id], 0, len);
	memcpy(eqos.tx_dma_buf[eqos.tx_desc_id], packet, len);
	print_buffer(eqos.tx_dma_buf[eqos.tx_desc_id], len, "Tx buffer");

	tegrabl_eqos_prepare_tx_desc(len);

	/* Start Tx of DMA */
	SET_REG_BIT(DMA_CH0_TX_CONTROL, ST);

	/* Wait till DMA transfers the data */
	tx_desc = &(eqos.tx_descs[eqos.tx_desc_id]);
	while (true) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_EQOS, 0, (void *)tx_desc, DESCRIPTOR_SIZE,
								 TEGRABL_DMA_FROM_DEVICE);
		if ((tx_desc->des3 & (0x1 << 31)) == 0) {
			break;
		}
#if EQOS_DEBUG
		print_debug_registers(DEBUG_TX_DESC);
		print_reg("tx dma register", 0x2491104);
		print_reg("Tx buffer addr   ", 0x2491154);
		pr_info("checking for DMA own bit\n");
#endif
	}


	eqos.tx_desc_id++;
	eqos.tx_desc_id %= DESCRIPTORS_TX;

	/* Setup DMA interrupts */
	SET_REG_BITS(DMA_CH0_INTERRUPT_ENABLE,
				 BIT(DMA_CH0_INTERRUPT_ENABLE_NIE) | BIT(DMA_CH0_INTERRUPT_ENABLE_RIE));

	/* Stop Tx of DMA */
	CLR_REG_BIT(DMA_CH0_TX_CONTROL, ST);

	return;
}

void tegrabl_eqos_receive(void *packet, size_t *len)
{
	struct eqos_desc *rx_desc = NULL;
	static uint32_t total_rx_pkt_cnt = 0;

	TEGRABL_UNUSED(total_rx_pkt_cnt);

	/* Get packet length */
	rx_desc = &(eqos.rx_descs[eqos.rx_desc_id]);
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_EQOS, 0, (void *)rx_desc, DESCRIPTOR_SIZE, TEGRABL_DMA_FROM_DEVICE);
	*len = (rx_desc->des3 & (0x7FFF));

	/* Transfer packet to network layer */
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_EQOS, 0, (void *)eqos.rx_dma_buf[eqos.rx_desc_id], *len,
							 TEGRABL_DMA_FROM_DEVICE);
	memcpy(packet, eqos.rx_dma_buf[eqos.rx_desc_id], *len);

	/* Unicast */
	if ((*(uint8_t *)eqos.rx_dma_buf[eqos.rx_desc_id] & 1U) == 0U) {
		pr_trace("Rx packet: %u, len = %d, desc cnt: %u\n", total_rx_pkt_cnt++, (int32_t)*len, eqos.rx_desc_id);
		print_buffer(eqos.rx_dma_buf[eqos.rx_desc_id], *len, "Rx buffer");
	}

	/* Disable Rx of DMA (till next set of descriptor is ready) */
	CLR_REG_BIT(DMA_CH0_RX_CONTROL, SR);

	eqos.rx_desc_id++;
	eqos.rx_desc_id %= DESCRIPTORS_RX;

	tegrabl_eqos_prepare_rx_desc();

	/* Enable Rx of DMA */
	SET_REG_BIT(DMA_CH0_RX_CONTROL, SR);

	return;
}

bool tegrabl_eqos_is_dma_rx_intr_occured(void)
{
	uint32_t val;
	val = NV_READ32(DMA_CH0_STATUS);
	return (bool)EQOS_GET_BIT(val, DMA_CH0_STATUS_RI);
}

void tegrabl_eqos_clear_dma_rx_intr(void)
{
	uint32_t val;

	SET_REG_BIT(DMA_CH0_STATUS, RI);

	/* Clear virtual interrupt */
	val = NV_READ32(REG_ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0);
	val = NV_FLD_SET_DRF_DEF(ETHER_QOS, VIRTUAL_INTR_CH0_STATUS, RX, SW_CLEAR, val);
	NV_WRITE32(REG_ETHER_QOS_VIRTUAL_INTR_CH0_STATUS_0, val);
}

void tegrabl_eqos_set_mac_addr(uint8_t * const addr)
{
	uint32_t val;

	/* High addr needs to be programmed first */
	SET_REG_BIT_FIELD_NUM(MAC_ADDR0_HIGH, ADDRHI, (addr[5] << 8) | (addr[4]));

	val = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | (addr[0]);
	NV_WRITE32(MAC_ADDR0_LOW, val);

	pr_info("MAC addr %02x:%02x:%02x:%02x:%02x:%02x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	pr_trace("mac addr low : 0x%08x\n", NV_READ32(MAC_ADDR0_LOW));
	pr_trace("mac addr high: 0x%08x\n", NV_READ32(MAC_ADDR0_HIGH));
}

void tegrabl_eqos_deinit(void)
{
	uint32_t i;

	/* Stop Tx of DMA */
	CLR_REG_BIT(DMA_CH0_TX_CONTROL, ST);
	/* Stop Tx and Rx of MAC */
	CLR_REG_BITS(MAC_CONFIGURATION, BIT(MAC_CONFIGURATION_TE) | BIT(MAC_CONFIGURATION_RE));
	/* Disable Rx of DMA */
	CLR_REG_BIT(DMA_CH0_RX_CONTROL, SR);

	tegrabl_eqos_disable_clks();

	for (i = 0; i < DESCRIPTORS_RX; i++) {
		if (eqos.rx_dma_buf[i] != NULL) {
			tegrabl_free(eqos.rx_dma_buf[i]);
		}
	}
	for (i = 0; i < DESCRIPTORS_TX; i++) {
		if (eqos.tx_dma_buf[i] != NULL) {
			tegrabl_free(eqos.tx_dma_buf[i]);
		}
	}
	if (eqos.rx_descs != NULL) {
		tegrabl_free(eqos.rx_descs);
	}
	if (eqos.tx_descs != NULL) {
		tegrabl_free(eqos.tx_descs);
	}
}
