/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_HEAP

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>

/**
 * @brief Magic number for free memory block.
 */
#define FREE_MAGIC 0xDEEFBEEE

/**
 * @brief Magic number for allocated memory blocks.
 */
#define ALLOC_MAGIC 0xDEADBEEE

/**
 * @brief Minimum size of allocation. This size should be more than
 * size required to store information about free block.
 */
#define MIN_SIZE sizeof(tegrabl_heap_free_block_t)

/**
 * @brief Information describing a free/unallocated block of memory.
 */
typedef struct tegrabl_heap_free_block {
	uint32_t magic; /**< magic identifier for free block */
	size_t size; /**< size of the free block, this includes the header */
	struct tegrabl_heap_free_block *prev; /**< pointer to previous free block */
	struct tegrabl_heap_free_block *next; /**< pointer to next free block */
} tegrabl_heap_free_block_t;

/**
 * @brief Information describing the allocated memory. This is used while
 * freeing memory.
 */
typedef struct tegrabl_heap_alloc_block {
	uint32_t magic; /**< magic identifier for allocated block */
	size_t size; /**< total size of allocated block*/
	void *start; /**< start of the allocated block. */
	/* Note that the start of allocated block and what is returned tp caller
	 * of malloc APIs is different */
} tegrabl_heap_alloc_block_t;

/**
 * @brief Head of doubly linked list of free memory block.
 * List is sorted in ascending order of memory address.
 */
static tegrabl_heap_free_block_t *tegrabl_heap_free_list[TEGRABL_HEAP_TYPE_MAX];

tegrabl_error_t tegrabl_heap_init(enum tegrabl_heap_type heap_type,
			size_t start, size_t size)
{
	tegrabl_heap_free_block_t *free_list;

	if (size < MIN_SIZE)
		return TEGRABL_ERROR(TEGRABL_ERR_TOO_SMALL, 0);

	if (heap_type >= TEGRABL_HEAP_TYPE_MAX)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	/* check if the heap is already initialized */
	if (tegrabl_heap_free_list[heap_type] != NULL)
		return TEGRABL_ERROR(TEGRABL_ERR_ALREADY_EXISTS, 0);

	free_list = (tegrabl_heap_free_block_t *)start;

	free_list->prev = NULL;
	free_list->next = NULL;
	free_list->size = size;
	free_list->magic = FREE_MAGIC;

	tegrabl_heap_free_list[heap_type] = free_list;
	return TEGRABL_NO_ERROR;
}

/**
 * @brief Splits the specified free block to create allocated
 * block. If there is more space then it will create free block
 * of remaining space and add this free block into free pool.
 *
 * @param free_block Free block which is to be split.
 * @param size Size of allocated block to be created.
 *
 * @return pointer to allocated block.
 */
static tegrabl_heap_alloc_block_t *tegrabl_heap_split_block(
		tegrabl_heap_free_block_t *free_block, size_t size)
{
	uint8_t *tmp;
	tegrabl_heap_free_block_t *new_free = NULL;
	size_t remaining_size;

	if (free_block == NULL)
		return NULL;

	remaining_size = free_block->size - size;

	/* If remaining size is less than size required to
	 * store free block information. Then no need to
	 * split.
	 */
	if (remaining_size <= MIN_SIZE) {
		if (free_block == tegrabl_heap_free_list[TEGRABL_HEAP_DEFAULT])
			tegrabl_heap_free_list[TEGRABL_HEAP_DEFAULT] = free_block->next;
		else if (free_block == tegrabl_heap_free_list[TEGRABL_HEAP_DMA])
			tegrabl_heap_free_list[TEGRABL_HEAP_DMA] = free_block->next;
		if (free_block->next)
			free_block->next->prev = free_block->prev;
		if (free_block->prev)
			free_block->prev->next =free_block->next;
		goto done;
	}

	tmp = (uint8_t *) free_block;

	new_free = (tegrabl_heap_free_block_t *)(tmp + size);

	new_free->prev = free_block->prev;
	new_free->next = free_block->next;

	if (new_free->prev)
		new_free->prev->next = new_free;

	if (new_free->next)
		new_free->next->prev = new_free;

	new_free->size = remaining_size;
	new_free->magic = FREE_MAGIC;
	free_block->size = size;

	/* If first free block gets split, then update the
	 * free list start.
	 */
	if (free_block == tegrabl_heap_free_list[TEGRABL_HEAP_DEFAULT]) {
		tegrabl_heap_free_list[TEGRABL_HEAP_DEFAULT] = new_free;
	} else if (free_block == tegrabl_heap_free_list[TEGRABL_HEAP_DMA]) {
		tegrabl_heap_free_list[TEGRABL_HEAP_DMA] = new_free;
	} else {
		/* No Action required */
	}

done:
	return (tegrabl_heap_alloc_block_t *) free_block;
}

static void *tegrabl_generic_malloc(tegrabl_heap_free_block_t *free_list,
									size_t size)
{
	tegrabl_heap_free_block_t *free_block = NULL;
	tegrabl_heap_alloc_block_t *alloc_block = NULL;
	void *found = NULL;
	size_t alloc_size;

	if (size == 0)
		return NULL;

	free_block = free_list;

	size = ROUND_UP(size, sizeof(uintptr_t));

	alloc_size = size + sizeof(tegrabl_heap_alloc_block_t);

	/* Minimum size to allocate is the size required to store
	 * free block information. This will ensure sufficient
	 * space to store free block information when freed later.
	 */
	alloc_size = MAX(alloc_size, MIN_SIZE);

	/* Find the first free block having sufficient space. */
	while (free_block) {
		TEGRABL_ASSERT(free_block->magic == FREE_MAGIC);

		if (free_block->size > alloc_size) {
			alloc_block = tegrabl_heap_split_block(free_block, alloc_size);
			found = (uint8_t *)alloc_block + sizeof(*alloc_block);
			break;
		}

		free_block = free_block->next;
	}

	if (alloc_block) {
		alloc_block->start = alloc_block;
		alloc_block->magic = ALLOC_MAGIC;
	}

	return found;
}

void *tegrabl_malloc(size_t size)
{
	return tegrabl_generic_malloc(tegrabl_heap_free_list[TEGRABL_HEAP_DEFAULT],
								   size);
}

void *tegrabl_alloc(enum tegrabl_heap_type heap_type, size_t size)
{
	if ((heap_type == TEGRABL_HEAP_DMA) &&
		tegrabl_heap_free_list[TEGRABL_HEAP_DMA])
		return tegrabl_generic_malloc(tegrabl_heap_free_list[TEGRABL_HEAP_DMA],
									  size);
	else
		return tegrabl_malloc(size);
}

/**
 * @brief Tries to merge previous and next free blocks with
 * specified block if found contiguous.
 *
 * @param free_block Current free block which is just added/updated in pool.
 *
 * @return New free block generated after merging contiguous blocks.
 */
static tegrabl_heap_free_block_t *tegrabl_heap_merge_blocks(
		tegrabl_heap_free_block_t *free_block)
{
	tegrabl_heap_free_block_t *next;
	tegrabl_heap_free_block_t *prev;
	uintptr_t cur_mem;
	uintptr_t next_mem;
	uintptr_t prev_mem;

	if (free_block == NULL)
		return NULL;

	next = free_block->next;
	prev  = free_block->prev;
	cur_mem = (uintptr_t)free_block;
	next_mem = (uintptr_t)next;
	prev_mem = (uintptr_t)prev;

	/* If next block is contiguous with current freed block, then
	 * merge these two.
	 */
	if (next && ((cur_mem + free_block->size) == next_mem)) {
		free_block->size += next->size;
		free_block->next = next->next;
		if (next->next)
			next->next->prev = free_block;
	}

	/* If previous block is contiguous with current freed block,
	 * then merge these two.
	 */
	if (prev && ((prev_mem + prev->size) == cur_mem)) {
		prev->size += free_block->size;
		prev->next = free_block->next;
		if (free_block->next)
			free_block->next->prev= prev;

		free_block = prev;
	}

	return free_block;
}

static tegrabl_heap_free_block_t*
tegrabl_generic_free(tegrabl_heap_free_block_t *free_block, void *ptr)
{
	uint8_t *tmp;
	tegrabl_heap_free_block_t *prev_block = NULL;
	tegrabl_heap_alloc_block_t *alloc_block = NULL;
	tegrabl_heap_free_block_t *tmp_free = NULL;

	if (ptr == NULL)
		return NULL;

	tmp = (uint8_t *) ptr;

	alloc_block = (tegrabl_heap_alloc_block_t *)(tmp - sizeof(*alloc_block));

	if (alloc_block->magic != ALLOC_MAGIC) {
		pr_error("heap corrupted !!!\n");
		while (true) {
		}
	}

	tmp_free = (tegrabl_heap_free_block_t *) alloc_block->start;

	/* Find the entry in free list which is just before the freed pointer */
	while (free_block && ((uintptr_t)ptr > (uintptr_t)free_block)) {
		prev_block = free_block;
		free_block = free_block->next;
	}

	if (prev_block)
		prev_block->next = tmp_free;

	tmp_free->prev = prev_block;
	tmp_free->next = free_block;
	tmp_free->magic = FREE_MAGIC;

	if (free_block)
		free_block->prev = tmp_free;

	/* Check if there are contiguous free memory locations. If found
	 * then merge them into single free block.
	 */
	while(1) {
		free_block = tegrabl_heap_merge_blocks(tmp_free);
		if (free_block == tmp_free)
			break;
		tmp_free = free_block;
	}

	return tmp_free;
}

void tegrabl_dealloc(enum tegrabl_heap_type heap_type, void *ptr)
{
	tegrabl_heap_free_block_t *tmp_free = NULL;

	if ((heap_type == TEGRABL_HEAP_DMA) &&
		tegrabl_heap_free_list[TEGRABL_HEAP_DMA]) {
		tmp_free = tegrabl_generic_free(
				tegrabl_heap_free_list[TEGRABL_HEAP_DMA], ptr);
	} else {
		heap_type = TEGRABL_HEAP_DEFAULT;
		tmp_free = tegrabl_generic_free(
			tegrabl_heap_free_list[TEGRABL_HEAP_DEFAULT], ptr);
	}

	if (!tmp_free)
		return;

	/* If free list does not have any blocks or if freed block points to memory
	 * address less than memory pointed by head then update the head of free
	 * block list.
	 */
	if (!tegrabl_heap_free_list[heap_type] ||
		((uintptr_t) tegrabl_heap_free_list[heap_type] > (uintptr_t) tmp_free))
		tegrabl_heap_free_list[heap_type] = tmp_free;
}

void tegrabl_free(void *ptr)
{
	tegrabl_dealloc(TEGRABL_HEAP_DEFAULT, ptr);
}

void *tegrabl_calloc(size_t nmemb, size_t size)
{
	void *mem;

	mem = tegrabl_malloc(size * nmemb);
	if (mem)
		memset(mem, 0x0, size * nmemb);
	return mem;
}

static void *tegrabl_memalign_generic(
		 enum tegrabl_heap_type heap_type, size_t alignment, size_t size)
{
	void *found = NULL;
	tegrabl_heap_alloc_block_t *alloc_block = NULL;
	size_t alloc_size;
	tegrabl_heap_free_block_t *free_block =
		tegrabl_heap_free_list[heap_type];

	if (size == 0)
		return NULL;

	size = ROUND_UP(size, sizeof(uintptr_t));
	alloc_size = size + sizeof(tegrabl_heap_alloc_block_t);
	/* Minimum size to allocate is the size required to store
	 * free block information. This will ensure sufficient
	 * space to store free block information when freed later.
	 */
	alloc_size = MAX(alloc_size, MIN_SIZE);

	/* Find the first free block having sufficient space. */
	while (free_block) {
		size_t align_size;
		size_t orig_size;
		uintptr_t address;
		uint8_t *ptr;
		size_t block_size = free_block->size;

		TEGRABL_ASSERT(free_block->magic == FREE_MAGIC);

		if (block_size < alloc_size) {
			free_block = free_block->next;
			continue;
		}

		address = (uintptr_t) free_block + sizeof(tegrabl_heap_alloc_block_t);

		align_size = alignment - (address % alignment);

		if ((align_size + alloc_size) > block_size) {
			free_block = free_block->next;
			continue;
		}

		tegrabl_heap_free_block_t *prev_block = free_block->prev;
		tegrabl_heap_free_block_t *next_block = free_block->next;

		alloc_block = tegrabl_heap_split_block(free_block,
						alloc_size + align_size);

		if (alloc_block)
			alloc_block->start = alloc_block;
		else
			break;

		ptr = (uint8_t *)alloc_block;
		orig_size = alloc_block->size;

		found = ptr + sizeof(*alloc_block) + align_size;

		alloc_block = (tegrabl_heap_alloc_block_t *) (ptr + align_size);
		alloc_block->size = orig_size;
		alloc_block->magic = ALLOC_MAGIC;
		alloc_block->start = ptr;

		/* If size of alignment is more than the information required to
		 * store free block then free memory and allocate only memory
		 * after alignment.
		 */
		if (align_size < MIN_SIZE)
			break;

		/* This new free block will always be in between prev and next
		 * block of block which just split. This split could have added
		 * a free block in between prev and next. Update the prev and next
		 * appropriately.
		 */
		if (!prev_block && !next_block)
			next_block = tegrabl_heap_free_list[heap_type];
		else if (!next_block)
			next_block = prev_block->next;
		else if (next_block->prev != prev_block)
			next_block = next_block->prev;
		else {
			/* No Action Required */
		}

		alloc_block->size = alloc_size;
		alloc_block->start = ptr + align_size;

		free_block->next = next_block;
		free_block->prev = prev_block;
		free_block->size = align_size;
		free_block->magic = FREE_MAGIC;

		if (next_block)
			next_block->prev = free_block;
		if (prev_block)
			prev_block->next = free_block;

		/* If free list does not have any blocks or if freed block points to
		 * memory address less than memory pointed by head then update the head
		 * of free block list.
		 */
		if (!tegrabl_heap_free_list[heap_type] ||
			((uintptr_t) tegrabl_heap_free_list[heap_type] >
			 (uintptr_t) free_block))
			tegrabl_heap_free_list[heap_type] = free_block;

		break;
	}

	return found;
}

void *tegrabl_alloc_align(enum tegrabl_heap_type heap_type,
		size_t alignment, size_t size)
{
	if ((heap_type == TEGRABL_HEAP_DMA) &&
		tegrabl_heap_free_list[TEGRABL_HEAP_DMA]) {
		return tegrabl_memalign_generic(TEGRABL_HEAP_DMA, alignment, size);
	} else {
		return tegrabl_memalign_generic(TEGRABL_HEAP_DEFAULT, alignment, size);
	}
}

void *tegrabl_memalign(size_t alignment, size_t size)
{
	return tegrabl_memalign_generic(TEGRABL_HEAP_DEFAULT, alignment, size);
}

