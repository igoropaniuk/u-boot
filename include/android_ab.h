/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2017 The Android Open Source Project
 */

#ifndef __ANDROID_AB_H
#define __ANDROID_AB_H

#include <stdbool.h>

struct blk_desc;
struct bootloader_control;
struct disk_partition;

/* Android standard boot slot names are 'a', 'b', 'c', ... */
#define BOOT_SLOT_NAME(slot_num) ('a' + (slot_num))

/* Number of slots */
#define NUM_SLOTS 2

/**
 * ab_select_slot() - Select the slot where to boot from.
 *
 * @dev_desc: Place to store the device description pointer
 * @part_info: Place to store the partition information
 *
 * On Android devices with more than one boot slot (multiple copies of the
 * kernel and system images) selects which slot should be used to boot from and
 * registers the boot attempt. This is used in by the new A/B update model where
 * one slot is updated in the background while running from the other slot. If
 * the selected slot did not successfully boot in the past, a boot attempt is
 * registered before returning from this function so it isn't selected
 * indefinitely.
 *
 * Return: The slot number (>= 0) on success, or a negative on error
 */
int ab_select_slot(struct blk_desc *dev_desc, struct disk_partition *part_info,
                   bool dec_tries);

/**
 * ab_get_control() - Read and validate the bootloader control block.
 *
 * @dev_desc: Device description pointer
 * @part_info: Partition information (normally "misc")
 * @abc: Where to store the bootloader_control (sizeof(struct
 *	bootloader_control) bytes)
 *
 * Reads the A/B metadata without modifying it, for callers that only need
 * to look at it (slot flags, active slot). A bad CRC, magic or version is
 * an error, the metadata is not re-initialized.
 *
 * Return: 0 on success, or a negative on error
 */
int ab_get_control(struct blk_desc *dev_desc,
		   const struct disk_partition *part_info,
		   struct bootloader_control *abc);

/**
 * ab_get_merge_status() - Read the Virtual A/B merge status.
 *
 * @dev_desc: Device description pointer
 * @part_info: Partition information (normally "misc")
 * @source_slot: If not NULL, where to store the slot the status was written
 *	from
 *
 * Reads struct misc_virtual_ab_message from the system space of the misc
 * partition (written by the boot control HAL, IBootControl
 * setSnapshotMergeStatus()).
 *
 * Return: the merge status (MISC_VIRTUAL_AB_MERGE_STATUS_*), NONE when the
 *	message is absent or invalid, or a negative on I/O error
 */
int ab_get_merge_status(struct blk_desc *dev_desc,
			const struct disk_partition *part_info,
			int *source_slot);

/**
 * ab_set_active_slot() - Make a slot the one to boot next ("set_active").
 *
 * @dev_desc: Device description pointer
 * @part_info: Partition information (normally "misc")
 * @slot: Slot number to activate
 *
 * Gives @slot the highest priority with a fresh try count and lowers the
 * others, like the boot control HAL's setActiveBootSlot(). Refused while a
 * Virtual A/B merge is in progress, since the merging slot must keep
 * booting until the merge completes.
 *
 * Return: 0 on success, -EBUSY while merging, or a negative on error
 */
int ab_set_active_slot(struct blk_desc *dev_desc,
		       const struct disk_partition *part_info, int slot);

/**
 * ab_dump_abc() - Dump ABC information for specific partition.
 *
 * @dev_desc: Device description pointer
 * @part_info: Partition information
 *
 * Return: 0 on success, or a negative on error
 */
int ab_dump_abc(struct blk_desc *dev_desc, struct disk_partition *part_info);

#endif /* __ANDROID_AB_H */
