/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * This header is meant to export the C library DMA offload handling interface
 * between assembly files and the C files.
 */
#ifndef INCLUDED_CLIB_DMA_H
#define INCLUDED_CLIB_DMA_H

#include <string.h>

/**
 * @brief DMA callback implementing memcpy offload
 */
extern clib_dma_memcpy_t clib_dma_memcpy_callback;

/**
 * @brief Threshold (in bytes) beyond which DMA engine callback should be used
 * (instead of using load/stores) for memcpy operation
 */
extern size_t clib_dma_memcpy_threshold;

/**
 * @brief DMA driver specific private data that must be passed to
 * clib_dma_memcpy_callback
 */
extern void *clib_dma_memcpy_priv;

/**
 * @brief DMA callback implementing memset offload
 */
extern clib_dma_memset_t clib_dma_memset_callback;

/**
 * @brief Threshold (in bytes) beyond which DMA engine callback should be used
 * (instead of using load/stores) for memset operation
 */
extern size_t clib_dma_memset_threshold;

/**
 * @brief DMA driver specific private data that must be passed to
 * clib_dma_memset_callback
 */
extern void *clib_dma_memset_priv;

#endif /* INCLUDED_CLIB_DMA_H */
