/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef _TEGRABL_SMMU_EXT_H
#define _TEGRABL_SMMU_EXT_H

#define SMMU_READ	(1 << 0)
#define SMMU_WRITE	(1 << 1)

tegrabl_error_t tegrabl_smmu_enable_prot(void *data, unsigned long iova,
				uint64_t paddr, size_t size, int prot);

size_t tegrabl_smmu_disable_prot(void *data, unsigned long iova, size_t size);

tegrabl_error_t tegrabl_smmu_init(void);

void tegrabl_smmu_deinit(void);

tegrabl_error_t tegrabl_smmu_add_device(const void *fdt, int32_t node_offset,
					void **data);

void tegrabl_smmu_remove_device(void *data);

#endif // _TEGRABL_SMMU_EXT_H
