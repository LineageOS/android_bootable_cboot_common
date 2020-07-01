/*
 * Copyright (c) 2014-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "build_config.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_compiler.h>
#include <tegrabl_io.h>
#include <tegrabl_linuxboot.h>
#include <tegrabl_linuxboot_helper.h>

#ifdef CONFIG_ENABLE_DISPLAY
#include <tegrabl_display.h>
#endif

#define TOSTR(s)       #s
#define STRINGIFY(s)   TOSTR(s)

#define COMMAND_LINE_SIZE 1024

static char s_cmdline[COMMAND_LINE_SIZE];

#define UART_BAUD 115200

#define ODMDATA_FIELD(field) \
	(void *)((uintptr_t)((ODMDATA_##field##_SHIFT << 16) | \
				(ODMDATA_##field##_WIDTH)))


static int tegrabl_linuxboot_add_carveout(char *cmdline, int len,
										  char *param, void *priv)
{
	struct tegrabl_linuxboot_memblock memblock;
	uint32_t carveout_type = (uint32_t)((uintptr_t)priv);

	if (!cmdline || !param) {
		return -1;
	}

	if (tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_CARVEOUT,
			&carveout_type, &memblock) != TEGRABL_NO_ERROR) {
		return -1;
	}

	if (memblock.size)
		return tegrabl_snprintf(cmdline, len, "%s=0x%" PRIx64 "@0x%08" PRIx64
								" ", param, memblock.size, memblock.base);
	else
		return 0;
}

static int tegrabl_linuxboot_add_nvdumper_info(char *cmdline, int len,
											   char *param, void *priv)
{
	struct tegrabl_linuxboot_memblock memblock;
	uint32_t carveout_type = TEGRABL_LINUXBOOT_CARVEOUT_NVDUMPER;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

	if (tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_CARVEOUT,
			&carveout_type, &memblock) != TEGRABL_NO_ERROR) {
		return -1;
	}

	if (memblock.size)
		return tegrabl_snprintf(cmdline, len, "%s=0x%08" PRIx64 " ",
								param, memblock.base);
	else
		return 0;
}

static int tegrabl_linuxboot_add_earlycon(char *cmdline, int len,
										  char *param, void *priv)
{
	enum tegrabl_linuxboot_debug_console console;
	uint64_t early_uart_addr;
	tegrabl_error_t status;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

	if (tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_DEBUG_CONSOLE,
			NULL, &console) != TEGRABL_NO_ERROR) {
		return -1;
	}

	pr_debug("%s: console = %u\n", __func__, console);

	if (console != TEGRABL_LINUXBOOT_DEBUG_CONSOLE_NONE) {
		status = tegrabl_linuxboot_helper_get_info(
			TEGRABL_LINUXBOOT_INFO_EARLYUART_BASE,	NULL, &early_uart_addr);
		if (status != TEGRABL_NO_ERROR) {
			return 0;
		}
		pr_debug("%s: early_uartbase = 0x%lx\n", __func__, early_uart_addr);
		return tegrabl_snprintf(cmdline, len, "%s=uart8250,mmio32,0x%08"PRIx64
								" ", param, early_uart_addr);
	}

	return 0;
}

int tegrabl_linuxboot_add_string(char *cmdline, int len,
								 char *param, void *priv)
{
	if (!cmdline || !param) {
		return -1;
	}

	if (priv)
		return tegrabl_snprintf(cmdline, len, "%s=%s ", param, (char *)priv);
	else
		return tegrabl_snprintf(cmdline, len, "%s ", param);
}

int tegrabl_linuxboot_add_number(char *cmdline, int len,
								 char *param, void *priv)
{
	uint64_t *num = (uint64_t *)priv;

	if (!cmdline || !param) {
		return -1;
	}

	if (num)
		return tegrabl_snprintf(cmdline, len, "%s=%#" PRIx64 " ", param, *num);
	else
		return -1;
}

#ifdef CONFIG_ENABLE_DISPLAY
static int tegrabl_linuxboot_add_disp_param(char *cmdline, int len, char *param,
											void *priv)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_display_unit_params disp_params;
	uint32_t du_idx = 0;

	TEGRABL_UNUSED(priv);

	pr_debug("%s: adding disp_params for %s\n", __func__, param);

	for (du_idx = 0; du_idx < DISPLAY_OUT_MAX; du_idx++) {
		err = tegrabl_display_get_params(du_idx, &disp_params);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s, du %d failed to get display params\n",
					 __func__, du_idx);
			goto fail;
		}

		if (disp_params.size != 0) {
			if (!strcmp(param, "tegra_fbmem") && (disp_params.instance == 0)) {
				return tegrabl_snprintf(cmdline, len, "%s=0x%x@0x%08"PRIx64" "
						"lut_mem=0x%x@0x%08"PRIx64" ", param,
						disp_params.size, disp_params.addr,
						disp_params.lut_size, disp_params.lut_addr);
			} else if (!strcmp(param, "tegra_fbmem2") &&
					(disp_params.instance == 1)) {
				return tegrabl_snprintf(cmdline, len, "%s=0x%x@0x%08"PRIx64" "
						"lut_mem2=0x%x@0x%08"PRIx64" ", param, disp_params.size,
						disp_params.addr, disp_params.lut_size,
						disp_params.lut_addr);
			} else if (!strcmp(param, "tegra_fbmem3") &&
					(disp_params.instance == 2)) {
				return tegrabl_snprintf(cmdline, len, "%s=0x%x@0x%08"PRIx64" "
						"lut_mem3=0x%x@0x%08"PRIx64" ", param, disp_params.size,
						disp_params.addr, disp_params.lut_size,
						disp_params.lut_addr);
			}
		}
	}

fail:
	return -1;
}
#endif

static int tegrabl_linuxboot_add_secureos_name(char *cmdline, int len,
	char *param, void *priv)
{
	enum tegrabl_tos_type tos_type;
	char *tos_name = "none\0";

	TEGRABL_UNUSED(priv);
	TEGRABL_UNUSED(len);

	if (tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_SECUREOS,
			NULL, &tos_type) != TEGRABL_NO_ERROR)
		return -1;

	switch (tos_type) {
	case TEGRABL_TOS_TYPE_TLK:
		tos_name = "tlk\0";
		break;

	case TEGRABL_TOS_TYPE_TRUSTY:
		tos_name = "trusty\0";
		break;

	default:
		break;
	}

	return tegrabl_snprintf(cmdline, len, "%s=%s ", param, tos_name);
}

static struct tegrabl_linuxboot_param common_params[] = {
	{ "memtype", tegrabl_linuxboot_add_string, "0" },
	{ "tzram", tegrabl_linuxboot_add_carveout,
		(void *)((uintptr_t)TEGRABL_LINUXBOOT_CARVEOUT_TOS) },
	{ "video", tegrabl_linuxboot_add_string, "tegrafb" },
	{ "no_console_suspend", tegrabl_linuxboot_add_string, "1" },
	{ "earlycon", tegrabl_linuxboot_add_earlycon, NULL},
	{ "nvdumper_reserved", tegrabl_linuxboot_add_nvdumper_info, NULL },
	{ "gpt", tegrabl_linuxboot_add_string, NULL },
#ifdef CONFIG_ENABLE_DISPLAY
	{ "tegra_fbmem", tegrabl_linuxboot_add_disp_param, NULL },
	{ "tegra_fbmem2", tegrabl_linuxboot_add_disp_param, NULL },
	{ "tegra_fbmem3", tegrabl_linuxboot_add_disp_param, NULL },
#endif
#if !defined(CONFIG_OS_IS_L4T)
	{ "androidboot.secureos", tegrabl_linuxboot_add_secureos_name, NULL },
#endif
	{ NULL, NULL, NULL},
};

#define IGNORE_FASTBOOT_CMD			"ignorefastboot"

static int n_orig_cmdlen, n_ignore_fastboot;
static const char * const ignore_fastboot_cmd = IGNORE_FASTBOOT_CMD;

static bool iswhitespace(int ch)
{
	if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
		return true;
	else
		return false;
}

static bool check_ignore_fastboot_cmd(const char *cmd, int length,
									  int start, int end)
{
	if ('"' == s_cmdline[start]) {
		if (length == end - start - 2 &&
			'"' == s_cmdline[end - 1])
			n_ignore_fastboot = !memcmp(&s_cmdline[start + 1], cmd, length);
	} else {
		if (length == end - start)
			n_ignore_fastboot = !memcmp(&s_cmdline[start], cmd, length);
	}

	return n_ignore_fastboot;
}

/* TODO: prepare a BST of params based on initcmdline */
static uint32_t init_cmd_list(const char *cmdline)
{
	int32_t i, next, mood = 0, quote = 0, start = 0;
	uint8_t curr_char;
	int32_t ignore_fastboot_cmdlen = strlen(ignore_fastboot_cmd);

	if (!cmdline) { /* Nothing to do. Early return */
		s_cmdline[0] = '\0';
		return 0;
	}

	for (i = next = 0; i < COMMAND_LINE_SIZE; i++) {
		curr_char = cmdline[i];
		if (0 == mood) {
			if (!iswhitespace(curr_char)) {
				if (curr_char) {
					s_cmdline[start = next++] = curr_char;
					if ('"' == curr_char) /* quote */ {
						quote = !0;
					}
					mood = 1;
					continue;
				} else {
					break;
				}
			}
		} else if (1 == mood) {
			if (iswhitespace(curr_char)) {
				if (quote) {/* quoted -> space does not count as a separator */
					s_cmdline[next++] = curr_char;
				} else { /* otherwise, end of a command */
					/* check for fastboot ignore command */
					if (!n_ignore_fastboot) {
						if (check_ignore_fastboot_cmd(ignore_fastboot_cmd,
													  ignore_fastboot_cmdlen,
													  start, next))
							/* and do not include it in final command line */
							next = start;
					}
					if (!n_ignore_fastboot) {
						s_cmdline[next++] = ' ';
					}
					mood = 0;
				}
				continue;
			} else if ('"' == curr_char) {/* quote character */
				s_cmdline[next++] = curr_char, quote = !quote;
			} else {
				if (curr_char) { /* non-space, non-quote, non-null */
					s_cmdline[next++] = curr_char;
					continue;
				} else {/* null */
					/* check for fastboot ignore command */
					if (!n_ignore_fastboot) {
						if (check_ignore_fastboot_cmd(ignore_fastboot_cmd,
													  ignore_fastboot_cmdlen,
													  start, next))
							/* and do not include it in final command line */
							next = start;
					}
					break;
				}
			}
		}
	}

	if (next) {
		/* if quoted, complete it */
		if (quote)
			s_cmdline[next++] = '"';

		/* check for fastboot ignore command */
		if (!n_ignore_fastboot) {
			if (check_ignore_fastboot_cmd(ignore_fastboot_cmd,
										  ignore_fastboot_cmdlen,
										  start, next))
				/* and do not include it in final command line */
				next = start;
		}

		/* add an space at the end, if necessary */
		if (' ' != s_cmdline[next - 1])
			s_cmdline[next++] = ' ';
	}

	s_cmdline[next] = 0; /* The END */

	return n_orig_cmdlen = next;
}

/* Check if given param is already present in the cmdline passed to
 * init_cmd_list() */
static int32_t does_command_exist(const char *command)
{
	int32_t i, mood = 0, quote = 0;
	uint8_t curr_char;
	const char *p = command;

	/* ignore fastboot */
	if (n_ignore_fastboot) {
		return !0;
	}
	/* nothing to find */
	if (!p || !*p) {
		return 0;
	}

	for (i = 0; i < n_orig_cmdlen; i++) {
		curr_char = s_cmdline[i];
		if (0 == mood) { /* skip space */
			if ('"' == curr_char) /* quote at the begining of a cmd */
				mood = 4, quote = !0;
			else if (*p == curr_char)
				p++, mood = 1;
			else
				mood = 2;
			continue;
		} else if (1 == mood) { /* try matching cmd */
			if (*p == curr_char) { /* match character */
				if ('"' == curr_char) /* keep the quote logic happy */ {
					quote = !quote;
				}
				p++;
				continue;
			} else if (*p) { /* match failed */
				if (' ' == curr_char) { /* separator ? */
					if (quote) /* not end of cmd in quote */
						mood = 2; /* wait until the next cmd */
					else /* end of cmd, try next */
						p = command, mood = 0;
				} else { /* not a seperator */
					if ('"' == curr_char) /* keep the quote logic happy */ {
						quote = !quote;
					}
					mood = 2; /* wait until the next cmd */
				}
				continue;
			} else { /* *p == 0 -> end of cmd, we are searching, cmd match? */
				if (quote) { /* quoted */
					if ('=' == curr_char) {
						return !0; /* a real match */
					}
					if ('"' == curr_char) /* and an end quote */
						quote = !quote, mood = 3; /* not yet, but
													 there's a chance */
					else
						mood = 2; /* alas, wait until the next cmd */
				} else if (' ' == curr_char || '=' == curr_char) {
					return !0; /* a real match */
				} else { /* no match */
					if ('"' == curr_char) /* keep the quote logic happy */ {
						quote = !quote;
					}
					mood = 2; /* wait until the next cmd */
				}
			}
		} else if (2 == mood) { /* wait until the next cmd */
			if ('"' == curr_char) /* quote */
				quote = !quote;
			else if (!quote && ' ' == curr_char) /* end of a cmd */
				p = command, mood = 0; /* try the next cmd */
			continue;
		} else if (3 == mood) { /* matched cmd string while quoted */
			/* match, as quote ends right */
			if ('=' == curr_char || ' ' == curr_char)
				return !0; /* a real match */
			else  /* no match, wait until the next cmd */
				mood = 2;
			continue;
		} else if (4 == mood) {
			if (*p == curr_char) /* match, try rest */
				p++, mood = 1;
			else /* no match, wait until the next cmd */
				mood = 2;
			continue;
		}
	}

	return 0;
}

char *tegrabl_linuxboot_prepare_cmdline(char *initcmdline)
{
	char *ptr = NULL;
	int idx, remain;
	uint32_t i;
	struct tegrabl_linuxboot_param *extra_params = NULL;

	memset(s_cmdline, 0, sizeof(s_cmdline));
	i = init_cmd_list(initcmdline);

	remain = sizeof(s_cmdline) / sizeof(char) - i;
	ptr = &s_cmdline[i];

	for (i = 0; common_params[i].str != NULL; i++) {
		if (!does_command_exist(common_params[i].str)) {
			if (common_params[i].append == NULL) {
				pr_error("No append-handler for '%s'\n", common_params[i].str);
				continue;
			}
			idx = common_params[i].append(ptr, remain,
					common_params[i].str, common_params[i].priv);
			if ((idx > 0) && (idx <= remain)) {
				remain -= idx;
				ptr += idx;
			}
		}
		pr_debug("Cmdline: %s\n", s_cmdline);
	}

	if ((tegrabl_linuxboot_helper_get_info(
			TEGRABL_LINUXBOOT_INFO_EXTRA_CMDLINE_PARAMS,
			NULL, &extra_params) == TEGRABL_NO_ERROR) && (extra_params)) {
		pr_debug("%s: extra_params: %p\n", __func__, extra_params);
		for (i = 0; extra_params[i].str != NULL; i++) {
			if (!does_command_exist(extra_params[i].str)) {
				if (extra_params[i].append == NULL) {
					pr_error("No append-handler for '%s'\n",
							 extra_params[i].str);
					continue;
				}
				idx = extra_params[i].append(ptr, remain,
						extra_params[i].str, extra_params[i].priv);
				if ((idx > 0) && (idx <= remain)) {
					remain -= idx;
					ptr += idx;
				}
			}
		}
		pr_debug("Cmdline: %s\n", s_cmdline);
	}
	pr_info("Linux Cmdline: %s\n", s_cmdline);

	return s_cmdline;
}

