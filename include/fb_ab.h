/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Android A/B slot helpers for fastboot
 */

#ifndef _FB_AB_H_
#define _FB_AB_H_

struct bootloader_control;

/**
 * fastboot_ab_get_control() - Read the A/B metadata of the fastboot device
 *
 * @abc: Where to store the bootloader_control from the misc partition
 * @response: Pointer to fastboot response buffer, filled on failure
 * Return: 0 on success, -1 on failure
 */
int fastboot_ab_get_control(struct bootloader_control *abc, char *response);

/**
 * fastboot_ab_merge_status() - Read the Virtual A/B merge status
 *
 * Return: MISC_VIRTUAL_AB_MERGE_STATUS_* (NONE if unknown)
 */
int fastboot_ab_merge_status(void);

/**
 * fastboot_ab_update_in_progress() - Refuse an operation during an update
 *
 * While a Virtual A/B update is snapshotted or merging, userdata and
 * metadata must not be erased and the slot must not change.
 *
 * @response: Pointer to fastboot response buffer, filled when refused
 * Return: true (and @response set to FAIL) if an update is in progress
 */
bool fastboot_ab_update_in_progress(char *response);

/**
 * fastboot_ab_set_active() - "set_active" command
 *
 * @slot: Slot name ("a", "b", "_a" or "_b")
 * @response: Pointer to fastboot response buffer
 */
void fastboot_ab_set_active(const char *slot, char *response);

/**
 * fastboot_ab_slot_index() - Parse a fastboot slot name
 *
 * @slot: "a", "b", "_a", "_b"...
 * Return: the slot number, or -1 if the name is invalid
 */
int fastboot_ab_slot_index(const char *slot);

#endif
