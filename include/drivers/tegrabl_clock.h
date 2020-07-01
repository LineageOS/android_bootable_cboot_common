/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_TEGRABL_CLOCK_H
#define INCLUDED_TEGRABL_CLOCK_H

#include <stdbool.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <arclk_rst.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define PLLP_FIXED_FREQ_KHZ_13000            13000
#define PLLP_FIXED_FREQ_KHZ_216000          216000
#define PLLP_FIXED_FREQ_KHZ_408000          408000
#define PLLP_FIXED_FREQ_KHZ_432000          432000

/**
 * @brief enum for plls that might be used. Not all of them might be
 * supported. Add new plls to this list and update the clock driver add
 * support for the new pll.
 */
enum tegrabl_clk_pll_id {
	TEGRABL_CLK_PLL_ID_PLLP = 0,					/* 0x0 */
	TEGRABL_CLK_PLL_ID_PLLC4 = 1,					/* 0x1 */
	TEGRABL_CLK_PLL_ID_PLLD = 2,					/* 0x2 */
	TEGRABL_CLK_PLL_ID_PLLD2 = 3,					/* 0x3 */
	/* TEGRABL_CLK_PLL_ID_PLLD3 at 14 */
	TEGRABL_CLK_PLL_ID_PLLDP = 4,					/* 0x4 */
	TEGRABL_CLK_PLL_ID_PLLE = 5,					/* 0x5 */
	TEGRABL_CLK_PLL_ID_PLLM = 6,					/* 0x6 */
	TEGRABL_CLK_PLL_ID_SATA_PLL = 7,				/* 0x7 */
	TEGRABL_CLK_PLL_ID_UTMI_PLL = 8,				/* 0x8 */
	TEGRABL_CLK_PLL_ID_XUSB_PLL = 9,				/* 0x9 */
	TEGRABL_CLK_PLL_ID_AON_PLL = 10,				/* 0xA */
	TEGRABL_CLK_PLL_ID_PLLDISPHUB = 11,				/* 0xB */
	TEGRABL_CLK_PLL_ID_PLL_NUM = 12,				/* 0xC */
	TEGRABL_CLK_PLL_ID_PLLMSB = 13,					/* 0xD */
	TEGRABL_CLK_PLL_ID_PLLD3 = 14,					/* 0xE */
	TEGRABL_CLK_PLL_ID_MAX = 15,					/* 0xF */
	TEGRABL_CLK_PLL_ID_PLL_FORCE32 = 2147483647ULL,	/* 0x7FFFFFFF */
};

/**
 * @brief - enum for possible module clock divisors
 * @TEGRABL_CLK_DIV_TYPE_REGULAR - Divide by (N + 1)
 * @TEGRABL_CLK_DIV_TYPE_FRACTIONAL - Divide by (N/2 + 1)
 * where N is the divisor value written to the clock source register
 * one of the PLLs). Not all of them are supported.
 */
enum tegrabl_clk_div_type {
	TEGRABL_CLK_DIV_TYPE_INVALID = 0x0,
	TEGRABL_CLK_DIV_TYPE_REGULAR,
	TEGRABL_CLK_DIV_TYPE_FRACTIONAL,
	TEGRABL_CLK_DIV_TYPE_FORCE32 = 0x7fffffff,
};

/**
 * @brief - enum for possible clock sources
 * Add new sources to this list and update tegrabl_clk_get_src_freq()
 * to take care of the newly added source (usually a derivative of one of
 * one of the PLLs). Not all of them are supported.
 */
enum tegrabl_clk_src_id_t {
	TEGRABL_CLK_SRC_INVALID = 0x0,
	TEGRABL_CLK_SRC_CLK_M,
	TEGRABL_CLK_SRC_CLK_S, /* 0x2 */
	TEGRABL_CLK_SRC_PLLP_OUT0,
	TEGRABL_CLK_SRC_PLLM_OUT0, /* 0x4 */
	TEGRABL_CLK_SRC_PLLC_OUT0,
	TEGRABL_CLK_SRC_PLLC4_MUXED, /* 0x6 */
	TEGRABL_CLK_SRC_PLLC4_VCO,
	TEGRABL_CLK_SRC_PLLC4_OUT0_LJ, /* 0x8 */
	TEGRABL_CLK_SRC_PLLC4_OUT1,
	TEGRABL_CLK_SRC_PLLC4_OUT1_LJ, /* 0xA */
	TEGRABL_CLK_SRC_PLLC4_OUT2,
	TEGRABL_CLK_SRC_PLLC4_OUT2_LJ, /* 0xC */
	TEGRABL_CLK_SRC_PLLE,
	TEGRABL_CLK_SRC_PLLAON_OUT, /* 0xE */
	TEGRABL_CLK_SRC_PLLD_OUT1,
	TEGRABL_CLK_SRC_PLLD2_OUT0, /* 0x10 */
	TEGRABL_CLK_SRC_PLLD3_OUT0,
	TEGRABL_CLK_SRC_PLLDP, /* 0x12 */
	TEGRABL_CLK_SRC_NVDISPLAY_P0_CLK,
	TEGRABL_CLK_SRC_NVDISPLAY_P1_CLK, /* 0x14 */
	TEGRABL_CLK_SRC_NVDISPLAY_P2_CLK,
	TEGRABL_CLK_SRC_SOR0, /* 0x16*/
	TEGRABL_CLK_SRC_SOR1,
	TEGRABL_CLK_SRC_SOR_SAFE_CLK, /* 0x18 */
	TEGRABL_CLK_SRC_SOR0_PAD_CLKOUT,
	TEGRABL_CLK_SRC_SOR1_PAD_CLKOUT, /* 0x1A */
	TEGRABL_CLK_SRC_DFLLDISP_DIV,
	TEGRABL_CLK_SRC_PLLDISPHUB_DIV, /* 0x1C */
	TEGRABL_CLK_SRC_PLLDISPHUB,
	TEGRABL_CLK_SRC_DUMMY, /* 0x1E */
	TEGRABL_CLK_SRC_NUM = 0x7fffffff,
};


/*
 * @brief - enum for possible set of oscillator frequencies
 * supported in the internal API + invalid (measured but not in any valid band)
 * + unknown (not measured at all)
 * Define tegrabl_clk_osc_freq here to have the correct collection of
 * oscillator frequencies.
 */
enum tegrabl_clk_osc_freq {
	/* Specifies an oscillator frequency of 13MHz.*/
	TEGRABL_CLK_OSC_FREQ_13 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC13,

	/* Specifies an oscillator frequency of 19.2MHz. */
	TEGRABL_CLK_OSC_FREQ_19_2 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC19P2,

	/* Specifies an oscillator frequency of 12MHz. */
	TEGRABL_CLK_OSC_FREQ_12 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC12,

	/* Specifies an oscillator frequency of 26MHz. */
	TEGRABL_CLK_OSC_FREQ_26 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC26,

	/* Specifies an oscillator frequency of 16.8MHz. */
	TEGRABL_CLK_OSC_FREQ_16_8 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC16P8,

	/* Specifies an oscillator frequency of 38.4MHz. */
	TEGRABL_CLK_OSC_FREQ_38_4 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC38P4,

	/* Specifies an oscillator frequency of 48MHz. */
	TEGRABL_CLK_OSC_FREQ_48 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC48,

	TEGRABL_CLK_OSC_FREQ_NUM = 7, /* dummy to get number of frequencies */
	TEGRABL_CLK_OSC_FREQ_MAX_VAL = 13, /* dummy to get the max enum value */
	TEGRABL_CLK_OSC_FREQ_UNKNOWN = 15, /* illegal/undefined frequency */
	TEGRABL_CLK_OSC_FREQ_FORCE32 = 0x7fffffff
};

/**
 * ------------------------NOTES------------------------
 * Please read below before using these APIs.
 * 1) For using APIs that query clock state, namely get_clk_rate()
 * and get_clk_src(), it is necessary that clk_enable has been
 * called for the module before regardless of whether the clock is
 * enabled by default on POR. This is how the driver keeps initializes
 * the module clock states.
 * 2) set_clk_src() will not directly update the clk_source register if the
 * clock is disabled. The new settings will only take effect when the clock
 * is enabled.
 * 3) set_clk_rate() will also enable the module clock in addition to
 * configuring it to the specified rate.
 */


/**
 * @brief Configures the clock source and divider if needed
 * and enables clock for the module specified.
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @priv_data - module specific private data pointer to module specific clock
 * init data
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_clk_enable(tegrabl_module_t module,
					     uint8_t instance,
					     void *priv_data);

/**
 * @brief  Disables clock for the module specified
 *
 * @module  Module ID of the module
 * @instance  Instance of the module
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_clk_disable(tegrabl_module_t module,
					uint8_t instance);

/**
 * @brief Puts the module in reset
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_rst_set(tegrabl_module_t module,
				    uint8_t instance);

/**
 * @brief  Releases the module from reset
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_rst_clear(tegrabl_module_t module,
				      uint8_t instance);

/**
 * @brief - Gets the current clock source of the module
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @return - Enum of clock source if module is found and has a valid clock source
 * configured. TEGRABL_CLK_SRC_INVAID otherwise.
 */
enum tegrabl_clk_src_id_t tegrabl_car_get_clk_src(
		tegrabl_module_t module,
		uint8_t instance);


/**
 * @brief - Gets the current clock rate of the module
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @rate_khz - Address to store the current clock rate
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_get_clk_rate(
		tegrabl_module_t module,
		uint8_t instance,
		uint32_t *rate_khz);

/**
 * @brief - Attempts to set the current clock rate of
 * the module to the value specified and returns the actual rate set.
 * NOTE: If the module clock is disabled when this function is called,
 * it will also enable the clock.
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @rate_khz - Rate requested
 * @rate_set_khz - Rate set
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_set_clk_rate(
		tegrabl_module_t module,
		uint8_t instance,
		uint32_t rate_khz,
		uint32_t *rate_set_khz);

/**
 * @brief - Sets the clock source of the module to
 * the source specified.
 * NOTE: If the module clock is disabled when this function is called,
 * the new settings will take effect only after enabling the clock.
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @clk_src - Specified source
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_set_clk_src(
		tegrabl_module_t module,
		uint8_t instance,
		enum tegrabl_clk_src_id_t clk_src);

/**
 * @brief - Configures the essential PLLs, Oscillator,
 * and other essential clocks.
 */
void tegrabl_car_clock_init(void);

/**
 * @brief - Returns the enum of oscillator frequency
 * @return - Enum value of current oscillator frequency
 */
enum tegrabl_clk_osc_freq tegrabl_get_osc_freq(void);

/**
 * @brief - Returns the oscillator frequency in KHz
 *
 * @freq_khz - Pointer to store the freq in kHz
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_get_osc_freq_khz(uint32_t *freq_khz);

/**
 * @brief - Initializes the pll specified by pll_id.
 * Does nothing if pll already initialized
 *
 * @pll_id - ID of the pll to be initialized
 * @rate_khz - Rate to which the PLL is to be initialized
 * @priv_data - Any PLL specific initialization data to send
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_init_pll_with_rate(
		enum tegrabl_clk_pll_id pll_id, uint32_t rate_khz,
		void *priv_data);

/**
 * @brief - Get current frequency of the specified
 * clock source.
 *
 * @src_id - enum of the clock source
 * @rate_khz - Address to store the frequency of the clock source
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_get_clk_src_rate(
		enum tegrabl_clk_src_id_t src_id,
		uint32_t *rate_khz);

/**
 * @brief - Set frequency for the specified
 * clock source.
 *
 * @src_id - enum of the clock source
 * @rate_khz - the frequency of the clock source
 * @rate_set_khz - Address to store the rate set
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_set_clk_src_rate(
		enum tegrabl_clk_src_id_t src_id,
		uint32_t rate_khz,
		uint32_t *rate_set_khz);

/**
 * @brief - Configures PLLM0 for WB0 Override
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_pllm_wb0_override(void);

/**
 * @brief - Configures CAR dividers for slave TSC
 *
 * Configuration is done for both OSC and PLL paths.
 * If OSC >= 38400, Osc is chosen as source
 * else PLLP is chosen as source.
 *
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_setup_tsc_dividers(void);

/**
 * @brief - Set/Clear fuse register visibility
 *
 * @param visibility if true, it will make all reg visible otherwise invisible.
 *
 * @return existing visibility before programming the value
 */
bool tegrabl_set_fuse_reg_visibility(bool visibility);

/**
 * @brief Power downs plle.
 */
void tegrabl_car_disable_plle(void);

/**
 * @brief init usb clocks for host
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_usbh_clock_init(void);

/**
 * @brief init usb clocks
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_usbf_clock_init(void);

/**
 * @brief enable/disable tracking unit clock for host
 *
 * @param is_enable boolean value to enable/disable clock
 */
void tegrabl_usbh_program_tracking_clock(bool is_enable);

/**
 * @brief enable/disable tracking unit clock
 *
 * @param is_enable boolean value to enable/disable clock
 */
void tegrabl_usbf_program_tracking_clock(bool is_enable);

/**
 * @brief perform init of ufs clocks
 *
 * @param
 */
tegrabl_error_t tegrabl_ufs_clock_init(void);

/**
 * @brief perform deinit of ufs clocks
 *
 * @param
 */
void tegrabl_ufs_clock_deinit(void);

/**
 * @brief checks whether clock is enabled for the module or not
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 *
 * @return true if clock is enabled for module else false
 */
bool tegrabl_car_clk_is_enabled(tegrabl_module_t module, uint8_t instance);

#if defined(__cplusplus)
}
#endif

#endif  /* INCLUDED_TEGRABL_CLOCK_H */
