/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.	All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <malloc.h>
#include <stdint.h>
#include <string.h>

#include "asn1_decoder.h"

static const int kmask_constructed = 0xE0;
static const int kmask_tag = 0x7F;
static const int kmask_apptype = 0x1F;

static const int ktag_octet_string = 0x04;
static const int ktag_oid = 0x06;
static const int ktag_sequence = 0x30;
static const int ktag_set = 0x31;
static const int ktag_constructed = 0xA0;
static const int ktag_bit_string = 0x03;
static const int ktag_integer = 0x02;

struct asn1_context *asn1_context_new(uint8_t *buffer, size_t length)
{
	struct asn1_context *ctx =
	    (struct asn1_context *)calloc(1, sizeof(struct asn1_context));
	if (ctx == NULL)
		return NULL;
	ctx->p = buffer;
	ctx->length = length;
	return ctx;
}

void asn1_context_free(struct asn1_context *ctx)
{
	free(ctx);
}

static inline int peek_byte(struct asn1_context *ctx)
{
	if (ctx->length <= 0)
		return -1;
	return *ctx->p;
}

static inline int get_byte(struct asn1_context *ctx)
{
	if (ctx->length <= 0)
		return -1;
	int byte = *ctx->p;
	ctx->p++;
	ctx->length--;
	return byte;
}

static inline bool skip_bytes(struct asn1_context *ctx, size_t num_skip)
{
	if (ctx->length < num_skip)
		return false;
	ctx->p += num_skip;
	ctx->length -= num_skip;
	return true;
}

static bool decode_length(struct asn1_context *ctx, size_t *out_len)
{
	int num_octets = get_byte(ctx);
	if (num_octets == -1)
		return false;
	if ((num_octets & 0x80) == 0x00) {
		*out_len = num_octets;
		return 1;
	}
	num_octets &= kmask_tag;
	if ((size_t) num_octets >= sizeof(size_t))
		return false;
	size_t length = 0;
	for (int i = 0; i < num_octets; ++i) {
		int byte = get_byte(ctx);
		if (byte == -1)
			return false;
		length <<= 8;
		length += byte;
	}
	*out_len = length;
	return true;
}

/**
 * Returns the constructed type and advances the pointer. E.g. A0 -> 0
 */
struct asn1_context *asn1_constructed_get(struct asn1_context *ctx)
{
	int type = get_byte(ctx);
	if (type == -1 || (type & kmask_constructed) != ktag_constructed)
		return NULL;
	size_t length;
	if (!decode_length(ctx, &length) || length > ctx->length)
		return NULL;
	struct asn1_context *app_ctx = asn1_context_new(ctx->p, length);
	app_ctx->app_type = type & kmask_apptype;
	return app_ctx;
}

bool asn1_constructed_skip_all(struct asn1_context *ctx)
{
	int byte = peek_byte(ctx);
	while (byte != -1 && (byte & kmask_constructed) == ktag_constructed) {
		skip_bytes(ctx, 1);
		size_t length;
		if (!decode_length(ctx, &length) || !skip_bytes(ctx, length))
			return false;
		byte = peek_byte(ctx);
	}
	return byte != -1;
}

int asn1_constructed_type(struct asn1_context *ctx)
{
	return ctx->app_type;
}

struct asn1_context *asn1_sequence_get(struct asn1_context *ctx)
{
	if ((get_byte(ctx) & kmask_tag) != ktag_sequence)
		return NULL;
	size_t length;
	if (!decode_length(ctx, &length) || length > ctx->length)
		return NULL;
	return asn1_context_new(ctx->p, length);
}

struct asn1_context *asn1_set_get(struct asn1_context *ctx)
{
	if ((get_byte(ctx) & kmask_tag) != ktag_set)
		return NULL;
	size_t length;
	if (!decode_length(ctx, &length) || length > ctx->length)
		return NULL;
	return asn1_context_new(ctx->p, length);
}

bool asn1_sequence_next(struct asn1_context *ctx)
{
	size_t length;
	if (get_byte(ctx) == -1 || !decode_length(ctx, &length) ||
		!skip_bytes(ctx, length)) {
		return false;
	}
	return true;
}

bool asn1_sequence_skip(struct asn1_context *seq, size_t num)
{
	size_t i = 0;
	while (i < num && asn1_sequence_next(seq))
		i++;
	if (i < num)
		return false;
	return true;
}

bool asn1_oid_get(struct asn1_context *ctx, uint8_t **oid, size_t *length)
{
	if (get_byte(ctx) != ktag_oid)
		return false;
	if (!decode_length(ctx, length) || *length == 0 || *length > ctx->length)
		return false;

	*oid = ctx->p;
	return true;
}

bool asn1_octet_string_get(struct asn1_context *ctx, uint8_t **octet_string,
			   size_t *length)
{
	if (get_byte(ctx) != ktag_octet_string)
		return false;
	if (!decode_length(ctx, length) || *length == 0 || *length > ctx->length)
		return false;

	*octet_string = ctx->p;
	return true;
}

/* If the integer occupies more than one byte, the array will contain
 * bytes of integers in big endian byte ordering
 */
bool asn1_integer_get(struct asn1_context *ctx, uint8_t **integer,
					  size_t *length)
{
	if (get_byte(ctx) != ktag_integer)
		return false;

	if (!decode_length(ctx, length) || *length == 0 || *length > ctx->length)
		return false;

	*integer = ctx->p;
	skip_bytes(ctx, *length);

	return true;
}

bool asn1_bit_string_get(struct asn1_context *ctx, uint8_t **bit_string,
				uint8_t *num_pad_bits, size_t *length)
{
	if (get_byte(ctx) != ktag_bit_string)
		return false;

	if (!decode_length(ctx, length) || *length == 0 || *length > ctx->length)
		return false;

	*num_pad_bits = get_byte(ctx);
	*bit_string = ctx->p;

	return true;
}
