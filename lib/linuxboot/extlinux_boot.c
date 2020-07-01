/**
 * Copyright (c) 2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE  TEGRABL_ERR_LINUXBOOT

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_file_manager.h>
#include <extlinux_boot.h>

#define EXTLINUX_CONF_PATH							"/boot/extlinux/extlinux.conf"
#define EXTLINUX_CONF_MAX_SIZE						4096UL

static time_t user_input_wait_timeout_ms;

static char *skip_whitespace(char * const str)
{
	char *s = str;

	while (*s != '\n') {
		if (isalnum(*s)) {
			break;
		}
		s++;
	}

	return s;
}

static void extract_val(char * const str, char * const key, char ** const buf)
{
	char *val = str;

	val = val + strlen(key);
	*buf = skip_whitespace(val);
}

static void parse_conf_file(void *conf_load_addr, struct conf *extlinux_conf)
{
	char *timeout = NULL;
	char *default_boot = NULL;
	char *str;
	int entry = -1;
	char *key = NULL;
	char *token = strtok(conf_load_addr, "\n");

	pr_trace("%s(): %u\n", __func__, __LINE__);

	do {
		str = skip_whitespace(token);

		key = "TIMEOUT";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &timeout);
			continue;
		}
		key = "DEFAULT";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &default_boot);
			continue;
		}
		key = "MENU TITLE";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->menu_title);
			continue;
		}
		key = "LABEL";
		if (!strncmp(key, str, strlen(key))) {
			entry++;
			extract_val(str, key, &extlinux_conf->section[entry].label);
			continue;
		}
		key = "MENU LABEL";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry].menu_label);
			continue;
		}
		key = "LINUX";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry].linux_path);
			continue;
		}
		key = "INITRD";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry].initrd_path);
			continue;
		}
		key = "FDT";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry].dtb_path);
			continue;
		}
		key = "APPEND";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry].boot_args);
			continue;
		}
	} while ((token = strtok(NULL, "\n")) != NULL);

	if (timeout != NULL) {
		/* extlinux.conf timeout is 1/10 of a second */
		user_input_wait_timeout_ms = tegrabl_utils_strtoul(timeout, NULL, BASE_10);
		user_input_wait_timeout_ms = (user_input_wait_timeout_ms / 10UL) * TIME_1MS;
	} else {
		user_input_wait_timeout_ms = 0UL;
	}

	extlinux_conf->num_boot_entries = entry + 1;

	for (entry = 0; (uint32_t)entry < extlinux_conf->num_boot_entries; entry++) {
		if (strncmp(extlinux_conf->section[entry].label, default_boot, strlen(default_boot)) == 0) {
			extlinux_conf->default_boot_entry = entry;
		}
		if (extlinux_conf->section[entry].menu_label) {
			pr_trace("menu label: %s\n", extlinux_conf->section[entry].menu_label);
		}
		if (extlinux_conf->section[entry].linux_path) {
			pr_trace("\tlinux path  : %s\n", extlinux_conf->section[entry].linux_path);
		}
		if (extlinux_conf->section[entry].initrd_path) {
			pr_trace("\tinitrd path : %s\n", extlinux_conf->section[entry].initrd_path);
		}
		if (extlinux_conf->section[entry].dtb_path) {
			pr_trace("\tdtb path    : %s\n", extlinux_conf->section[entry].dtb_path);
		}
		if (extlinux_conf->section[entry].boot_args) {
			pr_trace("\tboot args   : %s\n", extlinux_conf->section[entry].boot_args);
		}
	}
}

static tegrabl_error_t load_conf_file(struct tegrabl_fm_handle *fm_handle, struct conf *extlinux_conf)
{
	uint32_t file_size;
	void *conf_load_addr;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	/* Allocate space for extlinux.conf file */
	conf_load_addr = tegrabl_malloc(EXTLINUX_CONF_MAX_SIZE);
	if (conf_load_addr == NULL) {
		pr_error("Failed to allocate memory\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	/* Read the extlinux.conf file */
	pr_info("Loading %s ...\n", EXTLINUX_CONF_PATH);
	file_size = EXTLINUX_CONF_MAX_SIZE;
	err = tegrabl_fm_read(fm_handle, EXTLINUX_CONF_PATH, NULL, &conf_load_addr, &file_size, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to find/load %s\n", EXTLINUX_CONF_PATH);
		goto fail;
	}

	/* Parse extlinux.conf file */
	parse_conf_file(conf_load_addr, extlinux_conf);

fail:
	return err;
}

static int display_boot_menu(struct conf * const extlinux_conf)
{
	uint32_t idx;
	int ch = ~(0);
	bool first_attempt = true;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	/* Display boot menu */
	pr_info("%s\n", extlinux_conf->menu_title);
	for (idx = 0; idx < extlinux_conf->num_boot_entries; idx++) {
		pr_info("[%u]: \"%s\"\n", idx + 1, extlinux_conf->section[idx].menu_label);
	}

	/* Get user input */
	while (true) {
		pr_info("Enter choice: ");
		tegrabl_enable_timestamp(false);
		if (first_attempt && (user_input_wait_timeout_ms != 0UL)) {
			ch = tegrabl_getc_wait(user_input_wait_timeout_ms);
		} else {
			ch = tegrabl_getc();
		}
		first_attempt = false;
		tegrabl_printf("\n");
		tegrabl_enable_timestamp(true);

		if ((ch >= '1') && (ch <= ('0' + (signed)extlinux_conf->num_boot_entries))) {
			/* Valid input */
			ch = ch - '0';
			pr_info("Selected option: %d\n", ch);
			ch = ch - 1;  /* Adjust for array index */
			break;
		} else if (ch == ~(0)) {
			/* No user input, continue with default boot option */
			ch = extlinux_conf->default_boot_entry;
			pr_info("Continuing with default option: %d\n", ch + 1);
			break;
		} else {
			/* Invalid input, prompt again */
			if (isprint(ch)) {
				pr_info("Invalid option: %c\n", ch);
			} else {
				pr_info("Invalid option: %x\n", ch);
			}
		}
	}

	return ch;
}

tegrabl_error_t get_boot_details(struct tegrabl_fm_handle *fm_handle,
								 struct conf * const extlinux_conf,
								 uint32_t * const boot_entry)
{
	tegrabl_error_t err;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	err = load_conf_file(fm_handle, extlinux_conf);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	*boot_entry = display_boot_menu(extlinux_conf);

fail:
	return err;
}
