/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
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
/* macro tegrabl dma data direction */
#define TEGRABL_DMA_TO_DEVICE 0x1U /**< DMA from memory to device */
#define TEGRABL_DMA_FROM_DEVICE 0x2U /**< DMA from device to memory */
#define TEGRABL_DMA_BIDIRECTIONAL 0x3U /**< DMA from/to device to memory */
typedef uint32_t tegrabl_dma_data_direction;

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

/**
 * @brief This maps BL component specific VA2PA for driver's use
 *
 * @params module Module owning the DMA
 * @params va Virtual address of dma buffer
 *
 * @return Physical address
 */
dma_addr_t tegrabl_dma_va_to_pa(tegrabl_module_t module, void *va);

#endif /* INCLUDED_TEGRABL_DMAMAP_H */
