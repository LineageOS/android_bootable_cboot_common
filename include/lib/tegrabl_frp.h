/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_FRP_H
#define TEGRABL_FRP_H

#include <tegrabl_error.h>

/**
 * @brief Check if FRP(Factory Reset Protection) is enabled
 *
 * @param frp_part_name FRP partition name
 * @param enabled FRP state, true: enabled, false: disabled
 *
 * @return TEGRABL_NO_ERROR if no error, or tegrabl_error_t code
 */
tegrabl_error_t tegrabl_is_frp_enabled(char *frp_part_name, bool *enabled);

#endif /* TEGRABL_FRP_H */
