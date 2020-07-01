/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_CACHE_H
#define INCLUDED_TEGRABL_CACHE_H

#define DCACHE 0x1
#define ICACHE 0x2
#define UCACHE 0x3

#if !defined(_ASSEMBLY_)

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Disable the specified caches
 *
 * @param flags Following flags specifying which caches to be enabled:
 * DCACHE - disable data cache
 * ICACHE - disable instruction cache
 * UCACHE - disable both data and instruction cache
 */
void tegrabl_arch_disable_cache(uint32_t flags);

/**
 * @brief Enable the specified caches
 *
 * @param flags Following flags specifying which caches to be enabled:
 * DCACHE - enable data cache
 * ICACHE = enable instruction cache
 * UCACHE - enable both data and instruction cache
 */
void tegrabl_arch_enable_cache(uint32_t flags);

/**
 * @brief Clean the data-cache lines (i.e. write the dirty data-cache lines to
 * the memory), corresponding to the given VA range.
 * @note Entire cache-line would be written back.
 *
 * @param start Starting VA of the address-range that needs to be cleaned
 * @param len Length of the address-range that needs to be cleaned
 */
void tegrabl_arch_clean_dcache_range(uintptr_t start, size_t len);

/**
 * @brief Clean the data-cache lines (i.e. write the dirty data-cache lines to
 * the memory) and mark them as invalid, corresponding to the given VA range.
 * @note Entire cache-line would be written back and invalidated.
 *
 * @param start Starting VA of the address-range that needs to be cleaned and
 * invalidated
 * @param len Length of the address-range that needs to be cleaned and
 * invalidated
 */
void tegrabl_arch_clean_invalidate_dcache_range(uintptr_t start, size_t len);

/**
 * @brief Invalidate the data-cache lines, corresponding to the given VA range.
 * @note Entire cache-lines would be invalidated.
 * @note ARM implementations are free to write-back the dirty cache-lines back
 * to memory before invalidating, so make sure that there are no dirty-cache
 * lines.
 *
 * @param start Starting VA of the address-range that needs to be invalidated
 * @param len Length of the address-range that needs to be invalidated
 */
void tegrabl_arch_invalidate_dcache_range(uintptr_t start, size_t len);

/**
 * @brief Flushes the data-cache corresponding to the given VA range and
 * invalidates the instruction-cache.
 * @note Entire cache-lines would be flushed.
 *
 * @param start Starting VA of the address-range that needs to be flushed
 * @param len Length of the address-range that needs to be flushed
 */
void tegrabl_arch_sync_cache_range(uintptr_t start, size_t len);

#endif /* _ASSEMBLY_ */

#endif /* INCLUDED_TEGRABL_CACHE_H */
