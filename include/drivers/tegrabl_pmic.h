/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_PMIC_H
#define TEGRABL_PMIC_H

#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include <tegrabl_error.h>

/* pmic info */
typedef struct {
	/* pmic list */
	struct list_node node;
	/* pmic phandle */
	int32_t phandle;
	/* pmic name */
	char name[20];
	/* I2c bus instance */
	uint32_t i2c_instance;
	/* irq vector */
	uint32_t irq;
	/* pmic operations */
	tegrabl_error_t (*poweroff) (void);
	tegrabl_error_t (*get_reset_reason) (uint32_t *buf);
	/* TODO - Events, Interrupts not supported yet */
} tegrabl_pmic_t;

/**
 * @brief struct -contains func pointers to aid pmic chips' hooks
 * with pmic interface
 */
struct tegrabl_pmic_config {
	/* get reason for the previous/latest reset */
	tegrabl_error_t (*get_reset_reason)(uint32_t *buf);

	/* TODO - add other relevant pmic hooks/APIs here */
};

/**
 * @brief - helps initialize the list
 */
void tegrabl_pmic_init(void);

/**
 * @brief - helps registering pmic chip specific routines with this interface
 * @return - error code
 */
tegrabl_error_t tegrabl_pmic_register(tegrabl_pmic_t *drv);

/**
 * @brief - Obtain the reason for the last pmic reset
 * @buf - buffer to be filled with reset reason
 * @return - error code
 */
tegrabl_error_t tegrabl_pmic_get_reset_reason(uint32_t *buf);

/**
 * @brief - performs poweroff
 * @return - error code
 */
tegrabl_error_t tegrabl_pmic_poweroff(void);

#endif /* TEGRABL_PMIC_H */
