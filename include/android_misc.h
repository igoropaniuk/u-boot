/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Messages the Android OS leaves for the bootloader in the system space of
 * the misc partition (see android_bootloader_message.h).
 */

#ifndef __ANDROID_MISC_H
#define __ANDROID_MISC_H

#include <linux/types.h>

struct blk_desc;
struct disk_partition;

/**
 * android_misc_read_memtag() - Read the MTE mode requested by the OS
 *
 * @dev_desc: Device holding the misc partition
 * @part_info: The misc partition
 * @modep: Returns the memtag_mode flags (MISC_MEMTAG_MODE_*)
 * Return: 0 if a valid misc_memtag_message was found, -ENOENT if there is
 *	none, other negative errno on error
 */
int android_misc_read_memtag(struct blk_desc *dev_desc,
			     const struct disk_partition *part_info,
			     u32 *modep);

/**
 * android_misc_clear_memtag_once() - Clear the one-shot MTE mode bits
 *
 * Android expects the bootloader to clear MISC_MEMTAG_MODE_MEMTAG_ONCE and
 * MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE on every boot once it has acted on
 * them.
 *
 * @dev_desc: Device holding the misc partition
 * @part_info: The misc partition
 * Return: 0 on success (also when there is no valid message), negative
 *	errno on error
 */
int android_misc_clear_memtag_once(struct blk_desc *dev_desc,
				   const struct disk_partition *part_info);

/**
 * android_misc_read_kcmdline() - Read the kernel command line flags
 *
 * @dev_desc: Device holding the misc partition
 * @part_info: The misc partition
 * @flagsp: Returns the kcmdline_flags (MISC_KCMDLINE_*)
 * Return: 0 if a valid misc_kcmdline_message was found, -ENOENT if there is
 *	none, other negative errno on error
 */
int android_misc_read_kcmdline(struct blk_desc *dev_desc,
			       const struct disk_partition *part_info,
			       u64 *flagsp);

#endif /* __ANDROID_MISC_H */
