
/*
 * (C) Copyright 2018, Linaro Limited
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <command.h>
#include <image.h>
#include <linux/kernel.h>
#include <malloc.h>

#include <tee_client_api.h>

#define TA_HELLO_WORLD_UUID \
	{ 0x8aaaf200, 0x2450, 0x11e4, \
		{ 0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b} }

int do_teec_init(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc != 1)
		return CMD_RET_USAGE;

	if (1)
		return CMD_RET_SUCCESS;

	return CMD_RET_FAILURE;
}

int do_teec_open_session(cmd_tbl_t *cmdtp,
			 int flag,
			 int argc,
			 char * const argv[])
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;

	if (argc != 1)
		return CMD_RET_USAGE;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed with code 0x%x",
		       res);
		return CMD_RET_FAILURE;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed with code 0x%x origin 0x%x",
		       res, err_origin);
		return CMD_RET_FAILURE;

	}
	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_teec[] = {
	U_BOOT_CMD_MKENT(init, 1, 0, do_teec_init, "", ""),
	U_BOOT_CMD_MKENT(open_session, 1, 0, do_teec_open_session, "", "")
};

static int do_teec(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	cp = find_cmd_tbl(argv[1], cmd_teec, ARRAY_SIZE(cmd_teec));

	argc--;
	argv++;

	if (!cp || argc > cp->maxargs)
		return CMD_RET_USAGE;

	if (flag == CMD_FLAG_REPEAT)
		return CMD_RET_FAILURE;

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	teec, 29, 0, do_teec,
	"Provides commands for testing OPTEE client functionality",
	"init - initialize optee client lib\n"
	"teec open_session  - Open Session to TA, UUID is hardcoded\n"
	"teec invoke_command - Invoke Commands to TA, UUID is hardcoded\n"
	);
