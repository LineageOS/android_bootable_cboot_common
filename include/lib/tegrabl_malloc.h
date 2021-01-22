/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_MALLOC_H
#define INCLUDED_TEGRABL_MALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>

/**
 * @brief Define heap types available. Possible values are @ref HEAP_TYPES.
 */
typedef uint32_t tegrabl_heap_type_t;

/**
 * @addtogroup HEAP_TYPES Heap types
 * @brief Values for @ref tegrabl_heap_type_t
 * @{
 */
#define TEGRABL_HEAP_DEFAULT 0U
#define TEGRABL_HEAP_DMA 1U
#define TEGRABL_HEAP_TYPE_MAX 2U
/** @}*/

/**
 * @brief Reserve a large pool of memory. Using tegrabl_malloc, tegrabl_calloc
 * or tegrabl_memalign small part of this memory can be requested on need basis
 * for use. This memory can be returned back to pool using tegrabl_free.
 *
 * @param[in] heap_type Constant indicating type of heap.
 *                      See @ref HEAP_TYPES for possible values
 * @param[in] start Start location of heap
 * @param[in] size  Size of the heap
 *
 * @return TEGRABL_NO_ERROR Successful, else error.
 */
tegrabl_error_t tegrabl_heap_init(tegrabl_heap_type_t heap_type,
								  size_t start, size_t size);

/**
 * @brief Allocate memory and returns pointer to the allocated memory from default heap.
 *
 * @param[in] size Specifies the size in bytes
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
void *tegrabl_malloc(size_t size);

/**
 * @brief Allocate memory and returns pointer to the allocated memory from default heap.
 * The memory address will be a multiple of alignment.
 *
 * @param[in] alignment  Specifies the alignment in bytes
 * @param[in] size       Specifies the size in bytes
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
void *tegrabl_memalign(size_t alignment, size_t size);

/**
 * @brief Allocate memory of an array of nmemb elements of size bytes each and
 * returns a pointer to the allocated memory. The memory is set to zero.
 *
 * @param[in] nmemb Specifies no of elements
 * @param[in] size  Specifies size of an element in bytes
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
void *tegrabl_calloc(size_t nmemb, size_t size);

/**
 * @brief Free the memory space which must have been allocated by
 * tegrabl_malloc or tegrabl_calloc. Calling this function to free memory
 * which has already freed by previous call to this function will lead to
 * undefined behaviour. If ptr is NULL, no operation is performed.
 *
 * @param[in] ptr Specifies start address of the memory
 */
void tegrabl_free(const void *ptr);

/**
 * @brief Allocate memory and returns pointer to the allocated memory.
 *
 * @param[in] heap_type Specifies the heap from where the memory has to be allocated
 * @param[in] size Specifies the size in bytes
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
void *tegrabl_alloc(tegrabl_heap_type_t heap_type, size_t size);

/**
 * @brief Free memory
 *
 * @param[in] heap_type Specifies the heap from where the memory has to be freed
 * @param[in] ptr Specifies start address of the memory
 *
 */
void tegrabl_dealloc(tegrabl_heap_type_t heap_type, const void *ptr);

/**
 * @brief Allocate aligned memory from specified heap type and returns pointer
 * to allocated memory.
 *
 * @param[in] heap_type Specifies the heap from where the memory has to be allocated
 * @param[in] alignment Specifies alignment in bytes
 * @param[in] size Specifies size in bytes
 *
 * @return Pointer to the allocated memory if successful else NULL
 */
void *tegrabl_alloc_align(tegrabl_heap_type_t heap_type,
		size_t alignment, size_t size);

/**
 * @brief Change the size of memory pointed to by ptr.
 *
 * @param[in] ptr Pointer to memory block previously allocated using
 * tegrabl_malloc, tegrabl_calloc, tegrabl_realloc.
 * @param[in] size New size of the memory block in bytes
 *
 * @return New address of memory block if successful else NULL.
 */
void *tegrabl_realloc(void *ptr, size_t size);

#endif /* INCLUDED_TEGRABL_MALLOC_H */

