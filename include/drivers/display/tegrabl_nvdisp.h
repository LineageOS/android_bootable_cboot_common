/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_NVDISP_H
#define TEGRABL_NVDISP_H

#include <tegrabl_error.h>
#include <tegrabl_surface.h>
#include <tegrabl_nvdisp_local.h>
#include <tegrabl_display_dtb.h>

/* nvdisp flags */
#define NVDISP_FLAG_ENABLED     (1 << 0)
#define NVDISP_FLAG_CMU_DISABLE (0 << 1)
#define NVDISP_FLAG_CMU_ENABLE  (1 << 1)

struct tegrabl_nvdisp {
	int32_t instance;
	int32_t sor_instance;
	uintptr_t base_addr;
	uint32_t module_nvdisp;
	uint32_t module_host1x;
	uint32_t module_nvdisp_hub;
	uint32_t module_nvdisp_dsc;
	uint32_t module_nvdisp_p0;
	uint64_t flags;
	uint32_t color_format;
	bool cmu_enable;
	uint32_t type;
	uint32_t align;
	uint32_t depth;
	uint32_t dither;
	struct nvdisp_mode *mode;
	uint32_t n_modes;
	uint32_t height; /* mm */
	uint32_t width; /* mm */
	uintptr_t cmu_base_addr;
	struct nvdisp_cmu *cmu_adobe_rgb;
	struct nvdisp_out_pin *out_pins;
	uint32_t clk_rate;
	struct nvdisp_out_ops *out_ops;
	int32_t out_type;
	void *out_data;
	struct nvdisp_win windows[N_WINDOWS];
	uint32_t n_windows;
	bool enabled;
};

/** @brief Configures the given window in the nvdisp controller
 *
 *  @param out_type Type of the display unit.
 *  @param pdata Address of display dtb structure

 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
struct tegrabl_nvdisp *tegrabl_nvdisp_init(int32_t out_type,
										   struct tegrabl_display_pdata *pdata);

/** @brief Lists the address of the available windows in the given window
 *         pointer
 *
 *  @param nvdisp Handle of the nvdisp structure.
 *  @param count Address of the variable, in which number of windows to be
 *         passed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
void tegrabl_nvdisp_list_windows(struct tegrabl_nvdisp *nvdisp,
								 uint32_t *count);

/** @brief Configures the given window in the nvdisp controller
 *
 *  @param nvdisp Handle of the nvdisp structure.
 *  @param win_id ID of the window to configure.
 *  @param surf Address of the surface.

 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_nvdisp_configure_window(struct tegrabl_nvdisp *nvdisp,
	uint32_t win_id, struct tegrabl_surface *surf);

/** @brief Sets the window start address with the given surface address
 *
 *  @param nvdisp Handle of the nvdisp structure.
 *  @param win_id ID of the window to configure.
 *  @param surf_buf Address of the surface buffer.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
void tegrabl_nvdisp_win_set_surface(struct tegrabl_nvdisp *nvdisp,
	uint32_t win_id, uintptr_t surf_buf);

/** @brief Get the resolution of the display
 *
 *  @param nvdisp Handle of the nvdisp structure.
 *  @param height Height section of the resolution.
 *  @param width width section of the resolution.
 */
void tegrabl_nvdisp_get_resolution(struct tegrabl_nvdisp *nvdisp,
								   uint32_t *height, uint32_t *width);

/*
 * Display function pointers for HDMI and DSI
 */
struct nvdisp_out_ops {
	tegrabl_error_t (*init)(struct tegrabl_nvdisp *nvdisp,
							struct tegrabl_display_pdata *pdata);
	tegrabl_error_t (*shutdown)(struct tegrabl_nvdisp *nvdisp);
	tegrabl_error_t (*enable)(struct tegrabl_nvdisp *nvdisp);
	tegrabl_error_t (*disable)(struct tegrabl_nvdisp *nvdisp);
	uint64_t (*setup_clk)(struct tegrabl_nvdisp *nvdisp, uint32_t clk_id);
};

#endif
