/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdio.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_fastboot_protocol.h>
#include <tegrabl_fastboot_partinfo.h>
#include <tegrabl_fastboot_oem.h>
#include <tegrabl_fastboot_a_b.h>
#include <tegrabl_transport_usbf.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_sparse.h>
#include <tegrabl_exit.h>
#include <tegrabl_board_info.h>
#include <linux_load.h>
#include <tegrabl_bootloader_update.h>
#include <tegrabl_a_b_partition_naming.h>
#include <menu.h>

#define MAX_SERIALNO_LEN 32
#define MAX_PART_NAME_LEN 16

static uint32_t fastboot_state = STATE_OFFLINE;
static struct fastboot_cmd *cmdlist;
static void *download_base;
static uint32_t download_size;

void fastboot_ack(const char *code, const char *reason)
{
	char response[MAX_RESPONSE_SIZE];
	uint32_t bytes_transmitted = 0;
	tegrabl_error_t retval = TEGRABL_NO_ERROR;

	if (fastboot_state != STATE_COMMAND) {
		return;
	}

	if (reason == 0) {
		reason = "";
	}

	snprintf(response, MAX_RESPONSE_SIZE, "%s%s", code, reason);

	if (strcmp(code, "INFO")) {
		fastboot_state = STATE_COMPLETE;
	}

	retval = tegrabl_transport_usbf_send(response, strlen(response),
										 &bytes_transmitted, FB_TFR_TIMEOUT);
	if (retval) {
		return;
	}
}

void fastboot_fail(const char *reason)
{
	fastboot_ack("FAIL", reason);
}

void fastboot_okay(const char *info)
{
	fastboot_ack("OKAY", info);
}

static void fastboot_getvar(const char *arg, char *response)
{
	const char *partition_name;
	ssize_t size = 0;
	const struct tegrabl_fastboot_partition_info *partinfo = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	uint8_t serial_no[MAX_SERIALNO_LEN] = {0};
	bool is_unlocked;

	if (IS_VAR_TYPE("version-bootloader"))
		COPY_RESPONSE("1.0");
	else if (IS_VAR_TYPE("max-download-size"))
		sprintf(response, "0x%08x", MAX_DOWNLOAD_SIZE);
	else if (IS_VAR_TYPE("product"))
		COPY_RESPONSE(FASTBOOT_PRODUCT);
	else if (IS_VAR_TYPE("serialno")) {
		error = tegrabl_get_serial_no(serial_no);
		if (error != TEGRABL_NO_ERROR) {
			COPY_RESPONSE("not valid");
			return;
		}
		COPY_RESPONSE((char *)serial_no);
	} else if (IS_VAR_TYPE("secure")) {
		error = tegrabl_is_device_unlocked(&is_unlocked);
		if (error) {
			return;
		}
		COPY_RESPONSE(is_unlocked ? "No" : "Yes");
	} else if (IS_VAR_TYPE("unlocked")) {
		error = tegrabl_is_device_unlocked(&is_unlocked);
		if (error) {
			return;
		}
		COPY_RESPONSE(is_unlocked ? "Yes" : "No");
	} else if (IS_VAR_TYPE("partition-size:")) {
		partition_name = arg + strlen("partition-size:");
		partinfo = tegrabl_fastboot_get_partinfo(partition_name);
		if (!partinfo) {
			pr_warn("%s: no partition present with this name.\n", partition_name);
			return;
		}

		error = tegrabl_partition_open(partinfo->tegra_part_name, &partition);
		if (error) {
			pr_warn("%s: no partition present with this name.\n", partition_name);
			return;
		}

		size = tegrabl_partition_size(&partition);

		if (size > 0)
			sprintf(response + strlen(response), "0x%zx", (size_t)size);
	} else if (IS_VAR_TYPE("partition-type:")) {
		partition_name = arg + strlen("partition-type:");

		partinfo = tegrabl_fastboot_get_partinfo(partition_name);
		if (!partinfo) {
			pr_warn("%s: no partition present with this name.\n", partition_name);
			return;
		}

		/* Double check if partition present */
		error = tegrabl_partition_open(partinfo->tegra_part_name, &partition);
		if (error) {
			pr_warn("%s: no partition present with this name.\n", partition_name);
			return;
		}

		sprintf(response + strlen(response), "%s", partinfo->fastboot_part_type);
		return;
	} else if (is_a_b_var_type(arg)) {
		error = tegrabl_fastboot_a_b_var_handler(arg, response);
		if (error != TEGRABL_NO_ERROR) {
			return;
		}
	} else {
		COPY_RESPONSE("Unknown var!");
	}
}

static void cmd_reboot(const char *arg, void *data, uint32_t size)
{
	(void)arg;
	(void)data;
	(void)size;

	pr_info("Rebooting the device\n");

	tegrabl_reset();

	return;
}

static void cmd_reboot_bootloader(const char *arg, void *data, uint32_t size)
{
	(void)arg;
	(void)data;
	(void)size;

	pr_info("Rebooting to bootloader\n");
	fastboot_okay("");

	tegrabl_reboot_fastboot();

	return;
}

static void cmd_set_active_slot(const char *arg, void *data, uint32_t size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool is_unlocked;
	(void)size;
	(void)data;

	err = tegrabl_is_device_unlocked(&is_unlocked);
	if (err != TEGRABL_NO_ERROR) {
		fastboot_fail("Bootloader lock state unknown");
		return;
	}
	if (!is_unlocked) {
		fastboot_fail("Bootloader is locked.");
		return;
	}

	err = tegrabl_set_active_slot(arg);
	if (err != TEGRABL_NO_ERROR) {
		fastboot_fail("Failed to set slot active");
		return;
	}

	fastboot_okay("");
	return;
}

static void cmd_getvar(const char *arg, void *data, uint32_t sz)
{
	uint32_t i, j;
	uint32_t size, num_partitions;
	char response[MAX_RESPONSE_SIZE];
	char cmd[MAX_RESPONSE_SIZE];
	uint8_t num_slots;

	if (IS_VAR_TYPE("all")) {
		/* Send var list */
		size = size_var_list;
		for (i = 0; i < size; i++) {
			memset(response, 0, MAX_RESPONSE_SIZE);
			COPY_RESPONSE(fastboot_var_list[i]);
			COPY_RESPONSE(": ");
			fastboot_getvar(fastboot_var_list[i], response);
			fastboot_ack("INFO", response);
		}

		/* Send partition-type and partition-size of flashable partitions */
		size = size_pvar_list;
		num_partitions = size_partition_map_list;

		for (i = 0; i < num_partitions; i++) {
			for (j = 0; j < size; j++) {
				memset(cmd, 0, MAX_RESPONSE_SIZE);
				memset(response, 0, MAX_RESPONSE_SIZE);
				COPY_RESPONSE(fastboot_partition_var_list[j]);
				COPY_RESPONSE(
					fastboot_partition_map_table[i].fastboot_part_name);
				strcpy(cmd, response);
				COPY_RESPONSE(": ");
				fastboot_getvar(cmd, response);
				fastboot_ack("INFO", response);
			}
		}
		tegrabl_error_t err = TEGRABL_NO_ERROR;
		err = tegrabl_get_slot_num(&num_slots);
		if (err != TEGRABL_NO_ERROR) {
			return;
		}

		/* Send bootslot info */
		for (i = 0; i < size_a_b_slot_var_list; i++) {
			for (j = 0; j < num_slots; j++) {
				memset(response, 0, MAX_RESPONSE_SIZE);
				memset(response, 0, MAX_RESPONSE_SIZE);
				COPY_RESPONSE(fastboot_a_b_slot_var_list[i]);
				err = tegrabl_get_slot_suffix(j, response + strlen(response));
				if (err != TEGRABL_NO_ERROR) {
					continue;
				}
				strcpy(cmd, response);
				COPY_RESPONSE(": ");
				fastboot_getvar(cmd, response);
				fastboot_ack("INFO", response);
			}
		}
		fastboot_okay("");
	} else {
		memset(response, 0, MAX_RESPONSE_SIZE);
		fastboot_getvar(arg, response);
		fastboot_okay(response);
	}
}

static void cmd_erase(const char *arg, void *data, uint32_t sz)
{
	const struct tegrabl_fastboot_partition_info *partinfo = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	bool is_unlocked;

	error = tegrabl_is_device_unlocked(&is_unlocked);
	if (error != TEGRABL_NO_ERROR) {
		fastboot_fail("Bootloader lock state unknown");
		return;
	}
	if (!is_unlocked) {
		fastboot_fail("Bootloader is locked.");
		return;
	}

	partinfo = tegrabl_fastboot_get_partinfo(arg);
	if (!partinfo) {
		fastboot_fail("No partition present with this name.");
		return;
	}

	error = tegrabl_partition_open(partinfo->tegra_part_name, &partition);
	if (error == TEGRABL_NO_ERROR) {
		error = tegrabl_partition_erase(&partition, false);
	}

	if (!error)
		fastboot_okay("");
	else
		fastboot_fail("Partition erase failed!");

	return;
}

static uint32_t hex2uint32_t(const uint8_t *x)
{
	uint32_t n = 0;

	while (*x) {
		switch (*x) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			n = (n << 4) | (*x - '0');
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			n = (n << 4) | (*x - 'a' + 10);
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			n = (n << 4) | (*x - 'A' + 10);
			break;
		default:
			return n;
		}
		x++;
	}

	return n;
}

static void cmd_download(const char *arg, void *data, uint32_t sz)
{
	char response[MAX_RESPONSE_SIZE];
	uint32_t len = hex2uint32_t((const uint8_t *)arg);
	tegrabl_error_t retval = TEGRABL_NO_ERROR;
	uint32_t received = 0;
	uint32_t transmitted = 0;
	static bool is_allocated;
	bool is_unlocked;

	retval = tegrabl_is_device_unlocked(&is_unlocked);
	if (retval != TEGRABL_NO_ERROR) {
		fastboot_fail("Bootloader lock state unknown");
		return;
	}
	if (!is_unlocked) {
		fastboot_fail("Bootloader is locked.");
		return;
	}

	download_size = 0;
	if (len > MAX_DOWNLOAD_SIZE) {
		fastboot_fail("data too large");
		return;
	}

	sprintf(response, "DATA%08x", len);
	if (tegrabl_transport_usbf_send(response, strlen(response), &transmitted,
									FB_TFR_TIMEOUT)) {
		return;
	}

	if (!is_allocated) {
		download_base = tegrabl_memalign(USB_BUFFER_ALIGNMENT, len);
		is_allocated = true;
	}

	if (!download_base) {
		pr_error("%s: malloc failed\n", __func__);
		fastboot_fail("Memory Insufficient");
		return;
	}

	pr_info("%s: allocated buffer of size: %d bytes\n", __func__, len);
	pr_info("%s: usb_read: start.\n", __func__);
	retval = tegrabl_transport_usbf_receive(download_base, len, &received,
											FB_TFR_TIMEOUT);
	pr_info("%s: usb_read: end.\n", __func__);
	if ((retval) || (received != len)) {
		pr_error("%s: usb_read failed\n", __func__);
		fastboot_fail("USB read Failed");
		fastboot_state = STATE_ERROR;
		return;
	}

	download_size = len;
	fastboot_okay("");
}

static void cmd_boot(const char *arg, void *data, uint32_t sz)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void (*kernel_entry)(uintptr_t dtb_addr);
	void *kernel_entry_point = NULL;
	void *kernel_dtb = NULL;
	struct tegrabl_kernel_bin kernel;
	bool is_unlocked;

	(void)arg;

	err = tegrabl_is_device_unlocked(&is_unlocked);
	if (err != TEGRABL_NO_ERROR) {
		fastboot_fail("Bootloader lock state unknown");
		return;
	}
	if (!is_unlocked) {
		fastboot_fail("Bootloader is locked.");
		return;
	}

	fastboot_okay("");

	kernel.bin_type = TEGRABL_BINARY_KERNEL;
	kernel.load_from_storage = false;
	err = tegrabl_load_kernel_and_dtb(&kernel, &kernel_entry_point,
									  &kernel_dtb, NULL, data);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("kernel boot failed\n");
		return;
	}

	kernel_entry = (void *)kernel_entry_point;
	kernel_entry((uintptr_t) kernel_dtb);
}

static void cmd_flash(const char *arg, void *data, uint32_t sz)
{
	const struct tegrabl_fastboot_partition_info *partinfo = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	bool is_sparse = false;
	struct tegrabl_unsparse_state unsparse_state;
	bool is_unlocked;
	const char *suffix = NULL;
	const char *tegra_part_name = NULL;

	error = tegrabl_is_device_unlocked(&is_unlocked);
	if (error != TEGRABL_NO_ERROR) {
		fastboot_fail("Bootloader lock state unknown");
		return;
	}
	if (!is_unlocked) {
		fastboot_fail("Bootloader is locked.");
		return;
	}

	if (!download_base || !download_size) {
		pr_error("fastboot %s invalid buffer or buffer size.\n", __func__);
		return;
	}

	suffix = tegrabl_a_b_get_part_suffix(arg);

	if (tegrabl_a_b_match_part_name_with_suffix("bootloader", arg)) {
		error = tegrabl_check_and_update_bl_payload(download_base,
													download_size,
													suffix);
		goto flash_exit;
	}

	partinfo = tegrabl_fastboot_get_partinfo(arg);
	if (!partinfo) {
		fastboot_fail("No partition present with this name.");
		return;
	}

	tegra_part_name = tegrabl_fastboot_get_tegra_part_name(suffix, partinfo);
	error = tegrabl_partition_open(tegra_part_name, &partition);
	if (error) {
		fastboot_fail("Partition may not exist or can not be accessed.");
		return;
	}

	is_sparse = tegrabl_sparse_image_check(download_base, download_size);

	if (is_sparse) {
		error = tegrabl_sparse_init_unsparse_state(&unsparse_state,
			tegrabl_partition_size(&partition),
			tegrabl_fastboot_partition_write,
			tegrabl_fastboot_partition_seek);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to initialize unsparse state\n");
			return;
		}
	}

	if (is_sparse) {
		error = tegrabl_sparse_unsparse(&unsparse_state, download_base,
										download_size, &partition);
	} else {
		tegrabl_fastboot_partition_write(download_base, download_size,
										 &partition);
	}

flash_exit:
	if (error == 0)
		fastboot_okay("");
	else
		fastboot_fail("Partition write failed!");
}

static void cmd_flashing(const char *arg, void *data, uint32_t sz)
{
	(void)arg;

	if (IS_VAR_TYPE("lock"))
		tegrabl_change_device_state(FASTBOOT_LOCK);
	else if (IS_VAR_TYPE("unlock"))
		tegrabl_change_device_state(FASTBOOT_UNLOCK);

	fastboot_okay("");
	return;
}

static void cmd_oem(const char *arg, void *data, uint32_t sz)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	(void)arg;

	ret = tegrabl_fastboot_oem_handler(arg);
	if (ret == TEGRABL_NO_ERROR) {
		fastboot_okay("");
	}

	return;
}

static void cmd_continue(const char *arg, void *data, uint32_t sz)
{
	(void)arg;

	pr_info("fastboot: continue booting\n");

	fastboot_okay("");

	return;
}

struct fastboot_cmd {
	struct fastboot_cmd *next;
	const char *prefix;
	uint32_t prefix_len;
	void (*handle)(const char *arg, void *data, uint32_t sz);
};

static void fastboot_register(const char *prefix,
	void (*handle)(const char *arg, void *data, uint32_t sz))
{
	struct fastboot_cmd *cmd;

	cmd = tegrabl_malloc(sizeof(*cmd));
	if (cmd) {
		cmd->prefix = prefix;
		cmd->prefix_len = strlen(prefix);
		cmd->handle = handle;
		cmd->next = cmdlist;
		cmdlist = cmd;
	}
}

void tegrabl_fastboot_cmd_init(void)
{
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader);
	fastboot_register("getvar:", cmd_getvar);
	fastboot_register("erase:", cmd_erase);
	fastboot_register("download:", cmd_download);
	fastboot_register("flash:", cmd_flash);
	fastboot_register("boot", cmd_boot);
	fastboot_register("continue", cmd_continue);
	fastboot_register("oem ", cmd_oem);
	fastboot_register("flashing ", cmd_flashing);
	fastboot_register("set_active:", cmd_set_active_slot);

	pr_info("fastboot %s done.\n", __func__);
}

tegrabl_error_t tegrabl_fastboot_command_handler(void)
{
	struct fastboot_cmd *cmd;
	tegrabl_error_t retval = TEGRABL_NO_ERROR;
	uint32_t received = 0;
	uint8_t *buffer = NULL;

	pr_info("fastboot: processing commands\n");

	buffer = tegrabl_memalign(USB_BUFFER_ALIGNMENT, FASTBOOT_CMD_RCV_BUF_SIZE);
	if (!buffer) {
		retval = TEGRABL_ERR_NO_MEMORY;
		pr_error("usb buffer alloc failed\n");
		goto fail;
	}

again:
	while (fastboot_state != STATE_ERROR) {
		retval = tegrabl_transport_usbf_receive(
			buffer, 64, &received, INFINITE_TIME);
		if (retval != TEGRABL_NO_ERROR) {
			break;
		}

		buffer[received] = 0;
		pr_info("fastboot: %s\n", buffer);

		for (cmd = cmdlist; cmd; cmd = cmd->next) {
			fastboot_state = STATE_COMMAND;
			if (memcmp(buffer, cmd->prefix, cmd->prefix_len)) {
				continue;
			}
			menu_lock();
			cmd->handle((const char *)(buffer + cmd->prefix_len),
						(void *)download_base, download_size);
			menu_unlock();

			if (!memcmp(buffer, "continue", strlen("continue"))) {
				return TEGRABL_NO_ERROR;
			}

			if (fastboot_state == STATE_COMMAND) {
				fastboot_fail("unknown reason");
			}

			goto again;
		}
		fastboot_fail("unknown command");
	}
fail:
	fastboot_state = STATE_OFFLINE;
	pr_error("fastboot: oops!\n");
	return retval;
}

