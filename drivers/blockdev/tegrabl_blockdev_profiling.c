/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <tegrabl_blockdev.h>
#include <tegrabl_blockdev_profiling.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_debug.h>
#include <inttypes.h>

void profile_read_start(tegrabl_bdev_t *dev)
{
	dev->last_read_start_time = tegrabl_get_timestamp_us();
}

void profile_read_end(tegrabl_bdev_t *dev, uint64_t size)
{
	dev->last_read_end_time = tegrabl_get_timestamp_us();
	dev->total_read_time +=
		(dev->last_read_end_time - dev->last_read_start_time);
	if (size > 0) {
		dev->total_read_size += size;
	}
}

void profile_write_start(tegrabl_bdev_t *dev)
{
	dev->last_write_start_time = tegrabl_get_timestamp_us();
}

void profile_write_end(tegrabl_bdev_t *dev, uint64_t size)
{
	dev->last_write_end_time = tegrabl_get_timestamp_us();
	dev->total_write_time += (dev->last_write_end_time -
		dev->last_write_start_time);
	if (size > 0) {
		dev->total_write_size += size;
	}
}

void list_kpi(struct tegrabl_bdev_struct *bdevs)
{
	tegrabl_bdev_t *entry;
	time_t read_time = 0;
	time_t write_time = 0;
	uint64_t read_size = 0;
	uint64_t write_size = 0;

	list_for_every_entry(&bdevs->list, entry, tegrabl_bdev_t, node) {
		read_time += entry->total_read_time;
		read_size += entry->total_read_size;
		write_time += entry->total_write_time;
		write_size += entry->total_write_size;
	}

	pr_info("read time = %" PRIu64" ms, read size = %"PRIu64" KB\n",
			read_time, read_size / 1000);
	pr_info("write time = %" PRIu64" ms, write size = %"PRIu64" KB\n",
			write_time, write_size / 1000);
}

