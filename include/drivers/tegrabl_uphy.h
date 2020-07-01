/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_UPHY_H
#define TEGRABL_UPHY_H

/**
 * @file tegrabl_uphy.h
 * @brief Header file for Common UPHY driver
 *
 * This file contains structures used for defining UPHY lanes, plls
 * and API's provided to initialize UPHY
 */

#include <tegrabl_error.h>
#include <tegrabl_module.h>

/**
 * @brief Lane owners of uphy
 */
/* macro tegrabl uphy owner */
typedef uint32_t tegrabl_uphy_owner_t;
#define TEGRABL_UPHY_XUSB 0U
#define TEGRABL_UPHY_SATA 1U
#define TEGRABL_UPHY_MPHY 2U
#define TEGRABL_UPHY_UNASSIGNED 3U

/**
 * @brief Init types for uphy
 */
/*macro tegrabl_uphy_init_type_t*/
typedef uint32_t tegrabl_uphy_init_type_t;
#define	TEGRABL_UPHY_FULL_INIT 0U
#define	TEGRABL_UPHY_SKIP_INIT_UPDATE_CONFIG 1U
#define	TEGRABL_UPHY_SKIP_INIT 2U

/**
 * @brief Lane config structure to configure uphy
 */
struct tegrabl_uphy_lane_config {
	uint32_t lane_id; /* Index of Lane */
	tegrabl_uphy_owner_t	owner;	/* Lane owner */
};

/**
 * @brief Macro for Lane PLL selection
 */
#define PLL_SEL_ALL_RIGHT	0xf
#define PLL_SEL_ALL_LEFT	0


/**
 * @brief Lane structure definition to access/control lanes
 */
struct tegrabl_uphy_lane {
	tegrabl_uphy_owner_t owner;	/* Lane owner */
	uint8_t module_id;	/* Reset handle for Lane */
	uint32_t	pll_sel;
	uint32_t	base;	/* Base address of Lane */
};

/**
 * @brief Pll structure definition to access/control plls
 */
struct tegrabl_uphy_pll {
	uint8_t module_id;	/* Reset ID of PLL */
	tegrabl_module_t mgmt_clk; /* UPHY Mgmt clock ID */
	uint32_t	base;	/* Base address of PLL */
};

/**
 * @brief Structure to access fuse values
 */
struct tegrabl_fuse_calib {
	uint32_t    sata_mphy_odm;
	uint32_t    sata_nv;
	uint32_t    mphy_nv;
};

/**
 * @brief Structure for programming PLL / Lane defaults
 */
struct init_data {
	uint8_t cfg_addr;
	uint16_t cfg_wdata;
};

/**
 * @brief Structure for fuse calibration
 */
struct tegrabl_uphy_fuse_config {
	uint8_t aux_rx_idle_th;
	uint8_t tx_drv_amp_sel0;
	uint8_t tx_drv_amp_sel1;
	uint8_t tx_drv_amp_sel2;
	uint8_t tx_drv_amp_sel3;
	uint8_t tx_drv_amp_sel4;
	uint8_t tx_drv_amp_sel5;
	uint8_t tx_drv_amp_sel6;
	uint8_t tx_drv_amp_sel7;
	uint8_t tx_drv_amp_sel8;
	uint8_t tx_drv_amp_sel9;
	uint8_t tx_drv_post_sel0;
	uint8_t tx_drv_post_sel1;
	uint8_t tx_drv_post_sel2;
	uint8_t tx_drv_post_sel3;
	uint8_t tx_drv_post_sel4;
	uint8_t tx_drv_post_sel5;
	uint8_t tx_drv_post_sel6;
	uint8_t tx_drv_post_sel7;
	uint8_t tx_drv_post_sel8;
	uint8_t tx_drv_post_sel9;
	uint8_t tx_drv_pre_sel2;
	uint8_t tx_drv_pre_sel3;
	uint8_t tx_drv_pre_sel4;
	uint8_t tx_drv_pre_sel5;
	uint8_t tx_drv_pre_sel6;
	uint8_t tx_drv_pre_sel7;
	uint8_t tx_drv_pre_sel8;
	uint8_t tx_drv_pre_sel9;
};

/**
 * @brief SOC structure to get soc specific information
 */
struct tegrabl_uphy_soc_data {
	struct tegrabl_uphy_lane *lanes;
	struct tegrabl_uphy_pll *plls;
	struct init_data *lane_defaults[3];
	struct init_data *pll_defaults[3];
	struct tegrabl_uphy_fuse_config *calib[3];
	uint32_t num_lanes;
	uint32_t num_plls;
	uint32_t num_lane_defaults[3];
	uint32_t num_pll_defaults[3];
	uint32_t num_fuse_calib[3];
};

/**
 * @brief UPHY structure
 */

struct tegrabl_uphy {
	struct tegrabl_uphy_soc_data *soc;
	struct tegrabl_fuse_calib fuse_calib;
	bool sata_enabled;
	bool ufs_enabled;
};

/**
 * @brief Handle to uphy driver.
 */
struct tegrabl_uphy_handle {
	tegrabl_error_t (*init)(tegrabl_uphy_owner_t owner);
	void (*power_down)(tegrabl_uphy_owner_t owner);
};

/**
 * @brief Initializes the UPHY Controller for specified lane configuration
 *
 * @param instance UPHY instance number
 *
 * @param config UPHY Lane configuration
 *
 * @param num_of_configs Number of lane configuration structures to be parsed
 *
 * @param phuphy pointer to handle of uphy struct
 *
 * @param init Init type for UPHY
 *
 * @return Address of tegrabl_uphy structure if successful else NULL
 */
tegrabl_error_t tegrabl_uphy_init(uint32_t instance,
								  struct tegrabl_uphy_lane_config *config,
								  uint32_t num_of_configs,
								  struct tegrabl_uphy **phuphy,
								  tegrabl_uphy_init_type_t init);

/**
 * @brief Deinit UPHY for the provided uphy handle.
 *
 * @param uphy Address of the uphy structure which is initialized
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
tegrabl_error_t tegrabl_uphy_deinit(struct tegrabl_uphy *uphy);

/**
 * @brief API to get default lane configuration
 *
 * @param instance UPHY instance number
 *
 * @param config pointer to get reference of tegrabl_uphy_lane_config struct
 *
 * @param num_of_configs pointer to know the num of configurations
 *
 * @param uphy_clients List of clients whose default config needs to be returned
 *
 * @param num_of_clients Size of uphy_clients array
 */

tegrabl_error_t tegrabl_uphy_get_default_config(
					uint32_t instance,
					struct tegrabl_uphy_lane_config **config,
					uint32_t *num_of_configs,
					uint32_t uphy_clients[],
					uint32_t num_of_clients);

#endif /* TEGRABL_UPHY_H */
