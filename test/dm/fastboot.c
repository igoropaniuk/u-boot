// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 */

#include <blk.h>
#include <dm.h>
#include <env.h>
#include <fastboot.h>
#include <fb_mmc.h>
#include <malloc.h>
#include <mmc.h>
#include <part.h>
#include <part_efi.h>
#include <dm/test.h>
#include <test/ut.h>
#include <linux/stringify.h>
#include <u-boot/sha256.h>

#define FB_ALIAS_PREFIX "fastboot_partition_alias_"

static int dm_test_fastboot_mmc_part(struct unit_test_state *uts)
{
	char response[FASTBOOT_RESPONSE_LEN] = {0};
	char str_disk_guid[UUID_STR_LEN + 1];
	struct blk_desc *mmc_dev_desc, *fb_dev_desc;
	struct disk_partition part_info;
	struct disk_partition parts[2] = {
		{
			.start = 48, /* GPT data takes up the first 34 blocks or so */
			.size = 1,
			.name = "test1",
		},
		{
			.start = 49,
			.size = 1,
			.name = "test2",
		},
	};

	/*
	 * There are a lot of literal 0s I don't want to have to construct from
	 * MMC_DEV.
	 */
	ut_asserteq(0, CONFIG_FASTBOOT_FLASH_MMC_DEV);
	ut_assertok(blk_get_device_by_str("mmc", "0", &mmc_dev_desc));
	if (CONFIG_IS_ENABLED(RANDOM_UUID)) {
		gen_rand_uuid_str(parts[0].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(parts[1].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(str_disk_guid, UUID_STR_FORMAT_STD);
	}
	ut_assertok(gpt_restore(mmc_dev_desc, str_disk_guid, parts,
				ARRAY_SIZE(parts)));

	/* "Classic" partition labels */
	ut_asserteq(1, fastboot_mmc_get_part_info("test1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(2, fastboot_mmc_get_part_info("test2", &fb_dev_desc,
						  &part_info, response));

	/* Test aliases */
	ut_assertnull(env_get(FB_ALIAS_PREFIX "test3"));
	ut_assertok(env_set(FB_ALIAS_PREFIX "test3", "test1"));
	ut_asserteq(1, fastboot_mmc_get_part_info("test3", &fb_dev_desc,
						  &part_info, response));
	ut_assertok(env_set(FB_ALIAS_PREFIX "test3", NULL));

	/* "New" partition labels */
	ut_asserteq(1, fastboot_mmc_get_part_info("#test1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info("0#test1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info("0.0#test1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info("0:1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info("0.0:1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info("0", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info("0.0", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(0, fastboot_mmc_get_part_info("0:0", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(0, fastboot_mmc_get_part_info("0.0:0", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(0, fastboot_mmc_get_part_info("2", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(0, fastboot_mmc_get_part_info("2.0", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(1, fastboot_mmc_get_part_info(":1", &fb_dev_desc,
						  &part_info, response));
	ut_asserteq(0, fastboot_mmc_get_part_info(":0", &fb_dev_desc,
						  &part_info, response));

	return 0;
}
DM_TEST(dm_test_fastboot_mmc_part, UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Run a fastboot command, leaving its response in @response */
static void fb_run(const char *cmd, char *response)
{
	char buf[FASTBOOT_COMMAND_LEN];

	strlcpy(buf, cmd, sizeof(buf));
	*response = '\0';
	fastboot_handle_command(buf, response);
}

/*
 * Write the frp partition the way Android's PersistentDataBlockService does:
 * the "OEM unlocking" setting in the last byte and a SHA-256 digest of the
 * partition, taken with the digest bytes as zero, in the first 32 bytes.
 */
static int fb_write_frp(struct unit_test_state *uts, struct blk_desc *desc,
			struct disk_partition *info, u8 oem_unlock,
			bool good_digest)
{
	size_t size = info->size * info->blksz;
	u8 digest[SHA256_SUM_LEN];
	u8 *buf = calloc(1, size);

	ut_assertnonnull(buf);
	buf[size - 1] = oem_unlock;
	sha256_csum_wd(buf, size, digest, CHUNKSZ_SHA256);
	memcpy(buf, digest, sizeof(digest));
	if (!good_digest)
		buf[0] ^= 0xff;
	ut_asserteq(info->size, blk_dwrite(desc, info->start, info->size, buf));
	free(buf);

	return 0;
}

/* Fill a partition with a pattern, or check that it was erased */
static int fb_part_fill(struct unit_test_state *uts, struct blk_desc *desc,
			struct disk_partition *info, bool check_erased)
{
	size_t size = info->size * info->blksz;
	u8 *buf = malloc(size);

	ut_assertnonnull(buf);
	if (check_erased) {
		ut_asserteq(info->size,
			    blk_dread(desc, info->start, info->size, buf));
		ut_assert(!buf[0] && !memcmp(buf, buf + 1, size - 1));
	} else {
		memset(buf, 0xa5, size);
		ut_asserteq(info->size,
			    blk_dwrite(desc, info->start, info->size, buf));
	}
	free(buf);

	return 0;
}

/*
 * The flashing commands guard the AVB lock state: a locked device must not
 * accept images that were not verified, unlocking needs the user's consent
 * from Android, and every change of the state wipes the user's data.
 */
static int dm_test_fastboot_flashing(struct unit_test_state *uts)
{
	char response[FASTBOOT_RESPONSE_LEN];
	char str_disk_guid[UUID_STR_LEN + 1];
	struct disk_partition frp, userdata, metadata;
	struct blk_desc *desc;
	struct disk_partition parts[] = {
		{ .start = 48, .size = 8, .name = "frp" },
		{ .start = 56, .size = 8, .name = "userdata" },
		{ .start = 64, .size = 8, .name = "metadata" },
	};
	int i;

	if (!IS_ENABLED(CONFIG_FASTBOOT_CMD_FLASHING))
		return -EAGAIN;

	ut_assertok(blk_get_device_by_str("mmc", "0", &desc));
	if (CONFIG_IS_ENABLED(RANDOM_UUID)) {
		for (i = 0; i < ARRAY_SIZE(parts); i++)
			gen_rand_uuid_str(parts[i].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(str_disk_guid, UUID_STR_FORMAT_STD);
	}
	ut_assertok(gpt_restore(desc, str_disk_guid, parts, ARRAY_SIZE(parts)));
	ut_assert(part_get_info_by_name(desc, "frp", &frp) > 0);
	ut_assert(part_get_info_by_name(desc, "userdata", &userdata) > 0);
	ut_assert(part_get_info_by_name(desc, "metadata", &metadata) > 0);

	/* The sandbox TA starts unlocked */
	fb_run("getvar:unlocked", response);
	ut_asserteq_str("OKAYyes", response);
	fb_run("flashing unlock", response);
	ut_asserteq_str("FAILdevice already unlocked", response);

	/* Locking wipes userdata and metadata */
	ut_assertok(fb_part_fill(uts, desc, &userdata, false));
	ut_assertok(fb_part_fill(uts, desc, &metadata, false));
	fb_run("flashing lock", response);
	ut_asserteq_str("OKAY", response);
	ut_assertok(fb_part_fill(uts, desc, &userdata, true));
	ut_assertok(fb_part_fill(uts, desc, &metadata, true));
	fb_run("getvar:unlocked", response);
	ut_asserteq_str("OKAYno", response);
	fb_run("flashing lock", response);
	ut_asserteq_str("FAILdevice already locked", response);

	/* A locked device refuses to change its partitions */
	fb_run("flash:userdata", response);
	ut_asserteq_str("FAILdevice is locked", response);
	fb_run("erase:userdata", response);
	ut_asserteq_str("FAILdevice is locked", response);

	/* Unlocking needs "OEM unlocking" in a valid frp partition */
	ut_assertok(fb_write_frp(uts, desc, &frp, 0, true));
	fb_run("flashing get_unlock_ability", response);
	ut_asserteq_str("OKAY0", response);
	fb_run("flashing unlock", response);
	ut_asserteq_str("FAILunlocking is not allowed, enable OEM unlocking in Android",
			response);

	ut_assertok(fb_write_frp(uts, desc, &frp, 1, false));
	fb_run("flashing get_unlock_ability", response);
	ut_asserteq_str("OKAY0", response);

	ut_assertok(fb_write_frp(uts, desc, &frp, 1, true));
	fb_run("flashing get_unlock_ability", response);
	ut_asserteq_str("OKAY1", response);

	/* Unlocking wipes userdata too */
	ut_assertok(fb_part_fill(uts, desc, &userdata, false));
	fb_run("flashing unlock", response);
	ut_asserteq_str("OKAY", response);
	ut_assertok(fb_part_fill(uts, desc, &userdata, true));
	fb_run("getvar:unlocked", response);
	ut_asserteq_str("OKAYyes", response);

	return 0;
}
DM_TEST(dm_test_fastboot_flashing, UTF_SCAN_PDATA | UTF_SCAN_FDT);
