/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#if defined(CONFIG_ENABLE_ETHERNET_BOOT)

#define MODULE TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <etharp.h>
#include <lwip/netif.h>
#include <lwip/init.h>
#include <lwip/dhcp.h>
#include <lwip/snmp.h>
#include <lwip/apps/tftp_client.h>
#include <platform/interrupts.h>
#include <tegrabl_board_info.h>
#include <tegrabl_eqos.h>
#include <tegrabl_cbo.h>
#include <tegrabl_sdram_usage.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_linuxboot_utils.h>
#include <net_boot.h>

#define TFTP_SERVER_IP						"10.24.238.35"
#define TFTP_MAX_RRQ_RETRIES				(5)

#define MAC_RX_CH0_INTR						(32 + 194)
#define DHCP_TIMEOUT_MS						(20 * 1000)

#define AUX_INFO_DHCP_TIMEOUT				1
#define AUX_INFO_DTB_RD_REQ_TIMEOUT			2
#define AUX_INFO_BOOT_IMAGE_RD_REQ_TIMEOUT	3
#define AUX_INFO_DTB_RECV_ERR				4
#define AUX_INFO_BOOT_IMAGE_RECV_ERR		5
#define AUX_INFO_TFTP_CLIENT_INIT_FAILED	6
#define AUX_INFO_TFTP_CLIENT_INIT_FAILED	6

static struct netif netif;
struct netif *saved_netif;

static void convert_ip_str_to_int(char * const ip_addr_str, uint8_t * const ip_addr_int)
{
	uint32_t i = 0;
	char *tok = strtok(ip_addr_str, ".");

	while (tok != NULL) {
		pr_trace("tok = %s\n", tok);
		ip_addr_int[i] = tegrabl_utils_strtoul(tok, NULL, 10);
		pr_trace("ip = %u\n", ip_addr_int[i]);
		tok = strtok(NULL, ".");
		i++;
	}
}

err_t pass_network_packet_to_ethernet_controller(struct netif *netif, struct pbuf *p)
{
	struct pbuf *tx_data;
	saved_netif = netif;

	/*
	 * Send the data from the pbuf to the ethernet controller, one pbuf at a time. The size of the data in
	 * each pbuf is kept in the len variable.
	 */
	for (tx_data = p; tx_data != NULL; tx_data = tx_data->next) {
		tegrabl_eqos_send(tx_data->payload, tx_data->len);
		unmask_interrupt(MAC_RX_CH0_INTR);   /* Enable interrupt */
	}

	/* Increment packet counters */
	MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
	if (((u8_t *)p->payload)[0] & 1) {
		MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);   /* broadcast or multicast packet*/
	} else {
		MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);	/* unicast packet */
	}

	LINK_STATS_INC(link.xmit);

	return ERR_OK;
}

err_t process_ethernet_frame(void)
{
	struct pbuf *p = NULL;
	size_t len;
	struct netif *netif = saved_netif;
	uint8_t payload[1536];
	err_t err = ERR_OK;

	if (saved_netif == NULL) {
		pr_error("Invalid netif\n");
		err = ERR_MEM;
		goto fail;
	}

	tegrabl_eqos_receive(&payload, &len);
	/* cboot only supports TFTP. Packages with size larger
	 * than standard TFTP package (with 512 bytes TFTP
	 * data for each package) will be dropped.
	 * Ping (ICMP) with size less than 512 is also supported.
	 */
	if (len > 562) {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		MIB2_STATS_NETIF_INC(netif, ifindiscards);
		err = ERR_MEM;
		goto fail;
	}
	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	if (p != NULL) {
		pbuf_copy_from_userbuffer(p, payload, len);
		MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
		if (((u8_t *)p->payload)[0] & 1) {
			/* broadcast or multicast packet*/
			LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
						("broadcast/multicast packet\n"));
			MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
		} else {
			/* unicast packet*/
			LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("unicast\n"));
			MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
		}

		LINK_STATS_INC(link.recv);
	} else {
		LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE , ("Dropping Packet / Do Nothing\n"));
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		MIB2_STATS_NETIF_INC(netif, ifindiscards);
		goto fail;
	}

	err = netif_input(p, netif);
	if (err != ERR_OK) {
		pr_error("Network layer failed to process packet, err: %d\n", err);
		goto fail;
	}

fail:
	if (p != NULL) {
		pbuf_free(p);
	}

	return err;
}

/* This callback will get called when interface is brought up/down or address is changed while up */
static void netif_status_callback(struct netif *netif)
{
	pr_info("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

static handler_return_t pass_ethernet_frame_to_network_stack(void *arg)
{
	TEGRABL_UNUSED(arg);

	mask_interrupt(MAC_RX_CH0_INTR);
	/* TODO: Handle or defer using RESCHED */
	if (tegrabl_eqos_is_dma_rx_intr_occured()) {
		tegrabl_eqos_clear_dma_rx_intr();
		process_ethernet_frame();			   /* Call LWIP to process RX */
	}
	unmask_interrupt(MAC_RX_CH0_INTR);

	return INT_NO_RESCHEDULE;
}

static err_t platform_netif_init(struct netif *netif)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	register_int_handler(MAC_RX_CH0_INTR, pass_ethernet_frame_to_network_stack, 0);

	/* Initialize ethernet i/f - MAC and PHY */
	error = tegrabl_eqos_init();
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to initialize EQoS controller\n");
		goto done;
	}

	/* Obtain MAC address from EEPROM and assign to netif */
	error = tegrabl_get_mac_address(MAC_ADDR_TYPE_ETHERNET, (uint8_t *)&(netif->hwaddr), NULL);
	if (error != TEGRABL_NO_ERROR) {
		pr_info("Failed to read MAC addr from EEPROM\n");
		goto cleanup;
	}
	/* Program MAC address to controller for packet filtering */
	tegrabl_eqos_set_mac_addr((uint8_t *)&netif->hwaddr);

	netif->name[0] = 'e';
	netif->name[1] = '0';
	netif->mtu = 1500;	 /* MAX per IEEE802.3 */
	netif->hwaddr_len = 6;
	netif->output = &etharp_output;
	netif->linkoutput = &pass_network_packet_to_ethernet_controller;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;  /* Program netif capabilties */
	netif->flags |= NETIF_FLAG_LINK_UP;  /* MAC controller initialization is successful means link is up */
	netif_set_status_callback(netif, netif_status_callback);
	netif_set_default(netif);

	/* Assume link is up - We will reach here only if link is up */
	netif_set_link_up(netif);

	goto done;

cleanup:
	tegrabl_eqos_deinit();
done:
	return (err_t)error;
}

static tegrabl_error_t net_boot_stack_init(void)
{
	uint8_t *ip_addr = NULL;
	struct netif *ret_netif = NULL;
	struct ip_info info = {0};
	time_t start_time_ms;
	time_t elapsed_time_ms;
	ip4_addr_t static_ip;
	ip4_addr_t netmask;
	ip4_addr_t gateway;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	lwip_init();

	ret_netif = netif_add(&netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, &platform_netif_init,
						  &netif_input);
	if (ret_netif == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		pr_error("Failed to add interface to the lwip netifs list\n");
		goto fail;
	}

	info = tegrabl_get_ip_info();

	if (info.is_dhcp_enabled) {

		pr_info("DHCP: Init: Requesting IP ...\n");

		/* Bring an interface up, available for processing */
		netif_set_up(&netif);

		err = (tegrabl_error_t)dhcp_start(&netif);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("DHCP failed\n");
			goto fail;
		}

		start_time_ms = tegrabl_get_timestamp_ms();

		while (dhcp_supplied_address(&netif) == 0) {
			elapsed_time_ms = tegrabl_get_timestamp_ms() - start_time_ms;
			if (elapsed_time_ms > DHCP_TIMEOUT_MS) {
				pr_error("Failed to acquire IP address via DHCP within timeout\n");
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_DHCP_TIMEOUT);
				goto fail;
			}
			tegrabl_mdelay(500);
			dhcp_fine_tmr();
		}

	} else {
		pr_info("Configure Static IP ...\n");
		memcpy(&static_ip.addr, &info.static_ip, 4);
		memcpy(&netmask.addr, &info.ip_netmask, 4);
		memcpy(&gateway.addr, &info.ip_gateway, 4);
		netif_set_addr(&netif, &static_ip, &netmask, &gateway);
		netif_set_up(&netif);
	}

	ip_addr = (uint8_t *)(&(netif.ip_addr.addr));
	pr_info("Our IP: %d.%d.%d.%d\n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

fail:
	return err;
}

static tegrabl_error_t download_kernel_and_dtb_from_tftp(uint8_t *tftp_server_ip,
														 void *boot_img_load_addr,
														 void *dtb_load_addr,
														 uint32_t *boot_img_size)
{
	err_t ret = 0;
	uint32_t retry;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	ret = tftp_client_init(tftp_server_ip);
	if (ret != ERR_OK) {
		pr_error("Failed to initialize TFTP client\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, AUX_INFO_TFTP_CLIENT_INIT_FAILED);
		goto fail;
	}

	retry = 0;
	while (retry++ < TFTP_MAX_RRQ_RETRIES) {
		ret = tftp_client_recv(KERNEL_DTB, "octet", dtb_load_addr, DTB_MAX_SIZE, NULL);
		if (ret == ERR_OK) {
			break;
		} else if (ret == ERR_CONN) {
			etharp_tmr();
			tegrabl_mdelay(10);
			continue;
		} else if (ret != ERR_OK) {
			pr_error("Failed to get %s\n", KERNEL_DTB);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_DTB_RECV_ERR);
			goto fail;
		}
	}
	if (retry >= TFTP_MAX_RRQ_RETRIES) {
		pr_error("Failed to send RRQ of %s within max retries\n", KERNEL_DTB);
		err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_DTB_RD_REQ_TIMEOUT);
		goto fail;
	}

	retry = 0;
	while (retry++ < TFTP_MAX_RRQ_RETRIES) {
		ret = tftp_client_recv(BOOT_IMAGE, "octet", boot_img_load_addr, BOOT_IMAGE_MAX_SIZE, boot_img_size);
		if (ret == ERR_OK) {
			break;
		} else if (ret == ERR_CONN) {
			tegrabl_mdelay(10);
			continue;
		} else if (ret != ERR_OK) {
			pr_error("Failed to get %s\n", BOOT_IMAGE);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_BOOT_IMAGE_RECV_ERR);
			goto fail;
		}
	}
	if (retry >= TFTP_MAX_RRQ_RETRIES) {
		pr_error("Failed to send RRQ of %s within max retries\n", BOOT_IMAGE);
		err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_BOOT_IMAGE_RD_REQ_TIMEOUT);
		goto fail;
	}

fail:
	tegrabl_eqos_deinit();
	netif_set_down(&netif);
	netif_remove(&netif);

	return err;
}

tegrabl_error_t net_boot_load_kernel_and_dtb(void **boot_img_load_addr, void **dtb_load_addr)
{
	struct ip_info info = {0};
	uint32_t boot_img_size;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("########## Net boot ##########\n");

	if ((boot_img_load_addr == NULL) || (dtb_load_addr == NULL)) {
		pr_error("Invalid args passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	/* Initialize network stack */
	if (net_boot_stack_init() != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) network stack init\n", err);
		err = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, 0);
		goto fail;
	}

	/* Get load address */
	*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();
	err = tegrabl_get_boot_img_load_addr(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Download kernel iand dtb from tftp */
	info = tegrabl_get_ip_info();
	err = download_kernel_and_dtb_from_tftp(info.tftp_server_ip,
											*boot_img_load_addr,
											*dtb_load_addr,
											&boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Validate downloaded binaries */
#if defined(CONFIG_OS_IS_L4T)
	err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL, BOOT_IMAGE_MAX_SIZE, *boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL_DTB, DTB_MAX_SIZE, *dtb_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

	err = tegrabl_verify_boot_img_hdr(*boot_img_load_addr, boot_img_size);

fail:
	return err;
}

#endif  /* CONFIG_ENABLE_ETHERNET_BOOT */
