// SPDX-License-Identifier: GPL-2.0-only
/*
 * sha512_ce_glue.c - SHA-512 secure hash using ARMv8 Crypto Extensions
 *
 * Copyright (C) 2024 Igor Opaniuk <igor.opaniuk@gmail.com>
 */

#include <asm/system.h>
#include <u-boot/sha512.h>

extern void sha512_ce_transform(uint64_t state[SHA512_SUM_LEN / 8],
				const uint8_t *src, uint32_t blocks);

/*
 * The SHA512H, SHA512H2, SHA512SU0 and SHA512SU1 instructions are an
 * optional part of the ARMv8.2 Crypto Extensions: ID_AA64ISAR0_EL1.SHA2
 * is 0b0010 when they are implemented, 0b0001 for SHA256 only.
 */
static bool sha512_ce_supported(void)
{
	uint64_t isar0;

	asm volatile("mrs %0, id_aa64isar0_el1" : "=r" (isar0));

	return (isar0 & ID_AA64ISAR0_EL1_SHA2) >= ID_AA64ISAR0_EL1_SHA2_SHA512;
}

void sha512_block_fn(sha512_context *ctx, const uint8_t *src, int blocks)
{
	if (blocks <= 0)
		return;

	if (sha512_ce_supported())
		sha512_ce_transform(ctx->state, src, blocks);
	else
		sha512_block_fn_generic(ctx, src, blocks);
}
