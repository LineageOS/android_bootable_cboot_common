/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 * Copyright (c) 2016, NVIDIA Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Optimized memset() for ARM.
 *
 * memset() returns its first argument.
 */

#include <tegrabl_asm.h>

	.section .text
	.syntax unified
/* void asm_memset(void *s, int ch, size_t num) */
FUNCTION(memset)
	/* compute the offset to align the destination
	 * offset = (4-(src&3))&3 = -src & 3
	 */
	stmfd       sp!, {r0, r4-r7, lr}

	rsb         r3, r0, #0
	ands        r3, r3, #3
	cmp         r3, r2
	movhi       r3, r2

	/* splat r1 */
	mov         r1, r1, lsl #24
	orr         r1, r1, r1, lsr #8
	orr         r1, r1, r1, lsr #16

	movs        r12, r3, lsl #31
	strbcs      r1, [r0], #1    /* can't use strh (alignment unknown) */
	strbcs      r1, [r0], #1
	strbmi      r1, [r0], #1
	subs        r2, r2, r3
	popls       {r0, r4-r7, pc}    /* return */

	/* align the destination to a cache-line */
	mov         lr, r1

	rsb         r3, r0, #0
	ands        r3, r3, #0x1C
	beq         3f
	cmp         r3, r2
	andhi       r3, r2, #0x1C
	sub         r2, r2, r3

	/* conditionally writes 0 to 7 words (length in r3) */
	movs        r3, r3, lsl #28
	stmcs       r0!, {r1, lr}
	stmcs       r0!, {r1, lr}
	stmmi       r0!, {r1, lr}
	movs        r3, r3, lsl #2
	strcs       r1, [r0], #4

3:
	subs        r2, r2, #32
	mov         r3, r1
	mov         r4, r1
	bmi         2f

.Ldma_memset32:
	/* check if DMA callback pointer (cached in r12) is valid */
	ldr         r12, _dma_memset_callback
	ldr         r12, [r12]
	cmp         r12, #0
	beq         4f

	/*
	 * Retrieve from stack the DMA threshold and private-data pointer and
	 * store the same in r5 and r6 respectively
	 */
	ldr         r5, _dma_memset_threshold
	ldr         r5, [r5]

	/* r6 = Round-down((r2 + 32), 32) */
	add         r6, r2, #32
	bic         r6, r6, #0x1f

	/* Compare against the threshold */
	cmp         r6, r5
	blo         4f

	/* Prepare parameters for DMA callback, preserving the original r0-r2
	 * and lr into callee-saved registers */
	mov         r7, r2	/* save r2 in r7 */
	mov         r3, r6
	mov         r2, r1
	mov         r1, r0

	ldr         r0, _dma_memset_priv
	ldr         r0, [r0]

	mov         r5, r1	/* save original value of r0 in r5 */
	mov         r4, r2	/* save original value of r1 in r4 */

	/* Call dma_offload_func(priv_data, s, ch, bytes) */
	blx         r12

	/* check if the offload copy was successful */
	cmp         r0, #0
	/* restore r0, r1, r2 */
	mov         r0, r5
	mov         r1, r4
	mov         r2, r7
	/* if offload copy was successful, adjust r0 and r2 */
	addeq       r0, r0, r6
	subeq       r2, r2, r6
	mov         lr, r1
	mov         r3, r1
	mov         r4, r1
	beq         2f

4:
	mov         r5, r1
	mov         r6, r1
	mov         r7, r1
	mov			r12, r1

1:
	subs        r2, r2, #32
	stmia       r0!, {r1,r3,r4,r5,r6,r7,r12,lr}
	bhs         1b

2:
	add         r2, r2, #32

	/* conditionally stores 0 to 31 bytes */
	movs        r2, r2, lsl #28
	stmcs       r0!, {r1,r3,r4,lr}
	stmmi       r0!, {r1, lr}
	movs        r2, r2, lsl #2
	strcs       r1, [r0], #4
	strhmi      r1, [r0], #2
	movs        r2, r2, lsl #2
	strbcs      r1, [r0]
	ldmfd       sp!, {r0, r4-r7, pc}

	.balign 4
_dma_memset_priv:
	.word clib_dma_memset_priv
_dma_memset_threshold:
	.word clib_dma_memset_threshold
_dma_memset_callback:
	.word clib_dma_memset_callback
