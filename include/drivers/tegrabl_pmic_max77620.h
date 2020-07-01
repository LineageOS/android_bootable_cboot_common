/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_PMIC_MAX77620_H
#define INCLUDED_TEGRABL_PMIC_MAX77620_H

/**
 * @brief - Registers max77620 driver routines with the pmic interface
 * @return - Error code
 */
tegrabl_error_t tegrabl_max77620_init(uint32_t i2c_instance);

/**
 * @brief - Un-registers max77620 driver routines with the pmic interface
 * @return - Error code
 */
tegrabl_error_t tegrabl_max77620_un_init(void);
#endif /*INCLUDED_TEGRABL_PMIC_MAX77620_H*/
