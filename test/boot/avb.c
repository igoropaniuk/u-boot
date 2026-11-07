// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the AVB operations used by the Android bootmeth
 */

#include <avb_verify.h>
#include <blk.h>
#include <dm.h>
#include <malloc.h>
#include <part.h>
#include <asm/global_data.h>
#include <dm/lists.h>
#include <dm/ofnode.h>
#include <test/ut.h>
#include "bootstd_common.h"

DECLARE_GLOBAL_DATA_PTR;

/* mmc12.img: boot (boot image v2 with a hash footer), system, vbmeta */
#define AVB_MMC_DEV	12

/* Verify boot with the given preloaded buffer, return the libavb result */
static AvbSlotVerifyResult avb_verify_preloaded(u8 *buf, size_t size,
						size_t max_size,
						bool *used_buf)
{
	static const char *const parts[] = { "boot", NULL };
	AvbSlotVerifyData *data = NULL;
	AvbSlotVerifyResult res;
	AvbOps *ops;

	ops = avb_ops_alloc(UCLASS_MMC, AVB_MMC_DEV);
	if (!ops)
		return AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
	if (avb_ops_add_preloaded(ops, "boot", buf, size, max_size)) {
		avb_ops_free(ops);
		return AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
	}

	res = avb_slot_verify(ops, parts, "", AVB_SLOT_VERIFY_FLAGS_NONE,
			      AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
			      &data);
	*used_buf = data && data->num_loaded_partitions == 1 &&
		    data->loaded_partitions[0].preloaded &&
		    data->loaded_partitions[0].data == buf;
	if (data)
		avb_slot_verify_data_free(data);
	avb_ops_free(ops);

	return res;
}

/*
 * The bootmeth hands libavb the buffers it boots from, so that the bytes it
 * verifies are the bytes it boots. Check that libavb hashes those buffers
 * rather than reading the partition again.
 */
static int avb_preloaded(struct unit_test_state *uts)
{
	struct disk_partition info;
	struct blk_desc *desc;
	bool used_buf;
	ofnode node;
	size_t size;
	u8 *buf;

	node = ofnode_find_subnode(oftree_root(oftree_default()), "mmc12");
	ut_assert(ofnode_valid(node));
	ut_assertok(lists_bind_fdt(gd->dm_root, node, NULL, NULL, false));
	desc = blk_get_devnum_by_uclass_id(UCLASS_MMC, AVB_MMC_DEV);
	ut_assertnonnull(desc);
	ut_assert(part_get_info_by_name(desc, "boot", &info) > 0);

	size = info.size * info.blksz;
	buf = malloc(size);
	ut_assertnonnull(buf);
	ut_asserteq(info.size, blk_dread(desc, info.start, info.size, buf));

	/* The preloaded buffer is what gets verified */
	ut_asserteq(AVB_SLOT_VERIFY_RESULT_OK,
		    avb_verify_preloaded(buf, size, size, &used_buf));
	ut_assert(used_buf);

	/* A change to the buffer fails although the partition is intact */
	buf[0] ^= 0xff;
	ut_asserteq(AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION,
		    avb_verify_preloaded(buf, size, size, &used_buf));

	/*
	 * The boot image header may announce fewer bytes than the hash
	 * descriptor covers: the rest is read into the same buffer...
	 */
	memset(buf, 0, size);
	ut_asserteq(AVB_SLOT_VERIFY_RESULT_OK,
		    avb_verify_preloaded(buf, 0, size, &used_buf));
	ut_assert(used_buf);

	/* ...but never past its end */
	ut_assert(avb_verify_preloaded(buf, 0, info.blksz, &used_buf) !=
		  AVB_SLOT_VERIFY_RESULT_OK);

	free(buf);

	return 0;
}
BOOTSTD_TEST(avb_preloaded, UTF_DM | UTF_SCAN_FDT);
