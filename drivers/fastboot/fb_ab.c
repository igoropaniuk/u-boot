// SPDX-License-Identifier: GPL-2.0+
/*
 * Android A/B slot handling for fastboot: the "set_active" command, the
 * slot-* variables and the Virtual A/B merge status, on the bootloader
 * control block in the misc partition of the fastboot MMC device.
 */

#include <android_ab.h>
#include <android_bootloader_message.h>
#include <fastboot.h>
#include <fb_ab.h>
#include <fb_mmc.h>
#include <part.h>
#include <linux/errno.h>

#define MISC_PART_NAME "misc"

static int get_misc(struct blk_desc **dev_desc, struct disk_partition *misc,
		    char *response)
{
	if (fastboot_mmc_get_part_info(MISC_PART_NAME, dev_desc, misc,
				       response) < 0)
		return -1;

	return 0;
}

int fastboot_ab_get_control(struct bootloader_control *abc, char *response)
{
	struct blk_desc *dev_desc;
	struct disk_partition misc;

	if (get_misc(&dev_desc, &misc, response))
		return -1;

	if (ab_get_control(dev_desc, &misc, abc) < 0) {
		fastboot_fail("invalid A/B metadata", response);
		return -1;
	}

	return 0;
}

int fastboot_ab_merge_status(void)
{
	struct blk_desc *dev_desc;
	struct disk_partition misc;
	char response[FASTBOOT_RESPONSE_LEN];
	int ret;

	if (get_misc(&dev_desc, &misc, response))
		return MISC_VIRTUAL_AB_MERGE_STATUS_NONE;

	ret = ab_get_merge_status(dev_desc, &misc, NULL);

	return ret < 0 ? MISC_VIRTUAL_AB_MERGE_STATUS_NONE : ret;
}

bool fastboot_ab_update_in_progress(char *response)
{
	switch (fastboot_ab_merge_status()) {
	case MISC_VIRTUAL_AB_MERGE_STATUS_SNAPSHOTTED:
		fastboot_fail("update snapshotted, not allowed", response);
		return true;
	case MISC_VIRTUAL_AB_MERGE_STATUS_MERGING:
		fastboot_fail("update merging, not allowed", response);
		return true;
	default:
		return false;
	}
}

int fastboot_ab_slot_index(const char *slot)
{
	if (!slot)
		return -1;
	if (slot[0] == '_')
		slot++;
	if (slot[0] < 'a' || slot[0] >= 'a' + NUM_SLOTS || slot[1])
		return -1;

	return slot[0] - 'a';
}

void fastboot_ab_set_active(const char *slot, char *response)
{
	struct blk_desc *dev_desc;
	struct disk_partition misc;
	int index, ret;

	index = fastboot_ab_slot_index(slot);
	if (index < 0) {
		fastboot_fail("invalid slot", response);
		return;
	}

	if (get_misc(&dev_desc, &misc, response))
		return;

	ret = ab_set_active_slot(dev_desc, &misc, index);
	if (ret == -EBUSY)
		fastboot_fail("update merging, not allowed", response);
	else if (ret < 0)
		fastboot_fail("cannot update the A/B metadata", response);
	else
		fastboot_okay(NULL, response);
}
