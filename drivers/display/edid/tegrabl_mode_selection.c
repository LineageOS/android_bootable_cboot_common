/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <tegrabl_error.h>
#include <tegrabl_edid.h>
#include <tegrabl_hdmi.h>
#include <tegrabl_modes.h>
#include <string.h>

static bool mode_is_equal(struct hdmi_mode *mode1, struct hdmi_mode *mode2)
{
	if ((mode1->width == mode2->width) &&
	    (mode1->height == mode2->height) &&
	    (mode1->frequency == mode2->frequency) &&
	    (mode1->timings.h_ref_to_sync == mode2->timings.h_ref_to_sync) &&
	    (mode1->timings.v_ref_to_sync == mode2->timings.v_ref_to_sync) &&
	    (mode1->timings.h_sync_width == mode2->timings.h_sync_width) &&
	    (mode1->timings.v_sync_width == mode2->timings.v_sync_width) &&
	    (mode1->timings.h_back_porch == mode2->timings.h_back_porch) &&
	    (mode1->timings.v_back_porch == mode2->timings.v_back_porch) &&
	    (mode1->timings.h_front_porch == mode2->timings.h_front_porch) &&
	    (mode1->timings.v_front_porch == mode2->timings.v_front_porch)) {
		return true;
	} else {
		return false;
	}
}

void get_best_mode(struct monitor_data *monitor_info,
				   struct hdmi_mode *best_mode)
{
	uint32_t i;
	uint32_t j;
	uint32_t h_total;
	uint32_t v_total;
	bool mode_found = false;
	struct hdmi_mode m;
	struct hdmi_mode q;

	for (i = 0; i < monitor_info->n_modes; i++) {
		if (monitor_info->modes[i].refresh == 0) {
			h_total = monitor_info->modes[i].timings.h_back_porch +
				monitor_info->modes[i].timings.h_sync_width +
				monitor_info->modes[i].timings.h_disp_active +
				monitor_info->modes[i].timings.h_front_porch;
			v_total = monitor_info->modes[i].timings.v_back_porch +
				monitor_info->modes[i].timings.v_sync_width +
				monitor_info->modes[i].timings.v_disp_active +
				monitor_info->modes[i].timings.v_front_porch;

			monitor_info->modes[i].refresh =
				((monitor_info->modes[i].frequency / h_total) / v_total);
			monitor_info->modes[i].refresh <<= 16;
		}
	}

#if defined(CONFIG_ENABLE_NVDISP_INIT)
	q = s_1024_768_0;    /* Minimum mode that we support that works w/UEFI */
#else
	q = s_640_480_1;
#endif
	for (i = 0; i < monitor_info->n_modes; i++) {
		m = monitor_info->modes[i];
		pr_info("width = %d, height = %d, frequency = %d\n", m.width, m.height,
				m.frequency);

		/* skip unsupported modes */
		mode_found = false;
		for (j = 0; j < size_s_hdmi_modes; j++) {
			if (mode_is_equal(s_hdmi_modes[j], &m)) {
				/* Skip those modes which have frequency
				 * greater than 340 MHz if HF_VSDB
				 * is not present
				 */
				/* TODO: These modes should be marked as YUV420
				 * if 420CMDB and 420VDB are present. So,
				 * do this when YUV420 is implemented
				 */
				if ((!monitor_info->hf_vsdb_present) &&
				    (m.frequency >= 340000000)) {
					mode_found = false;
				} else {
					mode_found = true;
					m.vic = s_hdmi_modes[j]->vic;
				}
				break;
			}
		}
		if (mode_found == false) {
			continue;
		}

		if (q.frequency < m.frequency) {
			q = m;
		} else if (q.frequency == m.frequency) {
			if (q.refresh < m.refresh) {
				q = m;
			}
		}
	}

	memcpy(best_mode, &q, sizeof(struct hdmi_mode));
}
