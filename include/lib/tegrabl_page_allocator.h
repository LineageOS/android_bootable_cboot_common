/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file tegrabl_page_allocator.h
 * @brief This file lists the public interface of the page allocator module.
 * The page allocator always allocates memory in multiple of fixed-size pages.
 * It can be used to manage multiple pools of memory, each of which can have
 * multiple non-overlapping regions of memory blocks.
 */
#ifndef INCLUDED_TEGRABL_PAGE_ALLOCATOR_H
#define INCLUDED_TEGRABL_PAGE_ALLOCATOR_H

#include "build_config.h"
#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>

/**
 * @brief Log2 of page size
 */
#define PAGE_SIZE_LOG2	CONFIG_PAGE_SIZE_LOG2

/**
 * @brief page size is the smallest unit of memory in which page allocator would
 * allocate memory
 */
#define PAGE_SIZE		(1ULL << PAGE_SIZE_LOG2)

/**
 * @brief Id of memory pools.
 */
/* macro tegrabl memory context pool */
typedef uint32_t tegrabl_memory_context_pool_t;
#define TEGRABL_MEMORY_POOL1 0UL
#define TEGRABL_MEMORY_POOL2 1UL
#define TEGRABL_MEMORY_POOL_MAX 2UL

/**
 * @brief Determines whether the page allocator allocates memory from the
 * highest address of the pool or the lowest address.
 */
/* macro tegrabl memory direction */
typedef uint32_t tegrabl_memory_direction_t;
#define TEGRABL_MEMORY_START 0U
#define TEGRABL_MEMORY_END 1U

/**
 * @brief Defines the address-range of a memory block
 */
struct tegrabl_memory_block {
	/** Lowest address of the memory block */
	uint64_t start;
	/** Highest address of the memory block */
	uint64_t end;
};

/**
 * @brief Describes a memory pool
 */
struct tegrabl_pool_data {
	/** Size of memory reserved for allocator meta-data of the pool */
	uint64_t context_mem_size;
	/** Start address of memory reserved for allocator meta-data of the pool.
		This memory should not be part of the same pool
	 */
	uint64_t context_mem_ptr;
	/** List of the memory blocks to be assigned to the pool */
	struct tegrabl_memory_block *block_info;
	/** Number of memory blocks in the list */
	uint32_t num_blocks;
};

/**
 * @brief Returns the address of context of given memory pool
 *
 * @param pool Target memory pool.
 *
 * @return Non zero address if successful else zero.
 */
uint64_t tegrabl_page_allocator_get_context(
		tegrabl_memory_context_pool_t pool);

/**
 * @brief Sets the address of pre-initialized context of a memory pool. This
 * would typically be used to avoid re-initialization of page allocator across
 * boot-binaries
 *
 * @param pool Target memory pool
 * @param address Address of memory context
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_page_allocator_set_context(
		tegrabl_memory_context_pool_t pool, uint64_t address);

/**
 * @brief Creates a memory context with specified list of free memory regions
 * and excludes any bad pages.
 *
 * @param pool_init_info List of the configuration required for pool
 * initialization
 *
 * @return TEGRABL_NO_ERROR if successfully initialized else appropriate error.
 */
tegrabl_error_t tegrabl_page_allocator_init(
									struct tegrabl_pool_data *pool_init_info);

/**
 * @brief Prints all free block information.
 *
 * @param pool Type of memory pool.
 */
void tegrabl_page_alloc_dump_free_blocks(
		tegrabl_memory_context_pool_t pool);


/**
 * @brief Prints all free list information.
 *
 * @param pool Type of memory pool.
 */
void tegrabl_page_alloc_dump_free_list(
		tegrabl_memory_context_pool_t pool);

/**
 * @brief Allocates memory from specific context and return the reference to
 * allocated memory. Tries to allocate memory having preferred address. If such
 * memory cannot be found then it will allocate different memory as per
 * alignment requirement.
 *
 * @param pool Pool from which memory to be allocated.
 * @param size Size to be allocated.
 * @param alignment For aligned memory.
 * @param preferred_base Try to allocate memory with this base address.
 * @param direction Direction to look for free block.
 *
 * @return Address of allocated memory if successful else 0.
 */
uint64_t tegrabl_page_alloc(tegrabl_memory_context_pool_t pool,
		uint64_t size, uint64_t alignment, uint64_t preferred_base,
		tegrabl_memory_direction_t direction);

/**
 * @brief Returns the base address and size of a particular free-block in the
 * specified context.
 *
 * @param pool Pool from which memory to be allocated.
 * @param idx The index of the free-block
 * @param base Base address of the free-block (output)
 * @param size Size of the free-block (output)
 */
void tegrabl_page_get_freeblock(tegrabl_memory_context_pool_t pool,
								uint32_t idx, uint64_t *base, uint64_t *size);

/**
 * @brief Adds the memory into free pool so that it can be re-allocated.
 *
 * @param pool Context in which memory to be returned.
 * @param ptr Start address of the memory to be freed.
 * @param size Size of the memory to be freed.
 */
void tegrabl_page_free(tegrabl_memory_context_pool_t pool,
				uint64_t ptr, size_t size);

/**
 * @brief Remove multiple pages from the given memory pool.
 *
 * @param pool Target pool from where pages will be removed
 * @param page_list Pointer to the list of pages to be removed.
 * @param page_count Number of pages in page list.
 *
 * @note This API expects a sorted memory block table as input
 */
tegrabl_error_t tegrabl_page_allocator_remove_pages(
									tegrabl_memory_context_pool_t pool,
									uint64_t *page_list, uint32_t page_count);

/**
 * @brief De-initialize page allocator to prevent any further allocation and
 * free up the context memory.
 *
 * @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_page_allocator_deinit(void);

#endif /* INCLUDED_TEGRABL_PAGE_ALLOCATOR_H */

