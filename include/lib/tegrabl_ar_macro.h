/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_AR_MACRO_H
#define INCLUDED_TEGRABL_AR_MACRO_H

/* Define Constant Address */
#define _MK_ADDR_CONST(_constant_) (_constant_)

/* Define Constant Field */
#define _MK_FIELD_CONST(_mask_, _shift_) (_MK_MASK_CONST(_mask_) << _shift_)

#if !defined(_ASSEMBLY_)
/* Define Constant Mask */
#define _MK_MASK_CONST(_constant_) (_constant_ ## UL)

/* Define Constant Shift */
#define _MK_SHIFT_CONST(_constant_) (_constant_ ## UL)

#else

/* Define Constant Mask */
#define _MK_MASK_CONST(_constant_) (_constant_)

/* Define Constant Shift */
#define _MK_SHIFT_CONST(_constant_) (_constant_)

#endif /* !_ASSEMBLY_ */
#endif /* INCLUDED_TEGRABL_AR_MACRO_H */
