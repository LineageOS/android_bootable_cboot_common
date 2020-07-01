/* rsa.c
**
** Copyright (c) 2015-2016, NVIDIA Corporation.  All Rights Reserved.
**
** NVIDIA Corporation and its licensors retain all intellectual property and
** proprietary rights in and to this software and related documentation.  Any
** use, reproduction, disclosure or distribution of this software and related
** documentation without an express license agreement from NVIDIA Corporation
** is strictly prohibited.
**
** Copyright 2012, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of Google Inc. nor the names of its contributors may
**       be used to endorse or promote products derived from this software
**       without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY Google Inc. ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
** EVENT SHALL Google Inc. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
** PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
** OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
** OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
** ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <lib/mincrypt/rsa.h>
#include <lib/mincrypt/sha.h>
#include <lib/mincrypt/sha256.h>

/* a[] -= mod*/
static void sub_m(const struct rsa_public_key *key, uint32_t *a)
{
	int64_t A = 0;
	int i;
	for (i = 0; i < key->len; ++i) {
		A += (uint64_t) a[i] - key->n[i];
		a[i] = (uint32_t) A;
		A >>= 32;
	}
}

/* return a[] >= mod*/
static int ge_m(const struct rsa_public_key *key, const uint32_t *a)
{
	int i;
	for (i = key->len; i;) {
		--i;
		if (a[i] < key->n[i])
			return 0;
		if (a[i] > key->n[i])
			return 1;
	}
	/* equal*/
	return 1;
}

/* montgomery c[] += a * b[] / R % mod*/
static void mont_mul_add(const struct rsa_public_key *key,
		       uint32_t *c, const uint32_t a, const uint32_t *b)
{
	uint64_t A = (uint64_t)a * b[0] + c[0];
	uint32_t d0 = (uint32_t)A * key->n0inv;
	uint64_t B = (uint64_t)d0 * key->n[0] + (uint32_t)A;
	int i;

	for (i = 1; i < key->len; ++i) {
		A = (A >> 32) + (uint64_t)a * b[i] + c[i];
		B = (B >> 32) + (uint64_t)d0 * key->n[i] + (uint32_t)A;
		c[i - 1] = (uint32_t) B;
	}

	A = (A >> 32) + (B >> 32);

	c[i - 1] = (uint32_t) A;

	if (A >> 32)
		sub_m(key, c);
}

/* montgomery c[] = a[] * b[] / R % mod*/
static void mont_mul(const struct rsa_public_key *key,
		    uint32_t *c, const uint32_t *a, const uint32_t *b)
{
	int i;
	for (i = 0; i < key->len; ++i)
		c[i] = 0;
	for (i = 0; i < key->len; ++i)
		mont_mul_add(key, c, a[i], b);
}

/* In-place public exponentiation.*/
/* Input and output big-endian byte array in inout.*/
static void modpow(const struct rsa_public_key *key, uint8_t *inout)
{
	uint32_t a[RSANUMWORDS];
	uint32_t a_r[RSANUMWORDS];
	uint32_t aa_r[RSANUMWORDS];
	uint32_t *aaa = 0;
	int i;

	if (key->exponent != 3 && key->exponent != 65537)
		return;

	/* Convert from big endian byte array to little endian word array.*/
	for (i = 0; i < key->len; ++i) {
		uint32_t tmp =
		    (inout[((key->len - 1 - i) * 4) + 0] << 24) |
		    (inout[((key->len - 1 - i) * 4) + 1] << 16) |
		    (inout[((key->len - 1 - i) * 4) + 2] << 8) |
		    (inout[((key->len - 1 - i) * 4) + 3] << 0);
		a[i] = tmp;
	}

	if (key->exponent == 65537) {
		aaa = aa_r;	/* Re-use location.*/
		mont_mul(key, a_r, a, key->rr);	/* a_r = a * RR / R mod M*/
		for (i = 0; i < 16; i += 2) {
			mont_mul(key, aa_r, a_r, a_r);	/* aa_r = a_r * a_r / R mod M*/
			mont_mul(key, a_r, aa_r, aa_r);	/* a_r = aa_r * aa_r / R mod M*/
		}
		mont_mul(key, aaa, a_r, a);	/* aaa = a_r * a / R mod M*/
	} else if (key->exponent == 3) {
		aaa = a_r;	/* Re-use location.*/
		mont_mul(key, a_r, a, key->rr);	/* a_r = a * RR / R mod M   */
		mont_mul(key, aa_r, a_r, a_r);	/* aa_r = a_r * a_r / R mod M */
		mont_mul(key, aaa, aa_r, a);	/* aaa = aa_r * a / R mod M */
	}

	/* Make sure aaa < mod; aaa is at most 1x mod too large.*/
	if (ge_m(key, aaa))
		sub_m(key, aaa);
	/* Convert to bigendian byte array*/
	for (i = key->len - 1; i >= 0; --i) {
		uint32_t tmp = aaa[i];
		*inout++ = tmp >> 24;
		*inout++ = tmp >> 16;
		*inout++ = tmp >> 8;
		*inout++ = tmp >> 0;
	}
}

/* Expected PKCS1.5 signature padding bytes, for a keytool RSA signature.
 * Has the 0-length optional parameter encoded in the ASN1 (as opposed to the
 * other flavor which omits the optional parameter entirely). This code does not
 * accept signatures without the optional parameter.*/

/*
static const uint8_t sha_padding[RSANUMBYTES] = {
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x30, 0x21, 0x30,
    0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a,
    0x05, 0x00, 0x04, 0x14,

    // 20 bytes of hash go here.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
*/

/* SHA-1 of PKCS1.5 signature sha_padding for 2048 bit, as above.
 * At the location of the bytes of the hash all 00 are hashed.*/
static const uint8_t kExpectedPadSha_rsa2048[SHA_DIGEST_SIZE] = {
	0xdc, 0xbd, 0xbe, 0x42, 0xd5, 0xf5, 0xa7, 0x2e,
	0x6e, 0xfc, 0xf5, 0x5d, 0xaf, 0x9d, 0xea, 0x68,
	0x7c, 0xfb, 0xf1, 0x67
};

/*
static const uint8_t sha256_padding[RSANUMBYTES] = {
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x30, 0x31, 0x30,
    0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65,
    0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20,

    // 32 bytes of hash go here.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
*/

/* SHA-256 of PKCS1.5 signature sha256_padding for 2048 bit, as above.
 * At the location of the bytes of the hash all 00 are hashed.*/
static const uint8_t kExpectedPadSha256Rsa2048[SHA256_DIGEST_SIZE] = {
	0xab, 0x28, 0x8d, 0x8a, 0xd7, 0xd9, 0x59, 0x92,
	0xba, 0xcc, 0xf8, 0x67, 0x20, 0xe1, 0x15, 0x2e,
	0x39, 0x8d, 0x80, 0x36, 0xd6, 0x6f, 0xf0, 0xfd,
	0x90, 0xe8, 0x7d, 0x8b, 0xe1, 0x7c, 0x87, 0x59,
};

/* Verify a 2048-bit RSA PKCS1.5 signature against an expected hash.
 * Both e=3 and e=65537 are supported.  hash_len may be
 * SHA_DIGEST_SIZE (== 20) to indicate a SHA-1 hash, or
 * SHA256_DIGEST_SIZE (== 32) to indicate a SHA-256 hash.  No other
 * values are supported.
 *
 * Returns 1 on successful verification, 0 on failure.*/
int rsa_verify(const struct rsa_public_key *key,
	       const uint8_t *signature,
	       const int len, const uint8_t *hash, const int hash_len)
{
	uint8_t buf[RSANUMBYTES];
	int i;
	const uint8_t *padding_hash;

	if (key->len != RSANUMWORDS)
		/* Wrong key passed in.*/
		return 0;

	if (len != sizeof(buf))
		/* Wrong input length.*/
		return 0;

	if (hash_len != SHA_DIGEST_SIZE && hash_len != SHA256_DIGEST_SIZE)
		/* Unsupported hash.*/
		return 0;

	if (key->exponent != 3 && key->exponent != 65537)
		/* Unsupported exponent.*/
		return 0;

	/* Copy input to local workspace.*/
	for (i = 0; i < len; ++i)
		buf[i] = signature[i];

	/* In-place exponentiation.*/
	modpow(key, buf);

	/* Xor sha portion, so it all becomes 00 iff equal.*/
	for (i = len - hash_len; i < len; ++i)
		buf[i] ^= *hash++;

	/* Hash resulting buf, in-place.*/
	switch (hash_len) {
	case SHA_DIGEST_SIZE:
		padding_hash = kExpectedPadSha_rsa2048;
		sha_hash(buf, len, buf);
		break;
	case SHA256_DIGEST_SIZE:
		padding_hash = kExpectedPadSha256Rsa2048;
		sha256_hash(buf, len, buf);
		break;
	default:
		return 0;
	}

	/* Compare against expected hash value.*/
	for (i = 0; i < hash_len; ++i) {
		if (buf[i] != padding_hash[i])
			return 0;
	}

	/* All checked out OK.*/
	return 1;
}
