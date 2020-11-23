/**
 * Copyright (c) 2019-2020, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE  TEGRABL_ERR_LINUXBOOT

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fs.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_file_manager.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_linuxboot_utils.h>
#include <tegrabl_sdram_usage.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_linuxboot.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_fuse.h>
#include <extlinux_boot.h>
#include <linux_load.h>
#include <tegrabl_auth.h>

#define EXTLINUX_CONF_PATH			"/boot/extlinux/extlinux.conf"
#define EXTLINUX_CONF_MAX_SIZE		4096UL

static struct conf extlinux_conf;
static uint32_t boot_entry;
static time_t user_input_wait_timeout_ms;
static bool extlinux_boot_is_success;
static char *g_ramdisk_path;
static char *g_boot_args;

static char *skip_leading_whitespace(char * const str)
{
	char *first_non_space_char = str;

	if (str == NULL) {
		goto fail;
	}

	while ((*first_non_space_char != '\n') &&
		   (*first_non_space_char != '\0') &&
		   (isspace(*first_non_space_char) != 0UL)) {
		first_non_space_char++;
	}

fail:
	return first_non_space_char;
}

static char *remove_whitespace_and_comment(char * const str)
{
	char *s = str;
	char *first_non_space_char = NULL;

	if (str == NULL) {
		goto fail;
	}

	/* Remove leading whitespace and trailing comment */
	while ((*s != '\n') && (*s != '\0')) {
		if (isspace(*s) == 0UL) {
			if (first_non_space_char == NULL) {
				first_non_space_char = s;
			}
			if (*s == '#') {
				*s = '\0';
				break;
			}
		}
		s++;
	}
	if (first_non_space_char == NULL) {
		first_non_space_char = str;
	}

	/* Remove trailing whitespace */
	s--;
	while (s != first_non_space_char) {
		if (isspace(*s) == 0UL) {
			break;
		}
		*s = '\0';
		s--;
	}

fail:
	return first_non_space_char;
}

static void extract_val(char * const str, char * const key, char ** const buf)
{
	char *val = str + strlen(key);
	*buf = skip_leading_whitespace(val);
}

static tegrabl_error_t parse_conf_file(void *conf_load_addr, struct conf *extlinux_conf)
{
	char *timeout = NULL;
	char *default_boot = NULL;
	char *str;
	int entry;
	char *key = NULL;
	char *token = strtok(conf_load_addr, "\n");
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (token == NULL) {
		pr_error("Nothing to parse in conf file\n");
		goto fail;
	}

	pr_trace("%s(): %u\n", __func__, __LINE__);
	entry = -1;
	do {
		pr_trace("str: %s, len: %lu\n", token, strlen(token));
		str = remove_whitespace_and_comment(token);
		pr_trace("str: %s, len: %lu\n\n", str, strlen(str));
		if (*str == '\0') {
			continue;
		}

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
			extlinux_conf->section[entry] = tegrabl_malloc(sizeof(struct boot_section));
			if (extlinux_conf->section[entry] == NULL) {
				pr_error("Failed to allocate memory for %u boot section\n", entry);
				err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				goto fail;
			}
			memset(extlinux_conf->section[entry], 0, sizeof(struct boot_section));
			extract_val(str, key, &extlinux_conf->section[entry]->label);
			continue;
		}
		if (entry < 0) {
			continue;
		}
		key = "MENU LABEL";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry]->menu_label);
			continue;
		}
		key = "LINUX";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry]->linux_path);
			continue;
		}
		key = "INITRD";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry]->initrd_path);
			continue;
		}
		key = "FDT";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry]->dtb_path);
			continue;
		}
		key = "APPEND";
		if (!strncmp(key, str, strlen(key))) {
			extract_val(str, key, &extlinux_conf->section[entry]->boot_args);
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
		if (strncmp(extlinux_conf->section[entry]->label, default_boot, strlen(default_boot)) == 0) {
			extlinux_conf->default_boot_entry = entry;
		}
		if (extlinux_conf->section[entry]->menu_label) {
			pr_trace("menu label: %s\n", extlinux_conf->section[entry]->menu_label);
		}
		if (extlinux_conf->section[entry]->linux_path) {
			pr_trace("\tlinux path  : %s\n", extlinux_conf->section[entry]->linux_path);
		}
		if (extlinux_conf->section[entry]->initrd_path) {
			pr_trace("\tinitrd path : %s\n", extlinux_conf->section[entry]->initrd_path);
		}
		if (extlinux_conf->section[entry]->dtb_path) {
			pr_trace("\tdtb path    : %s\n", extlinux_conf->section[entry]->dtb_path);
		}
		if (extlinux_conf->section[entry]->boot_args) {
			pr_trace("\tboot args   : %s\n", extlinux_conf->section[entry]->boot_args);
		}
	}

fail:
	return err;
}

static tegrabl_error_t load_and_parse_conf_file(struct tegrabl_fm_handle *fm_handle,
												struct conf *extlinux_conf)
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
	pr_info("Loading extlinux.conf ...\n");
	file_size = EXTLINUX_CONF_MAX_SIZE;
	err = tegrabl_fm_read(fm_handle, EXTLINUX_CONF_PATH, NULL, conf_load_addr, &file_size, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to find/load %s\n", EXTLINUX_CONF_PATH);
		goto fail;
	}

	/* Parse extlinux.conf file */
	err = parse_conf_file(conf_load_addr, extlinux_conf);

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
		pr_info("[%u]: \"%s\"\n", idx + 1, extlinux_conf->section[idx]->menu_label);
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

static tegrabl_error_t get_boot_details(struct tegrabl_fm_handle *fm_handle,
										struct conf * const extlinux_conf,
										uint32_t * const boot_entry)
{
	tegrabl_error_t err;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	err = load_and_parse_conf_file(fm_handle, extlinux_conf);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	*boot_entry = display_boot_menu(extlinux_conf);

fail:
	return err;
}

static tegrabl_error_t load_binary_with_sig(struct tegrabl_fm_handle *fm_handle,
											uint32_t bin_type,
											uint32_t bin_max_size,
											char *bin_path,
											void *bin_load_addr,
											uint32_t *load_size)
{
	uint32_t sigheader_size = 0;
	uint32_t file_size;
	void *load_addr;
	char *bin_type_name;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

#if defined(CONFIG_ENABLE_SECURE_BOOT)
	char sig_file_path[FS_MAX_PATH_LEN];
	uint32_t sig_file_size;
	bool fail_flag = false;
#endif

	pr_trace("%s(): %u\n", __func__, __LINE__);

	switch (bin_type) {
	case TEGRABL_BINARY_KERNEL:
		bin_type_name = "kernel";
		break;
	case TEGRABL_BINARY_KERNEL_DTB:
		bin_type_name = "kernel-dtb";
		break;
	default:
		pr_error("Invalid bin_type (%d)\n", bin_type);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto exit;
	}

	if (bin_path) {
#if defined(CONFIG_ENABLE_SECURE_BOOT)
		/* load sig file */
		memset(sig_file_path, '\0', sizeof(sig_file_path));
		tegrabl_snprintf(sig_file_path, sizeof(sig_file_path), "%s.sig", bin_path);
		pr_info("Loading %s sig file from rootfs ...\n", bin_type_name);

		sig_file_size = sigheader_size = tegrabl_sigheader_size();
		load_addr = bin_load_addr;
		err = tegrabl_fm_read(fm_handle,
							  sig_file_path,
							  NULL,	/* no fallback to partition; must read from fs */
							  load_addr,
							  &sig_file_size,
							  NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_warn("Failed to load %s sig file (err=%d)\n", bin_type_name, err);
			fail_flag = true;
		}
		if (sig_file_size != sigheader_size) {
			pr_warn("Unexpected %s sig file size (%u) (expected=%u)\n",
					bin_type_name, sig_file_size, sigheader_size);
			fail_flag = true;
		}

		/* If fail_flag is set, that means sig file was not read succesfully,
		 * zero out the sig file buffer to guarantee that validation of
		 * sig + binary will fail.
		 */
		if (fail_flag) {
			memset(load_addr, 0, sigheader_size);
		}
#endif

		/* USB transactions require load address to be 64 KB aligned */
		load_addr = (void *)ROUND_UP((uintptr_t)(bin_load_addr + sigheader_size), SZ_64K);
		file_size = bin_max_size - (load_addr - bin_load_addr);
		pr_info("Loading %s binary from rootfs ...\n", bin_type_name);
		err = tegrabl_fm_read(fm_handle,
							  bin_path,
							  NULL,
							  load_addr,
							  &file_size,
							  NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_warn("Failed to load %s binary from rootfs (err=%d)\n", bin_type_name, err);
			/* continue to load binary from partition */
			goto load_from_partition;
		}
		*load_size = file_size;
		/* Place binary right after binary's signature.
		 * Note: always offset to a fixed tegrabl_sigheader_size(),
		 * in case that signature file is corrupted.
		 */
		if (load_addr != (bin_load_addr + sigheader_size)) {
			memmove(bin_load_addr + sigheader_size, load_addr, file_size);
		}

#if defined(CONFIG_ENABLE_SECURE_BOOT)
		err = tegrabl_validate_binary(bin_type, bin_max_size, bin_load_addr);
		if ((err != TEGRABL_NO_ERROR) || fail_flag) {
			/* Validation failed or sig file was not read correctly */
			pr_warn("Failed to validate %s binary (err=%d, fail=%d)\n", bin_type_name, err, fail_flag);

			/* If the security_mode fuse is burned, then abort loading binary from rootfs;
			 * and continue to load binary from partition.
			 */
			if (fuse_is_odm_production_mode()) {
				pr_error("Security fuse is burned, abort loading binary from rootfs\n");
				/* continue to load binary from partition */
				goto load_from_partition;
			} else {
				/* security_mode fuse is not burned, ignore validation failure */
				pr_warn("Security fuse not burned, ignore validation failure\n");
				pr_debug("Memmove from %p to %p\n", (bin_load_addr + sigheader_size), bin_load_addr);
				memmove(bin_load_addr, bin_load_addr + sigheader_size, *load_size);
				err = TEGRABL_NO_ERROR;
			}
		}
#endif
		goto exit;
	} else {
		pr_info("No %s binary path\n", bin_type_name);
	}

load_from_partition:
	/* Come here when either there is no bin_path, or fails to load from fs:
	 * will load binary from partition.
	 */
	load_addr = bin_load_addr;
	file_size = bin_max_size;
	err = tegrabl_load_binary(bin_type, &load_addr, &file_size);
	/* Note: tegrabl_load_binary() may change load_addr when it returns,
	 * (hence, it requires a pointer to a pointer).
	 * Then, we need to memmove the loaded binary to our inteneded location.
	 */
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to load %s binary from partition (err=%d)\n", bin_type_name, err);
		goto exit;
	}
	if (load_addr != bin_load_addr) {
		memmove(bin_load_addr, load_addr, bin_max_size);
	}
	*load_size = file_size;

#if defined(CONFIG_ENABLE_SECURE_BOOT)
	err = tegrabl_validate_binary(bin_type, bin_max_size, bin_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Failed to validate %s binary (err=%d)\n", bin_type_name, err);

		/* if the security_mode fuse is burned, then fail the loading of binary */
		if (fuse_is_odm_production_mode()) {
			pr_error("Security fuse is burned, fail to load binary\n");
			goto exit;
		}
		/* security_mode fuse is not burned, ignore validation failure */
		pr_warn("Security fuse not burned, ignore validation failure\n");
		pr_debug("Memmove from %p to %p\n", (bin_load_addr + sigheader_size), bin_load_addr);
		memmove(bin_load_addr, bin_load_addr + sigheader_size, *load_size);
		err = TEGRABL_NO_ERROR;
	}
#endif

exit:
	return err;
}

tegrabl_error_t extlinux_boot_load_kernel_and_dtb(struct tegrabl_fm_handle *fm_handle,
												  void **boot_img_load_addr,
												  void **dtb_load_addr,
												  uint32_t *kernel_size)
{
	char *linux_path;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t entry;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if ((fm_handle == NULL) || (fm_handle->mount_path == NULL)) {
		pr_error("Invalid fm_handle (%p) or mount path passed\n", fm_handle);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Load and parse extlinux.conf */
	err = get_boot_details(fm_handle, &extlinux_conf, &boot_entry);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	g_ramdisk_path = extlinux_conf.section[boot_entry]->initrd_path;
	g_boot_args = extlinux_conf.section[boot_entry]->boot_args;

	/* Get load address of kernel and dtb */
	err = tegrabl_get_boot_img_load_addr(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get kernel load addr.\n");
		goto fail;
	}
	*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();

	linux_path = extlinux_conf.section[boot_entry]->linux_path;
	err = load_binary_with_sig(fm_handle,
							   TEGRABL_BINARY_KERNEL,
							   BOOT_IMAGE_MAX_SIZE,
							   linux_path,
							   *boot_img_load_addr,
							   kernel_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to load kernel, abort booting.\n");
		goto fail;
	}

#if defined(CONFIG_DT_SUPPORT)
	uint32_t dtb_size;
	char *dtb_path = NULL;

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_KERNEL, dtb_load_addr);
	if ((err == TEGRABL_NO_ERROR) && (*dtb_load_addr !=  NULL)) {
		goto fail;
	}

	dtb_path = extlinux_conf.section[boot_entry]->dtb_path;
	err = load_binary_with_sig(fm_handle,
							   TEGRABL_BINARY_KERNEL_DTB,
							   DTB_MAX_SIZE,
							   dtb_path,
							   *dtb_load_addr,
							   &dtb_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to load kernel-dtb, abort booting.\n");
		goto fail;
	}
#endif	/* CONFIG_DT_SUPPORT */

fail:
	for (entry = 0; entry < extlinux_conf.num_boot_entries; entry++) {
		tegrabl_free(extlinux_conf.section[entry]);
	}
	return err;
}

tegrabl_error_t extlinux_boot_load_ramdisk(struct tegrabl_fm_handle *fm_handle,
										   void **ramdisk_load_addr,
										   uint64_t *ramdisk_size)
{
	uint32_t file_size = RAMDISK_MAX_SIZE;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if ((fm_handle == NULL) || (ramdisk_load_addr == NULL) || (ramdisk_size == NULL)) {
		pr_error("Invalid args passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (g_ramdisk_path == NULL) {
		pr_warn("Ramdisk image path not found\n");
		goto fail;
	}

	pr_info("Loading ramdisk from rootfs ...\n");
	*ramdisk_load_addr = (void *)tegrabl_get_ramdisk_load_addr();
	err = tegrabl_fm_read(fm_handle,
						  g_ramdisk_path,
						  NULL,
						  *ramdisk_load_addr,
						  &file_size,
						  NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	*ramdisk_size = file_size;

fail:
	return err;
}

tegrabl_error_t extlinux_boot_update_bootargs(void *kernel_dtb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	err = tegrabl_linuxboot_update_bootargs(kernel_dtb, g_boot_args);
	if (err != 0) {
		pr_warn("Failed to update DTB bootargs with extlinux.conf\n");
	}

	return err;
}

void extlinux_boot_set_status(bool status)
{
	extlinux_boot_is_success = status;
}

bool extlinux_boot_get_status(void)
{
	return extlinux_boot_is_success;
}

#endif /* CONFIG_ENABLE_EXTLINUX_BOOT */
