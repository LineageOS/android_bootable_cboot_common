/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#define MODULE TEGRABL_ERR_NVDISP

#include <tegrabl_ar_macro.h>
#include <ardisplay.h>
#include <ardisplay_a.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_utils.h>
#include <tegrabl_nvdisp.h>

struct reg_off_range {
	uint32_t start;
	uint32_t end;
};

struct reg_off_range cmd_reg_list[] = {
	{DC_CMD0_FIRST_REG, DC_CMD0_LAST_REG},
	{DC_CMD1_FIRST_REG, DC_CMD1_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD2_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD3_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD4_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD5_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD6_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD7_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD8_LAST_REG},
	{DC_CMD0_FIRST_REG, DC_CMD9_LAST_REG},
	{DC_CMD10_FIRST_REG, DC_CMD10_LAST_REG},
	{DC_CMD11_FIRST_REG, DC_CMD11_LAST_REG},
	{DC_CMD12_FIRST_REG, DC_CMD12_LAST_REG},
	{DC_CMD13_FIRST_REG, DC_CMD13_LAST_REG},
	{DC_CMD14_FIRST_REG, DC_CMD14_LAST_REG},
	{DC_CMD15_FIRST_REG, DC_CMD15_LAST_REG},
	{DC_CMD16_FIRST_REG, DC_CMD16_LAST_REG},
	{DC_CMD17_FIRST_REG, DC_CMD17_LAST_REG},
	{DC_CMD18_FIRST_REG, DC_CMD18_LAST_REG},
	{DC_CMD19_FIRST_REG, DC_CMD19_LAST_REG},
	{DC_CMD20_FIRST_REG, DC_CMD20_LAST_REG},
};

struct reg_off_range com_reg_list[] = {
	{DC_COM0_FIRST_REG, DC_COM0_LAST_REG},
	{DC_COM1_FIRST_REG, DC_COM1_LAST_REG},
	{DC_COM2_FIRST_REG, DC_COM2_LAST_REG},
	{DC_COM3_FIRST_REG, DC_COM3_LAST_REG},
	{DC_COM4_FIRST_REG, DC_COM4_LAST_REG},
	{DC_COM5_FIRST_REG, DC_COM5_LAST_REG},
	{DC_COM6_FIRST_REG, DC_COM6_LAST_REG},
	{DC_COM7_FIRST_REG, DC_COM7_LAST_REG},
	{DC_COM8_FIRST_REG, DC_COM8_LAST_REG},
	{DC_COM9_FIRST_REG, DC_COM9_LAST_REG},
	{DC_COM10_FIRST_REG, DC_COM10_LAST_REG},
};

struct reg_off_range disp_reg_list[] = {
	{DC_DISP0_FIRST_REG, DC_DISP0_LAST_REG},
	{DC_DISP1_FIRST_REG, DC_DISP1_LAST_REG},
	{DC_DISP2_FIRST_REG, DC_DISP2_LAST_REG},
	{DC_DISP3_FIRST_REG, DC_DISP3_LAST_REG},
	{DC_DISP4_FIRST_REG, DC_DISP4_LAST_REG},
	{DC_DISP5_FIRST_REG, DC_DISP5_LAST_REG},
	{DC_DISP6_FIRST_REG, DC_DISP6_LAST_REG},
	{DC_DISP7_FIRST_REG, DC_DISP7_LAST_REG},
	{DC_DISP8_FIRST_REG, DC_DISP8_LAST_REG},
	{DC_DISP9_FIRST_REG, DC_DISP9_LAST_REG},
	{DC_DISP10_FIRST_REG, DC_DISP10_LAST_REG},
	{DC_DISP11_FIRST_REG, DC_DISP11_LAST_REG},
	{DC_DISP12_FIRST_REG, DC_DISP12_LAST_REG},
	{DC_DISP13_FIRST_REG, DC_DISP13_LAST_REG},
	{DC_DISP14_FIRST_REG, DC_DISP14_LAST_REG},
	{DC_DISP15_FIRST_REG, DC_DISP15_LAST_REG},
	{DC_DISP16_FIRST_REG, DC_DISP16_LAST_REG},
	{DC_DISP17_FIRST_REG, DC_DISP17_LAST_REG},
	{DC_DISP18_FIRST_REG, DC_DISP18_LAST_REG},
	{DC_DISP19_FIRST_REG, DC_DISP19_LAST_REG},
#if defined(IS_T186)
	{DC_DISP20_FIRST_REG, DC_DISP20_LAST_REG},
#endif
};

struct reg_off_range winc_reg_list[] = {
	{DC_WINC0_FIRST_REG, DC_WINC0_LAST_REG},
	{DC_WINC1_FIRST_REG, DC_WINC1_LAST_REG},
	{DC_WINC2_FIRST_REG, DC_WINC2_LAST_REG},
	{DC_WINC3_FIRST_REG, DC_WINC3_LAST_REG},
	{DC_WINC4_FIRST_REG, DC_WINC4_LAST_REG},
	{DC_WINC5_FIRST_REG, DC_WINC5_LAST_REG},
	{DC_WINC6_FIRST_REG, DC_WINC6_LAST_REG},
	{DC_WINC7_FIRST_REG, DC_WINC7_LAST_REG},
	{DC_WINC8_FIRST_REG, DC_WINC8_LAST_REG},
	{DC_WINC9_FIRST_REG, DC_WINC9_LAST_REG},
	{DC_WINC10_FIRST_REG, DC_WINC10_LAST_REG},
	{DC_WINC11_FIRST_REG, DC_WINC11_LAST_REG},
	{DC_WINC12_FIRST_REG, DC_WINC12_LAST_REG},
	{DC_WINC13_FIRST_REG, DC_WINC13_LAST_REG},
};


struct reg_off_range win_reg_list[] = {
	{DC_WIN0_FIRST_REG, DC_WIN0_LAST_REG},
	{DC_WIN1_FIRST_REG, DC_WIN1_LAST_REG},
	{DC_WIN2_FIRST_REG, DC_WIN2_LAST_REG},
	{DC_WIN3_FIRST_REG, DC_WIN3_LAST_REG},
	{DC_WIN4_FIRST_REG, DC_WIN4_LAST_REG},
	{DC_WIN5_FIRST_REG, DC_WIN5_LAST_REG},
};

struct reg_off_range winbuf_reg_list[] = {
	{DC_WINBUF0_FIRST_REG, DC_WINBUF0_LAST_REG},
	{DC_WINBUF1_FIRST_REG, DC_WINBUF1_LAST_REG},
	{DC_WINBUF2_FIRST_REG, DC_WINBUF2_LAST_REG},
	{DC_WINBUF3_FIRST_REG, DC_WINBUF3_LAST_REG},
	{DC_WINBUF4_FIRST_REG, DC_WINBUF4_LAST_REG},
};


void nvdisp_dump_register_set(uint32_t *base, struct reg_off_range *list,
							  uint32_t count)
{
	uint32_t i, k;
	struct reg_off_range *curr;
	uint32_t *addr;

	for (k = 0; k < count; k++) {
		curr = &list[k];
		addr = base + curr->start;
		pr_debug("%p: start = %d, end = %d\n", addr, curr->start, curr->end);
		for (i = 0; i <= (curr->end - curr->start); i += 4) {
			pr_debug("%p: %08x %08X %08X %08X\n", addr, *(addr), *(addr + 1),
					 *(addr + 2), *(addr + 3));
			addr += 4;
		}
	}
	pr_debug("\n");
}

void tegrabl_nvdisp_dump_registers(struct tegrabl_nvdisp *nvdisp)
{
	pr_debug("cmd registers:\n");
	nvdisp_dump_register_set((uint32_t *)nvdisp->base_addr, &cmd_reg_list[0],
							 ARRAY_SIZE(cmd_reg_list));
	pr_debug("com registers:\n");
	nvdisp_dump_register_set((uint32_t *)nvdisp->base_addr, &com_reg_list[0],
							 ARRAY_SIZE(com_reg_list));
	pr_debug("disp registers:\n");
	nvdisp_dump_register_set((uint32_t *)nvdisp->base_addr, &disp_reg_list[0],
							 ARRAY_SIZE(disp_reg_list));
	pr_debug("winc registers:\n");
	nvdisp_dump_register_set((uint32_t *)nvdisp->base_addr, &winc_reg_list[0],
							 ARRAY_SIZE(winc_reg_list));
	pr_debug("win registers:\n");
	nvdisp_dump_register_set((uint32_t *)nvdisp->base_addr, &win_reg_list[0],
							 ARRAY_SIZE(win_reg_list));
	pr_debug("winbuf registers:\n");
	nvdisp_dump_register_set((uint32_t *)nvdisp->base_addr, &winbuf_reg_list[0],
							 ARRAY_SIZE(winbuf_reg_list));
}
