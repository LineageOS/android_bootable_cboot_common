/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
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
#include <tegrabl_soc_clock.h>

#if defined(__cplusplus)
extern "C"
{
#endif

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
tegrabl_clk_src_id_t tegrabl_car_get_clk_src(
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
		tegrabl_clk_src_id_t clk_src);

/**
 * @brief - Configures the essential PLLs, Oscillator,
 * and other essential clocks.
 */
void tegrabl_car_clock_init(void);

/**
 * @brief - Returns the enum of oscillator frequency
 * @return - Enum value of current oscillator frequency
 */
tegrabl_clk_osc_freq_t tegrabl_get_osc_freq(void);

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
		tegrabl_clk_pll_id_t pll_id, uint32_t rate_khz,
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
		tegrabl_clk_src_id_t src_id,
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
		tegrabl_clk_src_id_t src_id,
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
 * @brief init usb clocks
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_usbf_clock_init(void);

/**
 * @brief powergate XUSBA and XUSBC partitions
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_usb_powergate(void);

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

/**
 * @brief checks whether clock's reset is set or clear
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @state - Status of reset
 *
 * @return TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_clk_get_reset_state(tegrabl_module_t module, uint8_t instance, bool *state);

#if defined(__cplusplus)
}
#endif

#endif  /* INCLUDED_TEGRABL_CLOCK_H */
