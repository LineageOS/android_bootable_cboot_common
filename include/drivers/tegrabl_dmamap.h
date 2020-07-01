/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_DMAMAP_H
#define INCLUDED_TEGRABL_DMAMAP_H

#include <stdint.h>
#include <stddef.h>
#include <tegrabl_module.h>

/**
 * @brief Type for specifying DMA bus-address
 */
typedef uint64_t dma_addr_t;

/**
 * @brief Type to specify the direction of DMA transfer
 */
typedef enum {
	TEGRABL_DMA_TO_DEVICE = 0x1, /**< DMA from memory to device */
	TEGRABL_DMA_FROM_DEVICE = 0x2, /**< DMA from device to memory */
	TEGRABL_DMA_BIDIRECTIONAL = 0x3, /**< DMA from/to device to memory */
} tegrabl_dma_data_direction;

/**
 * @brief Make DMA the owner of CPU-owned buffers, (i.e. perform any requisite
 * cache maintainence for use with DMA). CPU should not use the buffers after
 * calling this API.
 * Also, the returned address should be used for specifying the buffer address
 * to the DMA.
 *
 * @param module Module owning the DMA
 * @param instance Instance number of the module
 * @param buffer Buffer (VA) being used by the DMA
 * @param size Size of the buffer
 * @param direction Direction of the DMA data-transfer
 *
 * @return DMA bus-address corresponding to the provided buffer
 */
dma_addr_t tegrabl_dma_map_buffer(tegrabl_module_t module, uint8_t instance,
								  void *buffer, size_t size,
								  tegrabl_dma_data_direction direction);

/**
 * @brief Make CPU the owner of DMA-owned buffers, (i.e. perform any requisite
 * cache maintainence for use with CPU). DMA should not use the buffers after
 * calling this API.
 *
 * @param module Module owning the DMA
 * @param instance Instance number of the module
 * @param buffer Buffer (VA) being used by the DMA
 * @param size Size of the buffer
 * @param direction Direction of the DMA data-transfer
 */
void tegrabl_dma_unmap_buffer(tegrabl_module_t module, uint8_t instance,
							  void *buffer, size_t size,
							  tegrabl_dma_data_direction direction);

#endif /* INCLUDED_TEGRABL_DMAMAP_H */
