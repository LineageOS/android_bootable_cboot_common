/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
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
 * @brief specifies the type of heap
 */
/* macro tegrabl heap type */
typedef uint32_t tegrabl_heap_type_t;
#define TEGRABL_HEAP_DEFAULT 0U
#define TEGRABL_HEAP_DMA 1U
#define TEGRABL_HEAP_TYPE_MAX 2U

/**
 * @brief Reserves a large pool of memory. Using tegrabl_malloc, tegrabl_calloc
 * or tegrabl_memalign small part of this memory can be requested on need basis
 * for use. This memory can be returned back to pool using tegrabl_free.
 *
 * @param heap_type Constant indicating type of heap
 * @param start Start location of heap
 * @param size  Size of the heap
 *
 * @return TEGRABL_NO_ERROR if successful, else error.
 */
tegrabl_error_t tegrabl_heap_init(tegrabl_heap_type_t heap_type,
								  size_t start, size_t size);

/**
 * @brief Allocates memory and returns pointer to the allocated memory.
 *
 * @param size Specifies the size in bytes
 *
 * @return pointer to the allocated memory if successful else NULL
 */
void *tegrabl_malloc(size_t size);

/**
 * @brief Allocates memory and returns pointer to the allocated memory.
 * The memory address will be a multiple of alignment.
 *
 * @param alignment  Specifies the alignment
 * @param size       Specifies the size in bytes
 *
 * @return pointer to the allocated memory if successful else NULL
 */
void *tegrabl_memalign(size_t alignment, size_t size);

/**
 * @brief Allocates memory of an array of nmemb elements of size bytes each and
 * returns a pointer to the allocated memory. The memory is set to zero.
 *
 * @param nmemb Specifies no of elelements
 * @param size  Specifies size of an element in bytes
 *
 * @return pointer to the allocated memory if successful else NULL
 */
void *tegrabl_calloc(size_t nmemb, size_t size);

/**
 * @brief Frees the memory space which must have been allocated by
 * tegrabl_malloc or tegrabl_calloc. Calling this function to free memory
 * which has already freed by previous call to this function will lead to
 * undefined behaviour. If ptr is NULL, no operation is performed.
 *
 * @param ptr Specifies start address of the memory
 */
void tegrabl_free(void *ptr);

/**
 * @brief Allocates memory and returns pointer to the allocated memory.
 *
 * @param heap_type Specifies the heap from where the memory has to be allocated
 * @param size Specifies the size in bytes
 *
 * @return pointer to the allocated memory if successful else NULL
 */
void *tegrabl_alloc(tegrabl_heap_type_t heap_type, size_t size);

/**
 * @brief Frees memory
 *
 * @param heap_type Specifies the heap from where the memory has to be freed
 * @param ptr Specifies start address of the memory
 *
 */
void tegrabl_dealloc(tegrabl_heap_type_t heap_type, void *ptr);

/**
 * @brief Allocates aligned memory from specified heap type and returns pointer
 * to allocated memory.
 *
 * @param heap_type Specifies the heap from where the memory has to be allocated
 * @param alignment Specifies alignment
 * @param size Specifies size in bytes
 *
 * @return pointer to the allocated memory if successful else NULL
 */
void *tegrabl_alloc_align(tegrabl_heap_type_t heap_type,
		size_t alignment, size_t size);

/**
 * @brief Changes the size of memory pointed to by ptr.
 *
 * @param ptr Pointer to memory block previously allocated using
 * tegrabl_malloc, tegrabl_calloc, tegrabl_realloc.
 * @param size New size of the memory block in bytes
 *
 * @return New address of memory block if successful else NULL.
 */
void *tegrabl_realloc(void *ptr, size_t size);

#endif /* INCLUDED_TEGRABL_MALLOC_H */

