/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_NO_MODULE

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <tegrabl_profiler.h>

struct profiler_record {
	char str[MAX_PROFILE_STRLEN + 1];
	int64_t timestamp;
};

/* Note: As these functions are called usually with log-level set such that
 * only critical errors are printed, rest of the logs would be disabled.
 * Hence, tegrabl_printf() is used intentionally in this library.
 */

static struct profiler_record *profiler_page_base;
static struct profiler_record *local_profiler_data;
static uint32_t profiler_count;
static uint32_t profiler_limit;

void tegrabl_profiler_add_record(const char *str, int64_t tstamp)
{
	if (profiler_count >= profiler_limit) {
		tegrabl_printf("profiler entries reached limit\n");
		return;
	}

	if ((local_profiler_data == NULL) || (profiler_page_base == NULL)) {
		tegrabl_printf("Profiler not initialized\n");
		return;
	}

	if (tstamp) {
		local_profiler_data[profiler_count].timestamp = tstamp;
	} else {
		local_profiler_data[profiler_count].timestamp =
			tegrabl_get_timestamp_us();
	}

	if (str) {
		strncpy(local_profiler_data[profiler_count].str, str,
				MAX_PROFILE_STRLEN);
		local_profiler_data[profiler_count].str[MAX_PROFILE_STRLEN - 1] = '\0';
	}
	profiler_count++;
}

tegrabl_error_t tegrabl_profiler_relocate(uintptr_t new_profiler_page_addr)
{
	struct profiler_record *relocated_profiler_page_base =
		(struct profiler_record *)(uintptr_t)new_profiler_page_addr;
	int local_offset = local_profiler_data - profiler_page_base;

	if (relocated_profiler_page_base == NULL) {
		tegrabl_printf("invalid profiling data address\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	/* clear the entire 64KB page */
	memset(relocated_profiler_page_base, 0, TEGRABL_PROFILER_PAGE_SIZE);

	/* copy only the local profiling data */
	memmove(relocated_profiler_page_base,
			local_profiler_data,
			profiler_limit * sizeof(struct profiler_record));

	profiler_page_base = relocated_profiler_page_base;
	local_profiler_data = profiler_page_base + local_offset;

	tegrabl_profiler_record("- move profile data", 0, DETAILED);

	return TEGRABL_NO_ERROR;
}

void tegrabl_profiler_dump(void)
{
	uint32_t i;
	int64_t prev_timestamp = 0;
	tegrabl_printf("Profiler Dump (@ %p):\n", profiler_page_base);
	tegrabl_printf("%3s| %10s | %10s | %s\n", "---", "----------", "---------", "---------------");
	tegrabl_printf("%3s| %10s | %10s | %s\n", "   ", "tstamp(us)", "delta(us)", "   stage       ");
	tegrabl_printf("%3s| %10s | %10s | %s\n", "---", "----------", "---------", "---------------");

	for (i = 0; i < 1024; i++) {
		if (profiler_page_base[i].timestamp == 0LL) {
			continue;
		}

		tegrabl_printf("%3u| %10"PRId64" | %9"PRId64" | %s\n", i,
					   profiler_page_base[i].timestamp,
					   profiler_page_base[i].timestamp - prev_timestamp,
					   profiler_page_base[i].str);

		prev_timestamp = profiler_page_base[i].timestamp;
	}
	tegrabl_printf("%3s| %10s | %10s | %s\n", "---", "----------", "---------", "---------------");
}

tegrabl_error_t tegrabl_profiler_init(uint64_t page_addr,
									  uint32_t offset,
									  uint32_t size)
{
	profiler_page_base = (void *)(uintptr_t)page_addr;
	local_profiler_data = (void *)(uintptr_t)(page_addr + offset);
	profiler_count = 0;
	profiler_limit = size / sizeof(struct profiler_record);

	if ((local_profiler_data == NULL) || (profiler_page_base == NULL)) {
		tegrabl_printf("invalid profiling data address\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}
