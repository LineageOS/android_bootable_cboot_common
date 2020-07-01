/*
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <tegrabl_linuxboot_utils.h>
#include <tegrabl_cpubl_params.h>

int32_t tegrabl_bom_compare(struct tegrabl_carveout_info *p_carveout, const uint32_t a, const uint32_t b)
{
    if (p_carveout[a].base < p_carveout[b].base)
        return -1;
    else if (p_carveout[a].base > p_carveout[b].base)
        return 1;
    else
        return 0;
}

void tegrabl_sort(struct tegrabl_carveout_info *p_carveout, uint32_t array[], int32_t count)
{
    uint32_t val;
    int32_t i;
    int32_t j;

    if (count < 2)
        return;

    for (i = 1; i < count; i++) {
        val = array[i];

        for (j = (i - 1);
                 (j >= 0) && (tegrabl_bom_compare(p_carveout, val, array[j]) < 0);
                 j--) {
            array[j + 1] = array[j];
        }

        array[j + 1] = val;
    }
}

