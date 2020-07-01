/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PANEL

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_malloc.h>
#include <tegrabl_timer.h>
#include <tegrabl_edid.h>
#include <tegrabl_hdmi.h>
#include <tegrabl_modes.h>
#include <string.h>
#include <tegrabl_dpaux.h>

#define MAX_FREQ 148500000
#define EDID_BLOCK_SIZE 128
#define EDID_SLAVE 0xA0
#define ESTABLISHED_TIMING_BYTE 0x23
#define STANDARD_TIMING_BYTE 0x26
#define DTD_BYTE 0x36
#define NUM_EXTENSIONS_BYTE 126
#define EDID_RETRY_DELAY_US 200
#define EDID_MAX_RETRY 10
#define ID_MANUFACTURER_NAME 0x08
#define DETAILED_TIMING_DESCRIPTIONS_START 0x36
#define DETAILED_TIMING_DESCRIPTION_SIZE 18

#define is_avi_m( \
	h_size, v_size, \
	h_avi_m, v_avi_m) \
	(((h_size) * (v_avi_m)) > ((v_size) * ((h_avi_m) - 1)) &&  \
	((h_size) * (v_avi_m)) < ((v_size) * ((h_avi_m) + 1))) \

/* macro block type */
#define BLOCK_TYPE_AUDIO 1
#define BLOCK_TYPE_VIDEO 2
#define BLOCK_TYPE_VENDOR_SPECIFIC 3
#define BLOCK_TYPE_SPEAKER 4

/* Only exposing supported resolutions */
/* macro video id */
#define VIDEO_ID_640_480_1 1
#define VIDEO_ID_720_480_2 2
#define VIDEO_ID_720_480_3 3
#define VIDEO_ID_1280_720_4 4
#define VIDEO_ID_1920_1080_16 16
#define VIDEO_ID_720_576_17 17
#define VIDEO_ID_720_576_18 18
#define VIDEO_ID_1280_720_19 19
#define VIDEO_ID_1920_1080_31 31
#define VIDEO_ID_1920_1080_32 32
#define VIDEO_ID_1920_1080_33 33
#define VIDEO_ID_1920_1080_34 34
#define VIDEO_ID_3840_2160_95 95
#define VIDEO_ID_3840_2160_97 97
#define VIDEO_ID_Force32 0x7FFFFFFF

static bool is_panel_hdmi;

#if defined (CONFIG_ENABLE_DP)
static tegrabl_error_t dpaux_i2c_dev_read(struct tegrabl_dpaux *hdpaux,
	uint8_t *edid, uint32_t offset, uint32_t size)
{
	struct tegrabl_i2c_transaction msgs[2];
	uint8_t write_buf;

	write_buf = offset;
	msgs[0].slave_addr = EDID_SLAVE >> 1;
	msgs[0].is_write = true;
	msgs[0].buf = &write_buf;
	msgs[0].len = 1;

	msgs[1].slave_addr = EDID_SLAVE >> 1;
	msgs[1].is_write = false;
	msgs[1].buf = edid;
	msgs[1].len = size;

	return tegrabl_dpaux_i2c_transactions(hdpaux, msgs, 2);
}
#endif

tegrabl_error_t read_edid(uint8_t *edid, uint32_t offset, uint32_t module,
						  uint32_t instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i;
	uint8_t last_checksum = 0;
	uint32_t attempt_cnt = 0;
	uint8_t checksum = 0;
	struct tegrabl_i2c_dev *hi2c = NULL;
#if defined (CONFIG_ENABLE_DP)
	struct tegrabl_dpaux *hdpaux;

	if (module == TEGRABL_MODULE_DPAUX) {
		err = tegrabl_dpaux_init_aux(instance, &hdpaux);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: dpaux init failed\n", __func__);
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}
	}
#endif
	if (module == TEGRABL_MODULE_I2C) {
		hi2c = tegrabl_i2c_dev_open(instance, EDID_SLAVE, 1, 1);
		if (!hi2c) {
			pr_error("%s, invalid i2c handle\n", __func__);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
	}

	do {
		checksum = 0;
	#if defined (CONFIG_ENABLE_DP)
		if (module == TEGRABL_MODULE_DPAUX) {
			err = dpaux_i2c_dev_read(hdpaux, edid, offset, EDID_BLOCK_SIZE);
		}
	#endif
		if (module == TEGRABL_MODULE_I2C) {
			err = tegrabl_i2c_dev_read(hi2c, edid, offset, EDID_BLOCK_SIZE);
		}
		if (err != TEGRABL_NO_ERROR) {
			pr_error("could not read edid\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 0);
			goto fail;
		}

		for (i = 0; i < EDID_BLOCK_SIZE; i++) {
			checksum += edid[i];
		}

			if (checksum != 0) {
				/*
				 * It is completely possible that the sink that we are reading has
				 * a bad EDID checksum (specifically, on some of the older TVs).
				 * These TVs have the modes etc programmed in their EDID correctly,
				 * but just have a bad checksum. It becomes hard to distinguish
				 * between an i2c failure vs bad EDID. To get around this, read
				 * the EDID multiple times. If the calculated checksum is exact
				 * same multiple number of times, just print a warning and ignore.
				 */
			if (attempt_cnt == 0) {
				last_checksum = checksum;
			}
			if (last_checksum != checksum) {
				pr_error("%s: checksum failed and did not match consecutive \
						 reads. Previous remainder was %d. New remainder is %d \
						 . Failed at attempt %d\n", __func__, last_checksum,
						 checksum, attempt_cnt);
				err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
				goto fail;
			}
			tegrabl_udelay(EDID_RETRY_DELAY_US);
		}
	} while ((last_checksum != 0) && (++attempt_cnt < EDID_MAX_RETRY));

    pr_info("edid read success\n");

fail:
	return err;
}

static uint8_t get_bit(int8_t in, uint8_t bit)
{
	return (in & (1 << bit)) >> bit;
}

static uint8_t get_bits(uint8_t in, uint8_t begin, uint8_t end)
{
	uint8_t mask = (1 << (end - begin + 1)) - 1;

	return (in >> begin) & mask;
}

void add_mode(struct monitor_data *monitor_info, struct hdmi_mode *m)
{
	if (monitor_info->n_modes >= ARRAY_SIZE(monitor_info->modes)) {
		pr_error("Not enough modes in the mode array!");
		return;
	}

	memcpy(&monitor_info->modes[monitor_info->n_modes++], m, sizeof(*m));
}

bool parse_header(const uint8_t *edid)
{
	uint8_t header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

	if (memcmp(edid, header, 8) == 0) {
		return true;
	}
	return false;
}

#if defined (CONFIG_ENABLE_DISPLAY_MONITOR_INFO)
static bool edid_is_serial_block(uint8_t *block)
{
	if ((block[0] == 0x00) && (block[1] == 0x00) &&
		(block[2] == 0x00) && (block[3] == 0xff) &&
		(block[4] == 0x00))
		return true;
	else
		return false;
}

static bool edid_is_ascii_block(uint8_t *block)
{
	if ((block[0] == 0x00) && (block[1] == 0x00) &&
		(block[2] == 0x00) && (block[3] == 0xfe) &&
		(block[4] == 0x00))
		return true;
	else
		return false;
}

static bool edid_is_monitor_block(uint8_t *block)
{
	if ((block[0] == 0x00) && (block[1] == 0x00) &&
		(block[2] == 0x00) && (block[3] == 0xfc) &&
		(block[4] == 0x00))
		return true;
	else
		return false;
}

static void copy_string(uint8_t *c, uint8_t *s)
{
	uint32_t i;

	c += 5;
	for (i = 0; (i < 13 && *c != 0x0A); i++) {
		*(s++) = *(c++);
	}
	*s = 0;

	while (i-- && (*--s == 0x20)) {
		*s = 0;
	}
}

bool parse_vendor_block(const uint8_t *edid, struct monitor_data *monitor_info)
{
	uint8_t *block = (uint8_t *)edid + DETAILED_TIMING_DESCRIPTIONS_START;
	uint32_t i;

	edid += ID_MANUFACTURER_NAME;
	monitor_info->manufacturer_code[0] = ((edid[0] & 0x7c) >> 2) + '@';
	monitor_info->manufacturer_code[1] = ((edid[0] & 0x03) << 3) +
		((edid[1] & 0xe0) >> 5) + '@';
	monitor_info->manufacturer_code[2] = (edid[1] & 0x1f) + '@';
	monitor_info->manufacturer_code[3] = 0;
	pr_info("Maufacturer = %s\n", monitor_info->manufacturer_code);

	monitor_info->product_code = edid[2] + (edid[3] << 8);
	monitor_info->serial_number = edid[4] + (edid[5] << 8) +
		(edid[6] << 16) + (edid[7] << 24);
	monitor_info->production_year = edid[9] + 1990;
	monitor_info->production_week = edid[8];

	for (i = 0; i < 4; i++, block += DETAILED_TIMING_DESCRIPTION_SIZE) {
		if (edid_is_serial_block(block)) {
			copy_string(block, monitor_info->serial_no);
			pr_info("Serial Number: %s\n", monitor_info->serial_no);
		} else if (edid_is_ascii_block(block)) {
			copy_string(block, monitor_info->ascii);
			pr_info("ASCII Block: %s\n", monitor_info->ascii);
		} else if (edid_is_monitor_block(block)) {
			copy_string(block, monitor_info->monitor);
			pr_info("Monitor Name: %s\n", monitor_info->monitor);
		}
	}

	return true;
}
#endif

bool parse_established_timings(const uint8_t *edid,
							   struct monitor_data *monitor_info)
{
	static const struct timing established[][8] = {
		{
			{ 800, 600, 60 },
			{ 800, 600, 56 },
			{ 640, 480, 75 },
			{ 640, 480, 72 },
			{ 640, 480, 67 },
			{ 640, 480, 60 },
			{ 720, 400, 88 },
			{ 720, 400, 70 }
		},
		{
			{ 1280, 1024, 75 },
			{ 1024, 768, 75 },
			{ 1024, 768, 70 },
			{ 1024, 768, 60 },
			{ 1024, 768, 87 },
			{ 832, 624, 75 },
			{ 800, 600, 75 },
			{ 800, 600, 72 }
		},
		{
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 1152, 870, 75 }
		},
	};

	uint8_t i, j, k;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 8; ++j) {
			uint8_t byte = edid[ESTABLISHED_TIMING_BYTE + i];
			if (get_bit(byte, j) && established[i][j].frequency != 0) {
				for (k = 0; k < size_s_hdmi_modes; k++) {
					if ((s_hdmi_modes[k]->width == established[i][j].width) &&
						(s_hdmi_modes[k]->height == established[i][j].height)) {
						add_mode(monitor_info, s_hdmi_modes[k]);
					}
				}
			}
		}
	}
	return true;
}

bool parse_standard_timings(const uint8_t *edid,
							struct monitor_data *monitor_info)
{
	uint32_t i;
	uint32_t j;
	uint32_t edid_refresh;
	uint32_t refresh;
	uint32_t h_total;
	uint32_t v_total;

	for (i = 0; i < 8; i++) {
		uint32_t first = edid[STANDARD_TIMING_BYTE + (2 * i)];
		uint32_t second = edid[STANDARD_TIMING_BYTE + 1 + (2 * i)];

		if (first != 0x01 && second != 0x01) {
			uint32_t w = 8 * (first + 31);
			uint32_t h;

			switch (get_bits(second, 6, 7)) {
			case 0x00:
				h = (w / 16) * 10;
				break;
			case 0x01:
				h = (w / 4) * 3;
				break;
			case 0x02:
				h = (w / 5) * 4;
				break;
			case 0x03:
				h = (w / 16) * 9;
				break;
			default:
				h = (w / 16) * 9;
				break;
			}

			monitor_info->standard[i].width = w;
			monitor_info->standard[i].height = h;
			edid_refresh = get_bits(second, 0, 5) + 60;

			for (j = 0; j < size_s_hdmi_modes; j++) {
				if ((s_hdmi_modes[j]->width == w) &&
					(s_hdmi_modes[j]->height == h)) {
					if (s_hdmi_modes[j]->refresh == 0) {
						h_total = s_hdmi_modes[j]->timings.h_back_porch +
							s_hdmi_modes[j]->timings.h_sync_width +
							s_hdmi_modes[j]->timings.h_disp_active +
							s_hdmi_modes[j]->timings.h_front_porch;
						v_total = s_hdmi_modes[j]->timings.v_back_porch +
							s_hdmi_modes[j]->timings.v_sync_width +
							s_hdmi_modes[j]->timings.v_disp_active +
							s_hdmi_modes[j]->timings.v_front_porch;
						refresh = ((s_hdmi_modes[j]->frequency / h_total) /
							v_total);
					} else {
						refresh = s_hdmi_modes[j]->refresh;
						refresh >>= 16;
					}
					if (edid_refresh == refresh) {
						add_mode(monitor_info, s_hdmi_modes[j]);
					}
				}
			}
		}
	}

	return true;
}

static void parse_detailed_timing(const uint8_t *timing,
								  struct detailed_timing *detailed)
{
	detailed->pixel_clock = (timing[0x00] | timing[0x01] << 8) * 10000;
	detailed->h_addr = timing[0x02] | ((timing[0x04] & 0xf0) << 4);
	detailed->h_blank = timing[0x03] | ((timing[0x04] & 0x0f) << 8);
	detailed->v_addr = timing[0x05] | ((timing[0x07] & 0xf0) << 4);
	detailed->v_blank = timing[0x06] | ((timing[0x07] & 0x0f) << 8);
	detailed->h_front_porch = timing[0x08] | get_bits(timing[0x0b], 6, 7) << 8;
	detailed->h_sync = timing[0x09] | get_bits(timing[0x0b], 4, 5) << 8;
	detailed->v_front_porch =
		get_bits(timing[0x0a], 4, 7) | get_bits(timing[0x0b], 2, 3) << 4;
	detailed->v_sync =
		get_bits(timing[0x0a], 0, 3) | get_bits(timing[0x0b], 0, 1) << 4;
	detailed->width_mm =  timing[0x0c] | get_bits(timing[0x0e], 4, 7) << 8;
	detailed->height_mm = timing[0x0d] | get_bits(timing[0x0e], 0, 3) << 8;
	detailed->right_border = timing[0x0f];
	detailed->top_border = timing[0x10];
	detailed->interlaced = get_bit(timing[0x11], 7);
}

void fill_mode_with_monitor_data(struct hdmi_mode *m,
								 struct monitor_data *monitor_info,
								 struct detailed_timing *timing)
{
	m->width = timing->h_addr;
	m->height = timing->v_addr;
	m->bpp = 24;
	m->refresh = 0;
	m->frequency = timing->pixel_clock;
	m->flags = 0;
	m->timings.h_ref_to_sync = 1;
	m->timings.v_ref_to_sync = 1;
	m->timings.h_sync_width = timing->h_sync;
	m->timings.v_sync_width = timing->v_sync;
	m->timings.h_disp_active = timing->h_addr;
	m->timings.v_disp_active = timing->v_addr;
	m->timings.h_front_porch = timing->h_front_porch;
	m->timings.v_front_porch = timing->v_front_porch;
	m->timings.h_back_porch = timing->h_blank - timing->h_front_porch -
		timing->h_sync;
	m->timings.v_back_porch = timing->v_blank - timing->v_front_porch -
		timing->v_sync;
}

void parse_descriptors(const uint8_t *edid, struct monitor_data *monitor_info,
					   uint32_t module, uint32_t instance)
{
	uint32_t i;
	uint32_t j;
	uint32_t k;
	uint32_t index;
	struct hdmi_mode m;
	uint32_t total_extensions = 0;
	uint32_t total_dtds = 0;
	uint8_t dtd_start;
	uint8_t edid_extension[EDID_BLOCK_SIZE] = {0};
	uint8_t svd_start;
	uint8_t block_type;
	uint8_t svd_byte;
	uint32_t n_blocks;
	uint32_t cea_table_index;
	struct hdmi_mode *psrc = NULL;
	uint8_t *ptr;
	uint32_t l;
	uint32_t hdmi_vic_len;
	uint8_t tmp;

	/* parse descriptors in basic edid */
	for (i = 0; i < 4; ++i) {
		index = DTD_BYTE + i * 18;
		if (!(edid[index + 0] == 0x00 && edid[index + 1] == 0x00)) {
			struct detailed_timing timing = {0};
			parse_detailed_timing(edid + index, &timing);
			fill_mode_with_monitor_data(&m, monitor_info, &timing);
			add_mode(monitor_info, &m);
		}
	}

	/* this var is used to distinguish between hdmi and dvi panels,
	 * make it explicitly false in case we have multiple displays.
	 */
	is_panel_hdmi = false;

	/* parse descriptors, SVDs in extension blocks */
	total_extensions = edid[NUM_EXTENSIONS_BYTE];
	for (i = 0; i < total_extensions; i++) {
		if (!read_edid(edid_extension, (i + 1) * EDID_BLOCK_SIZE, module,
			instance)) {
			if (edid_extension[0] != 0x2) {
				pr_info("This is not a CEA-extension block!\n");
				break;
			}

			/* parse descriptors */
			/* Bits 3..0 in 3rd byte indicate number of DTDs present
			 * Byte2 indicate the start byte of DTDs
			 * */
			total_dtds = edid_extension[0x3] & 0x0F;
			dtd_start = edid_extension[0x2];
			if (dtd_start == 0) {
				break;
			}
			for (j = 0; j < total_dtds; j++) {
				/* Each DTD will be of size 18 bytes */
				struct detailed_timing timing = {0};
				index = dtd_start + (j * 18);
				parse_detailed_timing(edid_extension + index, &timing);
				fill_mode_with_monitor_data(&m, monitor_info, &timing);
				add_mode(monitor_info, &m);
			}

			/* parse SVDs */
			/* If no non-DTD data is present in this extension block, the value
			 * should be set to 04h. If set to 00h, there are no DTDs present in
			 * this block and no non-DTD data
			 */
			if ((dtd_start == 0x0) || (dtd_start == 0x4)) {
				break;
			}
			j = 0;
			while (1) {
				svd_start = 0x4 + j;
				if (svd_start >= dtd_start) {
					break;
				}
				/* Bit 7..5: Block Type Tag (1 is audio, 2 is video, 3 is
				 * vendor specific, 4 is speaker allocation, all other
				 * values Reserved)
				 * Bit 4..0: Total number of bytes in this block following
				 * this byte
				 */
				block_type = (edid_extension[svd_start] & 0xE0);
				block_type = block_type >> 5;
				n_blocks = (edid_extension[svd_start] & 0x1F);
				switch (block_type) {
				case BLOCK_TYPE_VIDEO:
					for (k = 0; k < n_blocks; k++) {
						/* Any Video Data Block will contain one or more 1-byte
						 * Short Video Descriptors (SVDs).
						 * Bit 7: 1 to designate that this should be considered
						 * a "native" resolution, 0 for non-native.
						 * Bit 6..0: index value to a table of standard
						 * resolutions/timings from CEA/EIA-861F:
						 */
						svd_byte = edid_extension[svd_start + k + 1];
						cea_table_index = (svd_byte & 0x7F);

						psrc = NULL;
						switch (cea_table_index) {
						case VIDEO_ID_640_480_1:
							psrc = &s_640_480_1;
							break;
						case VIDEO_ID_720_480_2:
						case VIDEO_ID_720_480_3:
							psrc = &s_720_480_2;
							break;
						case VIDEO_ID_1280_720_4:
							psrc = &s_1280_720_4;
							break;
						case VIDEO_ID_1920_1080_16:
							psrc = &s_1920_1080_16;
							break;
						case VIDEO_ID_720_576_17:
						case VIDEO_ID_720_576_18:
							psrc = &s_720_576_17;
							break;
						case VIDEO_ID_1280_720_19:
							psrc = &s_1280_720_19;
							break;
						case VIDEO_ID_1920_1080_31:
							psrc = &s_1920_1080_31;
							break;
						case VIDEO_ID_1920_1080_32:
							psrc = &s_1920_1080_32;
							break;
						case VIDEO_ID_1920_1080_33:
							psrc = &s_1920_1080_33;
							break;
						case VIDEO_ID_1920_1080_34:
							psrc = &s_1920_1080_34;
							break;
						case VIDEO_ID_3840_2160_95:
							psrc = &s_3840_2160_95;
							break;
						case VIDEO_ID_3840_2160_97:
							psrc = &s_3840_2160_97;
							break;
						default:
							break;
						}
						if (psrc) {
							add_mode(monitor_info, psrc);
						}
					}
					j = j + n_blocks + 1;
					break;
				case BLOCK_TYPE_VENDOR_SPECIFIC:
					ptr = &edid_extension[svd_start];

					/* OUI for hdmi forum */
					if ((ptr[1] == 0xd8) &&
						(ptr[2] == 0x5d) &&
						(ptr[3] == 0xc4)) {
						is_panel_hdmi = true;
						monitor_info->hf_vsdb_present = true;
						j = j + n_blocks + 1;
						break;
					}

					/* 24-bit IEEE Registration Identifier for hdmi licensing */
					if ((ptr[1] == 0x03) &&
						(ptr[2] == 0x0c) &&
						(ptr[3] == 0)) {
						is_panel_hdmi = true;
					}

					if (n_blocks >= 8 &&
						(ptr[1] == 0x03) &&
						(ptr[2] == 0x0c) &&
						(ptr[3] == 0)) {
						l = 8;
						tmp = ptr[l++];
						/* HDMI_Video_present? */
						if (tmp & 0x20) {
							/* Latency_Fields_present? */
							if (tmp & 0x80) {
								l += 2;
							}
							/* I_Latency_Fields_present? */
							if (tmp & 0x40) {
								l += 2;
							}
							/* HDMI_VIC_LEN */
							if (++l <= n_blocks && (ptr[l] & 0xe0)) {
								uint32_t n = 0;
								hdmi_vic_len = ptr[l] >> 5;
								for (n = 0; n < hdmi_vic_len; n++) {
									switch (ptr[l+n+1]) {
									case 1:
										psrc = &s_hdmi_vic_1;
										break;
									case 2:
										psrc = &s_hdmi_vic_2;
										break;
									case 3:
										psrc = &s_hdmi_vic_3;
										break;
									case 4:
										psrc = &s_hdmi_vic_4;
										break;
									default:
										psrc = NULL;
										break;
									}
									if (psrc) {
										add_mode(monitor_info, psrc);
									}
								}
							}
						}
					}
					j = j + n_blocks + 1;
					break;
				default:
					j = j + n_blocks + 1;
					break;
				}
			}
		}
	}
}

static uint8_t calc_default_avi_m(uint32_t h_size, uint32_t v_size)
{
	if (!h_size || !v_size) {
		pr_error("invalid h_size %u or v_size %u\n", h_size, v_size);
		goto fail;
	}

	if (is_avi_m(h_size, v_size, 256, 135))
		return NVDISP_MODE_AVI_M_256_135;
	else if (is_avi_m(h_size, v_size, 64, 27))
		return NVDISP_MODE_AVI_M_64_27;
	else if (is_avi_m(h_size, v_size, 16, 9))
		return NVDISP_MODE_AVI_M_16_9;
	else if (is_avi_m(h_size, v_size, 4, 3))
		return NVDISP_MODE_AVI_M_4_3;
fail:
	return NVDISP_MODE_AVI_M_NO_DATA;
}

tegrabl_error_t parse_edid(const uint8_t *edid, struct hdmi_mode *best_mode,
						   uint32_t module, uint32_t instance)
{
	struct monitor_data *monitor_info;
	tegrabl_error_t status = TEGRABL_NO_ERROR;

	monitor_info = tegrabl_malloc(sizeof(struct monitor_data));
	memset(monitor_info, 0, sizeof(struct monitor_data));

	memcpy(best_mode, &s_1920_1080_16, sizeof(struct hdmi_mode));

	if (!parse_header(edid)) {
		status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}
#if defined (CONFIG_ENABLE_DISPLAY_MONITOR_INFO)
	if (!parse_vendor_block(edid, monitor_info)) {
		status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}
#endif
	if (!parse_established_timings(edid, monitor_info)) {
		status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	if (!parse_standard_timings(edid, monitor_info)) {
		status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		goto fail;
	}

	parse_descriptors(edid, monitor_info, module, instance);

	if (monitor_info->n_modes == 0)
		goto fail;
	else
		get_best_mode(monitor_info, best_mode);

	pr_info("Best mode Width = %d, Height = %d, freq = %d\n",
			best_mode->width, best_mode->height, best_mode->frequency);

fail:
	tegrabl_free(monitor_info);
	return status;
}

void mode_from_hdmi_mode(struct nvdisp_mode *modes, struct hdmi_mode *mode)
{
	modes->pclk = mode->frequency;
	modes->h_ref_to_sync = mode->timings.h_ref_to_sync;
	modes->v_ref_to_sync = mode->timings.v_ref_to_sync;
	modes->h_sync_width = mode->timings.h_sync_width;
	modes->v_sync_width = mode->timings.v_sync_width;
	modes->h_back_porch = mode->timings.h_back_porch;
	modes->v_back_porch = mode->timings.v_back_porch;
	modes->h_active = mode->width;
	modes->v_active = mode->height;
	modes->h_front_porch = mode->timings.h_front_porch;
	modes->v_front_porch = mode->timings.v_front_porch;
	modes->vic = mode->vic;
	if (modes->flags)
		modes->avi_m = modes->flags;
	else
		modes->avi_m = calc_default_avi_m(modes->h_active, modes->v_active);
}

tegrabl_error_t tegrabl_edid_get_mode(struct nvdisp_mode *modes,
									  uint32_t module, uint32_t instance)
{
	uint8_t edid[EDID_BLOCK_SIZE] = {0};
	struct hdmi_mode *mode = NULL;
	tegrabl_error_t status = TEGRABL_NO_ERROR;

	mode = tegrabl_malloc(sizeof(struct hdmi_mode));
	if (mode == NULL) {
		pr_error("memory allocation failed!\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	if (read_edid(edid, 0x0, module, instance) == TEGRABL_NO_ERROR) {
		status = parse_edid(edid, mode, module, instance);
		if (status != TEGRABL_NO_ERROR) {
			pr_debug("%s, parse edid failed\n", __func__);
			status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
			goto fail;
		}
	} else {
		pr_debug("%s, read edid failed, using default mode\n", __func__);
		memcpy(mode, &s_1920_1080_16, sizeof(struct hdmi_mode));
	}

	mode_from_hdmi_mode(modes, mode);

fail:
	if (mode != NULL) {
		tegrabl_free(mode);
	}
	return status;
}

bool tegrabl_edid_is_panel_hdmi(void)
{
	return is_panel_hdmi;
}

