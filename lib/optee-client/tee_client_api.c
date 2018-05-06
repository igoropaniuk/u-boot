/*
 * Copyright (c) 2015-2016, Linaro Limited
 * All rights reserved.
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


#include "teec_sysdeps.h"
#include "teec_debug.h"
#include "teec_core.h"

#include <tee_client_api.h>

#ifndef __aligned
#define __aligned(x) __attribute__((__aligned__(x)))
#endif

TEEC_Result TEEC_InitializeContext(const char *name,
				   TEEC_Context *ctx)
{
	int fd;
	size_t n;
	uint32_t gen_caps;

	if (!ctx)
		return TEEC_ERROR_BAD_PARAMETERS;

	/* TODO: remove magic numbers */
	ctx->fd = 5;
	ctx->reg_mem = 0;
	return TEEC_SUCCESS;
}

void TEEC_FinalizeContext(TEEC_Context *ctx)
{
	/* do nothing */
}


TEEC_Result TEEC_OpenSession(TEEC_Context *ctx,
			     TEEC_Session *session,
			     const TEEC_UUID *destination,
			     uint32_t connection_method,
			     const void *connection_data,
			     TEEC_Operation *operation, uint32_t *ret_origin)
{
	int rc;
	TEEC_Result res;
	uint32_t eorig;
	struct tee_open_session_arg *arg;
	struct tee_param *param;

	if (!ctx || !session) {
		eorig = TEEC_ORIGIN_API;
		res = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	arg = teec_alloc(sizeof(struct tee_open_session_arg) +
			 sizeof(struct tee_param) *
			 TEEC_CONFIG_PAYLOAD_REF_COUNT);

	param = teec_alloc(sizeof(struct tee_param) *
			   TEEC_CONFIG_PAYLOAD_REF_COUNT);

	memcpy(arg->uuid, destination, sizeof(*destination));

	arg->num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;

	optee_open_session(arg, param);


out:
	if (ret_origin)
		*ret_origin = eorig;
	return res;
}

void TEEC_CloseSession(TEEC_Session *session)
{
}

TEEC_Result TEEC_InvokeCommand(TEEC_Session *session,
			       uint32_t cmd_id,
			       TEEC_Operation *operation,
			       uint32_t *error_origin)
{
	return TEEC_SUCCESS;
}

void TEEC_RequestCancellation(TEEC_Operation *operation)
{
}

TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *ctx,
				      TEEC_SharedMemory *shm)
{
	return TEEC_SUCCESS;
}


TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *ctx,
				      TEEC_SharedMemory *shm)
{
	return TEEC_SUCCESS;
}

void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *shm)
{
}
