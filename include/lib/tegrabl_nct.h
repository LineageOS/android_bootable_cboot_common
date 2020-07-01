/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_NCT_H
#define INCLUDED_TEGRABL_NCT_H

#include <stdint.h>
#include <nct/nct.h>
#include <tegrabl_error.h>

/**
 * @brief Load in nct partition and sanity check the NCT header
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_nct_init(void);

/**
 * @brief Read nct item based on item id
 *
 * @param id NCT item id
 * @param buf Buffer to store item
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_nct_read_item(enum nct_id id, union nct_item *buf);

/**
 * @brief Get readable spec id/config from NCT
 *
 * @param id Buffer to store spec/id
 * @param config Buffer to store spec/config
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_nct_get_spec(char *id, char *config);

#endif /* INCLUDED_TEGRABL_NCT_H */
