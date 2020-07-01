/*
 * Copyright (c) 2016, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_NVDISP_WIN_LOCAL_H
#define TEGRABL_NVDISP_WIN_LOCAL_H

#include <stdint.h>
#include <tegrabl_nvdisp.h>

/**
* @brief Gives the total windows count.
*
* @param count Address of the location to which win count has be
*              stored.
*/
void nvdisp_win_list(uint32_t *count);

/**
* @brief To Select the given window for programming
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
*/
void nvdisp_win_select(struct tegrabl_nvdisp *nvdisp, uint32_t win_id);

/**
* @brief Returns the handle of the given window
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
*
* @return handle of the given window
*/
struct nvdisp_win *nvdisp_win_get(struct tegrabl_nvdisp *nvdisp,
								  uint32_t win_id);

/**
* @brief Sets the given buffer as framebuffer to the given window.
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
* @param buf address of the framebuffer
*/
void nvdisp_win_set_buf(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
						uintptr_t buf);

/**
* @brief Sets the given rotation angle to the given window.
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
* @param angle rotation angle
*/
void nvdisp_win_set_rotation(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
							 uint32_t angle);

/**
* @brief Configure the given window and enables it
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
*/
void nvdisp_win_config(struct tegrabl_nvdisp *nvdisp, uint32_t win_id);

/**
* @brief Initializes the defaults csc params
*
* @param csc Pointer to the csc params structure
*/
void nvdisp_win_csc_init_defaults(struct nvdisp_csc *csc);

/**
* @brief Sets the given csc parameters to given window.
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
* @param csc Pointer to the csc params structure
*/
void nvdisp_win_csc_set(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
						struct nvdisp_csc *csc);

/**
* @brief Initializes teh default values to cp structure.
*
* @param cp Pionter to the cp structure
*/
void nvdisp_win_cp_init_defaults(struct nvdisp_cp *cp);

/**
* @brief Sets the color palette of a given window
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
* @param cp Pointer to the cp structure
*/
void nvdisp_win_cp_set(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
					   struct nvdisp_cp *cp);

/**
* @brief Set the owner of a given window to a nvdisp instance
*
* @param nvdisp Handle of the nvdisp structure.
* @param win_id Id of the window
*/
void nvdisp_win_set_owner(struct tegrabl_nvdisp *nvdisp, uint32_t win_id);
#endif
