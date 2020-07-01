/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_PAGE_ALLOCATOR_H
#define INCLUDED_TEGRABL_PAGE_ALLOCATOR_H

#include "build_config.h"
#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>

#define PAGE_SIZE_LOG2	CONFIG_PAGE_SIZE_LOG2
#define PAGE_SIZE		(1 << PAGE_SIZE_LOG2)

/**
 * @brief Defines the different type of memory contexts.
 */
enum tegrabl_memory_context_type {
	TEGRABL_MEMORY_DRAM,
	TEGRABL_MEMORY_TYPE_MAX
};

/**
 * @brief Defines the from where to look for memory from end or
 * from beginnig.
 */
enum tegrabl_memory_direction {
	TEGRABL_MEMORY_START,
	TEGRABL_MEMORY_END
};

/**
 * @brief Returns the address of context of given type
 *
 * @param type Type of context.
 *
 * @return Non zero address if successful else zero.
 */
uint64_t tegrabl_page_allocator_get_context(
		enum tegrabl_memory_context_type type);

/**
 * @brief Sets the address of context which will
 * be later used by tegrabl_page_alloc() and
 * tegrabl_page_free().
 *
 * @param type Type of memory context
 * @param address Address of memory context
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_page_allocator_set_context(
		enum tegrabl_memory_context_type type, uint64_t address);

/**
 * @brief Creates a memory context with specified list of free memory regions
 * and excludes any bad pages.
 *
 * @param type Type of memory context
 * @param start Start of memory context.
 * @param end End of memory context.
 * @param bad_pages List of bad pages which needs to be neglected while
 * allocating memory. Last entry should be 0.
 * @param addr_for_context By default information about context will be saved
 * in first page. If different location is to be used to save the context
 * then specify here.
 * @param size_for_context Maximum size which can be used for saving data for
 * context..
 *
 * @return TEGRABL_NO_ERROR if successfully initialized else appropriate error.
 */
tegrabl_error_t tegrabl_page_allocator_init(
		enum tegrabl_memory_context_type type, uint64_t start,
		uint64_t end, uint64_t *bad_pages,
		uint64_t addr_for_context, uint64_t size_for_context);


/**
 * @brief Prints all free block information.
 *
 * @param type Type of memory context.
 */
void tegrabl_page_alloc_dump_free_blocks(
		enum tegrabl_memory_context_type type);


/**
 * @brief Prints all free list information.
 *
 * @param type Type of memory context.
 */
void tegrabl_page_alloc_dump_free_list(
		enum tegrabl_memory_context_type type);

/**
 * @brief Allocates memory from specific context and return the reference to
 * allocated memory. Tries to allocate memory having preferred address. If such
 * memory cannot be found then it will allocate different memory as per
 * alignment requirement.
 *
 * @param type Context from which memory to be allocated.
 * @param size Size to be allocated.
 * @param alignment For aligned memory.
 * @param preferred_base Try to allocate memory with this base address.
 * @param direction Direction to look for free block.
 *
 * @return Address of allocated memory if successful else 0.
 */
uint64_t tegrabl_page_alloc(enum tegrabl_memory_context_type type,
		size_t size, uint64_t alignment, uint64_t preferred_base,
		enum tegrabl_memory_direction direction);

/**
 * @brief Returns the base address and size of a particular free-block in the
 * specified context.
 *
 * @param type Context from which memory to be allocated.
 * @param idx The index of the free-block
 * @param base Base address of the free-block (output)
 * @param size Size of the free-block (output)
 */
void tegrabl_page_get_freeblock(enum tegrabl_memory_context_type type,
								uint32_t idx, uint64_t *base, uint64_t *size);

/**
 * @brief Adds the memory into free pool so that it can be re-allocated.
 *
 * @param type Context in which memory to be returned.
 * @param ptr Address to be freed.
 * @param size Size of the memory to be freed.
 */
void tegrabl_page_free(enum tegrabl_memory_context_type type,
				uint64_t ptr, size_t size);

#endif /* INCLUDED_TEGRABL_PAGE_ALLOCATOR_H */

