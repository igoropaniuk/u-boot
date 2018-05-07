/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2018, Linaro Limited
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define offsetof(type, member)  __builtin_offsetof (type, member)

#define SMCCC_QUIRK_NONE		0
#define SMCCC_QUIRK_QCOM_A6		1 /* Save/restore register a6 */
#define SMCCC_RES_X0_OFFS		0 /* offsetof(struct arm_smccc_res, a0) */
#define SMCCC_RES_X2_OFFS		16 /* offsetof(struct arm_smccc_res, a2) */
#define SMCCC_QUIRK_ID_OFFS		0 /* offsetof(struct arm_smccc_quirk, id) */
#define SMCCC_QUIRK_STATE_OFFS		4 /*offsetof(struct arm_smccc_quirk, state) */

/*
 * void smccc_smc(unsigned long a0, unsigned long a1, unsigned long a2,
 *		  unsigned long a3, unsigned long a4, unsigned long a5,
 *		  unsigned long a6, unsigned long a7, struct arm_smccc_res
 *		  *res)
 */
	.globl smccc_smc
	smccc_smc:
		smc	#0
		ldr	x4, [sp]
		stp	x0, x1, [x4, #SMCCC_RES_X0_OFFS]
		stp	x2, x3, [x4, #SMCCC_RES_X2_OFFS]
		ldr	x4, [sp, #8]
		cbz	x4, 1f /* no quirk structure */
		ldr	x9, [x4, #SMCCC_QUIRK_ID_OFFS]
		cmp	x9, #SMCCC_QUIRK_QCOM_A6
		b.ne	1f
		str	x6, [x4, SMCCC_QUIRK_STATE_OFFS]
	1:	ret

