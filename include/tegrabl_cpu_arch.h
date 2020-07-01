/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_CPU_ARCH_H
#define INCLUDED_TEGRABL_CPU_ARCH_H

#if defined(__aarch64__)
#include <tegrabl_armv8a.h>
#elif defined(__arm__) && defined(__ARM_ARCH_7R__)
#include <tegrabl_armv7r.h>
#endif

#endif /* INCLUDED_TEGRABL_CPU_ARCH_H */
