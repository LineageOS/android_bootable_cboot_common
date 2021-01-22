/*
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @brief Module used for error logging.
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
#define FREE_MAGIC 0xDEEFBEEEUL

/**
 * @brief Magic number for allocated memory blocks.
 */
#define ALLOC_MAGIC 0xDEADBEEEUL

/**
 * @brief Minimum size of allocation. This size should be more than
 * size required to store information about free block.
 */
#define MIN_SIZE sizeof(tegrabl_heap_free_block_t)

/**
 * @brief Information describing a free/unallocated block of memory.
 */
typedef struct tegrabl_heap_free_block {
	/** Magic identifier for free block */
	uint32_t magic;
	/** Size of the free block, this includes the header */
	size_t size;
	/** Pointer to previous free block */
	struct tegrabl_heap_free_block *prev;
	/** Pointer to next free block */
	struct tegrabl_heap_free_block *next;
} tegrabl_heap_free_block_t;

/**
 * @brief Information describing the allocated memory. This is used while
 * freeing memory.
 */
typedef struct tegrabl_heap_alloc_block {
	/** Magic identifier for allocated block */
	uint32_t magic;
	/** Total size of allocated block */
	size_t size;
	/** Start of the allocated block */
	void *start;
	/* Note that the start of allocated block and what is returned to caller
	 * of malloc APIs is different */
} tegrabl_heap_alloc_block_t;

/**
 * @brief Head of doubly linked list of free memory block.
 * List is sorted in ascending order of memory address.
 */
static tegrabl_heap_free_block_t *tegrabl_heap_free_list[TEGRABL_HEAP_TYPE_MAX];

/**
 * @brief Maximum size of heap. Same as size of heap at the time of initialization.
 */
static size_t max_heap_size[TEGRABL_HEAP_TYPE_MAX];

tegrabl_error_t tegrabl_heap_init(tegrabl_heap_type_t heap_type, size_t start, size_t size)
{
	tegrabl_heap_free_block_t *free_list;

	if (size < MIN_SIZE) {
		return TEGRABL_ERROR(TEGRABL_ERR_TOO_SMALL, 0);
	}

	if (heap_type >= TEGRABL_HEAP_TYPE_MAX) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* check if the heap is already initialized */
	if (tegrabl_heap_free_list[heap_type] != NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_ALREADY_EXISTS, 0);
	}

	free_list = (tegrabl_heap_free_block_t *)start;

	free_list->prev = NULL;
	free_list->next = NULL;
	free_list->size = size;
	free_list->magic = FREE_MAGIC;

	tegrabl_heap_free_list[heap_type] = free_list;
	max_heap_size[heap_type] = size;

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Splits the specified free block to create allocated
 * block. If there is more space then it will create free block
 * of remaining space and add this free block into free pool.
 *
 * @param[in] heap_type Type of heap. See @ref HEAP_TYPES for possible values.
 * @param[in] free_block Free block which is to be split.
 * @param[in] size Size of allocated block to be created.
 *
 * @return Pointer to allocated block.
 */
static tegrabl_heap_alloc_block_t *tegrabl_heap_split_block(tegrabl_heap_type_t heap_type,
	tegrabl_heap_free_block_t *free_block,
	size_t size)
{
	uint8_t *tmp;
	tegrabl_heap_free_block_t *new_free = NULL;
	size_t remaining_size;
	tegrabl_heap_free_block_t *prev_block;
	tegrabl_heap_free_block_t *next_block;

	TEGRABL_ASSERT(free_block != NULL);

	remaining_size = free_block->size - size;

	prev_block = free_block->prev;
	next_block = free_block->next;

	/* If remaining size is less than size required to
	 * store free block information. Then no need to
	 * split.
	 */
	if (remaining_size <= MIN_SIZE) {
		if (free_block == tegrabl_heap_free_list[heap_type]) {
			tegrabl_heap_free_list[heap_type] = free_block->next;
		}
		if (next_block != NULL) {
			next_block->prev = free_block->prev;
		}
		if (prev_block != NULL) {
			prev_block->next = free_block->next;
		}
		goto done;
	}

	tmp = (uint8_t *)free_block;

	new_free = (tegrabl_heap_free_block_t *)(tmp + size);

	new_free->prev = prev_block;
	new_free->next = next_block;

	if (prev_block != NULL) {
		prev_block->next = new_free;
	}

	if (next_block != NULL) {
		next_block->prev = new_free;
	}

	new_free->size = remaining_size;
	new_free->magic = FREE_MAGIC;

	free_block->size = size;

	/* If first free block gets split, then update the
	 * free list start.
	 */
	if (free_block == tegrabl_heap_free_list[heap_type]) {
		tegrabl_heap_free_list[heap_type] = new_free;
	} else {
		/* No Action required */
	}

done:
	return (tegrabl_heap_alloc_block_t *)free_block;
}

/**
 * @brief Generic allocate memory and return pointer to the allocated memory
 *
 * @param[in] heap_type Type of heap. See @ref HEAP_TYPES for possible values.
 * @param[in] size Size of allocated block to be created
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
static void *tegrabl_generic_malloc(tegrabl_heap_type_t heap_type, size_t size)
{
	tegrabl_heap_free_block_t *free_block = NULL;
	tegrabl_heap_alloc_block_t *alloc_block = NULL;
	void *found = NULL;
	size_t alloc_size;

	if ((size == 0UL) || (size > max_heap_size[heap_type])) {
		return NULL;
	}

	free_block = tegrabl_heap_free_list[heap_type];

	alloc_size = ROUND_UP(size, sizeof(uintptr_t));

	alloc_size = alloc_size + sizeof(tegrabl_heap_alloc_block_t);

	/* Ensure addition didn't wrap the value. */
	if (alloc_size < size) {
		return NULL;
	}

	/* Minimum size to allocate is the size required to store
	 * free block information. This will ensure sufficient
	 * space to store free block information when freed later.
	 */
	alloc_size = MAX(alloc_size, MIN_SIZE);

	/* Find the first free block having sufficient space. */
	while (free_block != NULL) {
		TEGRABL_ASSERT(free_block->magic == FREE_MAGIC);

		if (free_block->size > alloc_size) {
			if (free_block->magic != FREE_MAGIC) {
				pr_error("Heap free list corrupted !!!\n");
				while (true) {
				}
			}

			alloc_block = tegrabl_heap_split_block(heap_type, free_block, alloc_size);
			found = (uint8_t *)alloc_block + sizeof(*alloc_block);
			break;
		}

		free_block = free_block->next;
	}

	if (alloc_block != NULL) {
		alloc_block->start = alloc_block;
		alloc_block->magic = ALLOC_MAGIC;
	}

	return found;
}

void *tegrabl_malloc(size_t size)
{
	return tegrabl_generic_malloc(TEGRABL_HEAP_DEFAULT, size);
}

void *tegrabl_alloc(tegrabl_heap_type_t heap_type, size_t size)
{
	tegrabl_heap_type_t type = heap_type;

	if (heap_type >= TEGRABL_HEAP_TYPE_MAX) {
		return NULL;
	}

	if (tegrabl_heap_free_list[TEGRABL_HEAP_DMA] == NULL) {
		type = TEGRABL_HEAP_DEFAULT;
	}

	return tegrabl_generic_malloc(type, size);
}

/**
 * @brief Tries to merge previous and next free blocks with
 * specified block if found contiguous.
 *
 * @param[in] free_block Current free block which is just added/updated in pool.
 *
 * @return New free block generated after merging contiguous blocks.
 */
static tegrabl_heap_free_block_t *tegrabl_heap_merge_blocks(
		tegrabl_heap_free_block_t *free_block)
{
	const tegrabl_heap_free_block_t *next;
	tegrabl_heap_free_block_t *prev;
	uintptr_t cur_mem;
	uintptr_t next_mem;
	uintptr_t prev_mem;

	if (free_block == NULL) {
		return NULL;
	}

	next = free_block->next;
	prev  = free_block->prev;
	cur_mem = (uintptr_t)free_block;
	next_mem = (uintptr_t)next;
	prev_mem = (uintptr_t)prev;

	/* If next block is contiguous with current freed block, then
	 * merge these two.
	 */
	if ((next != NULL) && ((cur_mem + free_block->size) == next_mem)) {
		free_block->size += next->size;
		free_block->next = next->next;
		if (next->next != NULL) {
			next->next->prev = free_block;
		}
	}

	/* If previous block is contiguous with current freed block,
	 * then merge these two.
	 */
	if ((prev != NULL) && ((prev_mem + prev->size) == cur_mem)) {
		prev->size += free_block->size;
		prev->next = free_block->next;
		if (free_block->next != NULL) {
			free_block->next->prev= prev;
		}
		free_block = prev;
	}

	return free_block;
}

/**
 * @brief Generic free memory
 *
 * @param[in] heap_type Specifies the heap from where the memory has to be freed.
 *            See @ref HEAP_TYPES for possible values.
 * @param[in] ptr Specifies start address of the memory
 *
 * @return Pointer to list of type tegrabl_heap_free_block_t if successful else NULL
 */
static tegrabl_heap_free_block_t*
tegrabl_generic_free(tegrabl_heap_type_t heap_type, const void *ptr)
{
	const uint8_t *tmp;
	tegrabl_heap_free_block_t *prev_block = NULL;
	const tegrabl_heap_alloc_block_t *alloc_block = NULL;
	tegrabl_heap_free_block_t *tmp_free = NULL;
	tegrabl_heap_free_block_t *free_block = NULL;

	if (ptr == NULL) {
		return NULL;
	}

	tmp = (const uint8_t *)ptr;
	alloc_block = (const tegrabl_heap_alloc_block_t *)(tmp - sizeof(*alloc_block));

	if (alloc_block->magic != ALLOC_MAGIC) {
		pr_error("Heap corrupted !!!\n");
		while (true) {
		}
	}

	tmp_free = (tegrabl_heap_free_block_t *) alloc_block->start;
	tmp_free->size = alloc_block->size;

	free_block = tegrabl_heap_free_list[heap_type];
	/* Find the entry in free list which is just before the freed pointer */
	while ((free_block != NULL) && ((uintptr_t)(const uint8_t *)ptr > (uintptr_t)(const uint8_t *)free_block)) {
		prev_block = free_block;
		free_block = free_block->next;
	}

	if (prev_block != NULL) {
		prev_block->next = tmp_free;
	}

	tmp_free->prev = prev_block;
	tmp_free->next = free_block;
	tmp_free->magic = FREE_MAGIC;

	if (free_block != NULL) {
		free_block->prev = tmp_free;
	}

	/* Check if there are contiguous free memory locations. If found
	 * then merge them into single free block.
	 */
	while (true) {
		free_block = tegrabl_heap_merge_blocks(tmp_free);
		if (free_block == tmp_free) {
			break;
		}
		tmp_free = free_block;
	}

	return tmp_free;
}

/**
 * @brief Update free list head
 *
 * @param[in] heap_type Type of heap. See @ref HEAP_TYPES for possible values.
 * @param[in] free_block Pointer to free block of type tegrabl_heap_free_block_t
 */
static void update_free_list_head(tegrabl_heap_type_t heap_type, tegrabl_heap_free_block_t *free_block)
{
	/* If free list does not have any blocks or if freed block points to
	 * memory address less than memory pointed by head then update the head
	 * of free block list.
	 */
	if ((tegrabl_heap_free_list[heap_type] == NULL) || (tegrabl_heap_free_list[heap_type] > free_block)) {
		tegrabl_heap_free_list[heap_type] = free_block;
	}
}

void tegrabl_dealloc(tegrabl_heap_type_t heap_type, const void *ptr)
{
	tegrabl_heap_type_t type = heap_type;

	tegrabl_heap_free_block_t *tmp_free = NULL;

	if (heap_type >= TEGRABL_HEAP_TYPE_MAX) {
		return;
	}

	if (tegrabl_heap_free_list[TEGRABL_HEAP_DMA] == NULL) {
		type = TEGRABL_HEAP_DEFAULT;
	}

	tmp_free = tegrabl_generic_free(type, ptr);

	if (tmp_free == NULL) {
		return;
	}

	update_free_list_head(type, tmp_free);
}

void tegrabl_free(const void *ptr)
{
	tegrabl_dealloc(TEGRABL_HEAP_DEFAULT, ptr);
}

void *tegrabl_calloc(size_t nmemb, size_t size)
{
	void *mem;
	size_t total_size = size * nmemb;

	if (size == 0UL) {
		return NULL;
	}

	/* Ensure multiplication didn't wrap the value. */
	if (nmemb > (max_heap_size[TEGRABL_HEAP_DEFAULT] / size)) {
		return NULL;
	}

	TEGRABL_ASSERT((total_size / size) == nmemb);

	mem = tegrabl_malloc(total_size);
	if (mem != NULL) {
		(void)memset(mem, 0x0, total_size);
	}
	return mem;
}

/**
 * @brief Get free block
 *
 * @param[in] heap_type Type of heap. See @ref HEAP_TYPES for possible values.
 * @param[in] alignment Specifies alignment
 * @param[in] alloc_size Specifies size in bytes
 *
 * @return Pointer to free block of type tegrabl_heap_free_block_t
 */
static tegrabl_heap_free_block_t *get_free_block(tegrabl_heap_type_t heap_type,
	size_t alignment, size_t alloc_size)
{
	size_t align_size;
	uintptr_t address;
	size_t block_size;

	tegrabl_heap_free_block_t *free_block = tegrabl_heap_free_list[heap_type];

	/* Find the first free block having sufficient space. */
	while (free_block != NULL) {
		block_size = free_block->size;

		TEGRABL_ASSERT(free_block->magic == FREE_MAGIC);

		if (block_size < alloc_size) {
			free_block = free_block->next;
			continue;
		}

		/* Need space to store metadata */
		address = ((uintptr_t)(uint8_t *)free_block) + sizeof(tegrabl_heap_alloc_block_t);
		align_size = alignment - (address % alignment);

		if ((align_size + alloc_size) > block_size) {
			free_block = free_block->next;
			continue;
		}

		if (free_block->magic != FREE_MAGIC) {
			pr_error("Heap free list corrupted !!!\n");
			while (true) {
			}
		}
		break;
	}

	return free_block;
}

/**
 * @brief Generic allocate memory and alignment
 *
 * @param[in] heap_type Type of heap. See @ref HEAP_TYPES for possible values.
 * @param[in] alignment Specifies the alignment
 * @param[in] size Specifies the size in bytes
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
static void *tegrabl_memalign_generic(tegrabl_heap_type_t heap_type, size_t alignment, size_t size)
{
	void *found = NULL;
	tegrabl_heap_alloc_block_t *alloc_block = NULL;
	size_t alloc_size;
	tegrabl_heap_free_block_t *prev_block;
	tegrabl_heap_free_block_t *next_block;
	tegrabl_heap_free_block_t *free_block;
	size_t align_size;
	size_t orig_size;
	uintptr_t address;
	uint8_t *ptr;

	if (size == 0UL) {
		return NULL;
	}

	size = ROUND_UP(size, sizeof(uintptr_t));
	alloc_size = size + sizeof(tegrabl_heap_alloc_block_t);
	/* Minimum size to allocate is the size required to store
	 * free block information. This will ensure sufficient
	 * space to store free block information when freed later.
	 */
	alloc_size = MAX(alloc_size, MIN_SIZE);

	free_block = get_free_block(heap_type, alignment, alloc_size);

	if (free_block == NULL) {
		return NULL;
	}

	prev_block = free_block->prev;
	next_block = free_block->next;

	address = ((uintptr_t)(uint8_t *)free_block) + sizeof(tegrabl_heap_alloc_block_t);
	align_size = alignment - (address % alignment);

	alloc_block = tegrabl_heap_split_block(heap_type, free_block, alloc_size + align_size);

	TEGRABL_ASSERT((void *)alloc_block == (void *)free_block);

	ptr = (uint8_t *)alloc_block;
	orig_size = alloc_block->size;

	found = ptr + sizeof(*alloc_block) + align_size;

	/* Metadata is always before returned pointer. */
	alloc_block = (tegrabl_heap_alloc_block_t *) (ptr + align_size);
	alloc_block->size = orig_size;
	alloc_block->magic = ALLOC_MAGIC;
	alloc_block->start = ptr;

	/* If size of alignment is more than the information required to
	 * store free block then free memory and allocate only memory
	 * after alignment.
	 */
	if (align_size < MIN_SIZE) {
		goto done;
	}

	/* This new free block will always be in between prev and next
	 * block of block which just split. This split could have added
	 * a free block in between prev and next. Update the prev and next
	 * appropriately.
	 */
	if ((prev_block == NULL) && (next_block == NULL)) {
		/* There was only one free block. The new block is be before current block */
		next_block = tegrabl_heap_free_list[heap_type];
	} else if (next_block == NULL) {
		next_block = prev_block->next;
	} else if (next_block->prev != prev_block) {
		next_block = next_block->prev;
	} else {
		;/* No Action Required */
	}

	alloc_block->size = alloc_size;
	alloc_block->start = ptr + align_size;

	free_block->next = next_block;
	free_block->prev = prev_block;
	free_block->size = align_size;
	free_block->magic = FREE_MAGIC;

	if (next_block != NULL) {
		TEGRABL_ASSERT(next_block->prev == prev_block);
		next_block->prev = free_block;
	}
	if (prev_block != NULL) {
		TEGRABL_ASSERT(prev_block->next == next_block);
		prev_block->next = free_block;
	}

	update_free_list_head(heap_type, free_block);

done:
	TEGRABL_ASSERT((((uintptr_t)(uint8_t *)found) % alignment) == 0UL);
	return found;
}

/**
 * @brief Boundary and overflow checks for alignment and size
 *
 * @param[in] heap_type Type of heap. See @ref HEAP_TYPES for possible values.
 * @param[in] alignment Requested alignment
 * @param[in] size Requested size
 *
 * @retval true All checks pass
 * @retval false One of the following conditions is true:
 *	- size or alignment > Maximum size of heap type
 *	- size + alignment > Maximum size of heap type
 *	- size + alignment < size or alignment
 */
static bool is_size_alignment_valid(tegrabl_heap_type_t heap_type, size_t alignment, size_t size)
{
	size_t max_size = size + alignment;

	if (size > max_heap_size[heap_type]) {
		return false;
	}

	if (alignment > max_heap_size[heap_type]) {
		return false;
	}

	if (max_size > max_heap_size[heap_type]) {
		return false;
	}

	/* Ensure addition didn't wrap the value. */
	if (max_size < size) {
		return false;
	}

	if (max_size < alignment) {
		return false;
	}

	return true;
}

void *tegrabl_alloc_align(tegrabl_heap_type_t heap_type, size_t alignment, size_t size)
{
	tegrabl_heap_type_t type = heap_type;

	if (heap_type >= TEGRABL_HEAP_TYPE_MAX) {
		return NULL;
	}

	if (tegrabl_heap_free_list[TEGRABL_HEAP_DMA] == NULL) {
		type = TEGRABL_HEAP_DEFAULT;
	}

	if (!is_size_alignment_valid(type, alignment, size)) {
		return NULL;
	}

	return tegrabl_memalign_generic(type, alignment, size);
}

void *tegrabl_memalign(size_t alignment, size_t size)
{
	if (!is_size_alignment_valid(TEGRABL_HEAP_DEFAULT, alignment, size)) {
		return NULL;
	}

	return tegrabl_memalign_generic(TEGRABL_HEAP_DEFAULT, alignment, size);
}

