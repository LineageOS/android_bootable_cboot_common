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

#ifndef ASN1_DECODER_H_
#define ASN1_DECODER_H_

#include <stdint.h>
#include <stdlib.h>

struct asn1_context {
	size_t length;
	uint8_t *p;
	int app_type;
};

/**
 * @brief Generate asn1_context struct with an asn1 block
 * @param buffer block address
 * @param length asn1 block size
 * @return pointer of generated struct
 */
struct asn1_context *asn1_context_new(uint8_t *buffer, size_t length);

/**
 * @brief Free asn1_context struct
 * @param ctx asn1_context to be freed
 */
void asn1_context_free(struct asn1_context *ctx);

/**
 * @brief Get first constructed block from input asn1_context
 * @param ctx input struct
 * @return pointer of the desired struct
 */
struct asn1_context *asn1_constructed_get(struct asn1_context *ctx);

/**
 * @brief Skip all constructed block in input asn1_context
 * @param ctx input asn1_context
 * @return true if successful, false if an error occurs
 */
bool asn1_constructed_skip_all(struct asn1_context *ctx);

/**
 * @brief return the type of input asn1_context
 * @param ctx input asn1_context
 * @return The asn1_context type
 */
int asn1_constructed_type(struct asn1_context *ctx);

/**
 * @brief Get first sequence in input asn1_context
 * @param ctx input asn1_context
 * @return the pointer of the sequence
 */
struct asn1_context *asn1_sequence_get(struct asn1_context *ctx);

/**
 * @brief Get first set in input asn1_context
 * @param ctx input asn1_context
 * @return the pointer of the set
 */
struct asn1_context *asn1_set_get(struct asn1_context *ctx);

/**
 * @brief Skip one sequence
 * @param ctx input asn1_context
 * @return true if successful, false if an error occurs
 */
bool asn1_sequence_next(struct asn1_context *seq);

/**
 * @brief Skip several sequences
 * @param ctx input asn1_context
 * @return true if successful, false if an error occurs
 */
bool asn1_sequence_skip(struct asn1_context *seq, size_t num);

/**
 * @brief Get first oid in input asn1_context
 * @param ctx input asn1_context
 * @param oid oid returned
 * @param length length of returned oid
 * @return true if successful, false if an error occurs
 */
bool asn1_oid_get(struct asn1_context *ctx, uint8_t **oid, size_t *length);

/**
 * @brief Get first octet string in input asn1_context
 * @param ctx input asn1_context
 * @param octet_string octet string returned
 * @param length length of returned octet string
 * @return true if successful, false if an error occurs
 */
bool asn1_octet_string_get(struct asn1_context *ctx, uint8_t **octet_string,
						   size_t *length);

/**
 * @brief Get first integer in input asn1_context
 * @param ctx input asn1_context
 * @param integer integer returned
 * @param length length of returned integer
 * @return true if successful, false if an error occurs
 */
bool asn1_integer_get(struct asn1_context *ctx, uint8_t **integer,
					  size_t *length);

/**
 * @brief Get first bit string in input asn1_context
 * @param ctx input asn1_context
 * @param bit_string bit string returned
 * @param num_padding_bits number of unused bits appended to the bitstream
 * @param length length of returned bit string
 * @return true if successful, false if an error occurs
 */
bool asn1_bit_string_get(struct asn1_context *ctx, uint8_t **bit_string,
						 uint8_t *num_padding_bits, size_t *length);

#endif /* ASN1_DECODER_H_ */
