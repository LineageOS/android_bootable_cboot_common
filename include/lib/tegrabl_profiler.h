/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_PROFILER_H
#define INCLUDED_TEGRABL_PROFILER_H

#include "build_config.h"
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_compiler.h>

#define TEGRABL_PROFILER_PAGE_SIZE	(64U * 1024U)

#define MB1_PROFILER_OFFSET		0x0U
#define MB1_PROFILER_SIZE		(8U * 1024U)

#define MB2_PROFILER_OFFSET		(MB1_PROFILER_OFFSET + MB1_PROFILER_SIZE)
#define MB2_PROFILER_SIZE		(4U * 1024U)

#define BPMPFW_PROFILER_OFFSET	(MB2_PROFILER_OFFSET + MB2_PROFILER_SIZE)
#define BPMPFW_PROFILER_SIZE	(4U * 1024U)

#define TOS_PROFILER_OFFSET		(BPMPFW_PROFILER_OFFSET + BPMPFW_PROFILER_SIZE)
#define TOS_PROFILER_SIZE		(4U * 1024U)

#define CPUBL_PROFILER_OFFSET		(TOS_PROFILER_OFFSET + TOS_PROFILER_SIZE)
#define CPUBL_PROFILER_SIZE		(4U * 1024U)

#define MAX_PROFILE_STRLEN	55U

/*
 * @brief enums to record various profiler levels
 */
/* macro tegrabl profiler level */
typedef uint32_t tegrabl_profiler_level_t;
/* used to captures entry/exit and any important profiling pts */
#define PROFILER_RECORD_MINIMAL 0U

/* used to capture detailed profiling pts */
#define PROFILER_RECORD_DETAILED 1U

#if !defined(CONFIG_PROFILER_RECORD_LEVEL)
#define CONFIG_PROFILER_RECORD_LEVEL PROFILER_RECORD_DETAILED
#endif

#if defined(CONFIG_BOOT_PROFILER)
/**
 * @brief Initializes the profiler library
 *
 * @param page_addr Address of the page storing profiler data
 * @param offset Offset in profiler_page where local profiling data must start
 * @param size Size of the profiling region to be used for local profiling
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise an appropriate error-code
 */
tegrabl_error_t tegrabl_profiler_init(uint64_t page_addr,
									  uint32_t offset,
									  uint32_t size);

/**
 * @brief Copies the profiling data into a new location and records the
 * copy overhead
 *
 * @param new_profiler_page_addr New location of the profiling data
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise an appropriate error code
 */
tegrabl_error_t tegrabl_profiler_relocate(uintptr_t new_profiler_page_addr);

/**
 * @brief Prints the profiler information to UART
 */
void tegrabl_profiler_dump(void);

/**
 * @brief returns timestamp of particular profiler record
 */
uint64_t tegrabl_get_profile_record_timestamp(const char *str);

/**
 * @brief core api to add the profiler record
 *
 * @param str describes the profiling point
 * @params tstamp timestamp of the profiling point. If tstamp == 0, the
 *                function captures the time stamp and adds to the record
 *
 */
void tegrabl_profiler_add_record(const char *str, uint64_t tstamp);

#else

static inline tegrabl_error_t tegrabl_profiler_init(uint64_t page_addr,
													uint32_t offset,
													uint32_t size)
{
	TEGRABL_UNUSED(page_addr);
	TEGRABL_UNUSED(offset);
	TEGRABL_UNUSED(size);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_profiler_relocate(
		uintptr_t new_profiler_data_addr)
{
	TEGRABL_UNUSED(new_profiler_data_addr);

	return TEGRABL_NO_ERROR;
}

static inline void tegrabl_profiler_dump(void)
{
}

static inline void tegrabl_profiler_add_record(const char *str, uint64_t tstamp)
{
	TEGRABL_UNUSED(str);
	TEGRABL_UNUSED(tstamp);
}

#endif

/*
 * @brief helper macros for profiling
 */
#define tegrabl_profiler_record(str, tstamp, level)						\
do {																	\
		if (PROFILER_RECORD_##level <= CONFIG_PROFILER_RECORD_LEVEL) {	\
			tegrabl_profiler_add_record(str, tstamp);					\
		}																\
} while (0)

#endif /* INCLUDED_TEGRABL_PROFILER_H */
