/*
 * Copyright (c) 2006 David Gibson, IBM Corporation.
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All rights reserved.
 *
 * Nvidia is integrating libfdt under the terms of BSD license only.
 *
 *  Redistribution and use in source and binary forms, with or
 *  without modification, are permitted provided that the following
 *  conditions are met:
 *
 *  1. Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer.
 *  2. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "libfdt_env.h"

#include <fdt.h>
#include <libfdt.h>
#include <libfdt_local.h>

#include "libfdt_internal.h"

static int _fdt_blocks_misordered(const void *fdt,
			      int mem_rsv_size, int struct_size)
{
	return (fdt_off_mem_rsvmap(fdt) < FDT_ALIGN(sizeof(struct fdt_header), 8))
		|| (fdt_off_dt_struct(fdt) <
		    (fdt_off_mem_rsvmap(fdt) + mem_rsv_size))
		|| (fdt_off_dt_strings(fdt) <
		    (fdt_off_dt_struct(fdt) + struct_size))
		|| (fdt_totalsize(fdt) <
		    (fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt)));
}

static int _fdt_rw_check_header(void *fdt)
{
	FDT_CHECK_HEADER(fdt);

	if (fdt_version(fdt) < 17)
		return -FDT_ERR_BADVERSION;
	if (_fdt_blocks_misordered(fdt, sizeof(struct fdt_reserve_entry),
				   fdt_size_dt_struct(fdt)))
		return -FDT_ERR_BADLAYOUT;
	if (fdt_version(fdt) > 17)
		fdt_set_version(fdt, 17);

	return 0;
}

#define FDT_RW_CHECK_HEADER(fdt) \
	{ \
		int err; \
		if ((err = _fdt_rw_check_header(fdt)) != 0) \
			return err; \
	}

int fdt_add_subnode_if_absent(void *fdt, int parentnode, char *nodename)
{
	int node;

	FDT_RW_CHECK_HEADER(fdt);

	node = fdt_subnode_offset(fdt, parentnode, nodename);
	if (node < 0)
		node = fdt_add_subnode(fdt, parentnode, nodename);

	return node;
}
