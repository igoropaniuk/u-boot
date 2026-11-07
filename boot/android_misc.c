// SPDX-License-Identifier: GPL-2.0+
/*
 * Messages the Android OS leaves for the bootloader in the system space of
 * the misc partition. Each message is a 64-byte struct at a fixed offset of
 * struct misc_system_space_layout, which starts at
 * SYSTEM_SPACE_OFFSET_IN_MISC, with a magic and a version.
 */
#include <android_bootloader_message.h>
#include <android_misc.h>
#include <blk.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <part.h>
#include <linux/err.h>

/**
 * misc_system_space_io() - Read or write one message of the system space
 *
 * @dev_desc: Device holding the misc partition
 * @part_info: The misc partition
 * @offset: Offset of the message in struct misc_system_space_layout
 * @msg: Message buffer
 * @size: Message size
 * @write: true to write @msg, false to read into it
 * Return: 0 on success, negative errno on error
 */
static int misc_system_space_io(struct blk_desc *dev_desc,
				const struct disk_partition *part_info,
				ulong offset, void *msg, size_t size,
				bool write)
{
	ulong byte_off = SYSTEM_SPACE_OFFSET_IN_MISC + offset;
	ulong blk, blocks, in_blk, ret;
	void *buf;

	blk = byte_off / part_info->blksz;
	in_blk = byte_off % part_info->blksz;
	blocks = DIV_ROUND_UP(in_blk + size, part_info->blksz);
	if (blk + blocks > part_info->size)
		return -ENOSPC;

	buf = malloc_cache_aligned(blocks * part_info->blksz);
	if (!buf)
		return -ENOMEM;

	ret = blk_dread(dev_desc, part_info->start + blk, blocks, buf);
	if (IS_ERR_VALUE(ret) || ret != blocks) {
		free(buf);
		return -EIO;
	}

	if (write) {
		memcpy(buf + in_blk, msg, size);
		ret = blk_dwrite(dev_desc, part_info->start + blk, blocks, buf);
		if (IS_ERR_VALUE(ret) || ret != blocks) {
			free(buf);
			return -EIO;
		}
	} else {
		memcpy(msg, buf + in_blk, size);
	}
	free(buf);

	return 0;
}

static int read_memtag(struct blk_desc *dev_desc,
		       const struct disk_partition *part_info,
		       struct misc_memtag_message *msg)
{
	int ret;

	ret = misc_system_space_io(dev_desc, part_info,
				   offsetof(struct misc_system_space_layout,
					    memtag_message),
				   msg, sizeof(*msg), false);
	if (ret)
		return ret;
	if (msg->magic != MISC_MEMTAG_MAGIC_HEADER ||
	    msg->version > MISC_MEMTAG_MESSAGE_VERSION)
		return -ENOENT;

	return 0;
}

int android_misc_read_memtag(struct blk_desc *dev_desc,
			     const struct disk_partition *part_info,
			     u32 *modep)
{
	struct misc_memtag_message msg;
	int ret;

	ret = read_memtag(dev_desc, part_info, &msg);
	if (ret)
		return ret;
	*modep = msg.memtag_mode;

	return 0;
}

int android_misc_clear_memtag_once(struct blk_desc *dev_desc,
				   const struct disk_partition *part_info)
{
	struct misc_memtag_message msg;
	u32 once = MISC_MEMTAG_MODE_MEMTAG_ONCE |
		   MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE;
	int ret;

	ret = read_memtag(dev_desc, part_info, &msg);
	if (ret == -ENOENT)
		return 0;
	if (ret)
		return ret;
	if (!(msg.memtag_mode & once))
		return 0;

	msg.memtag_mode &= ~once;

	return misc_system_space_io(dev_desc, part_info,
				    offsetof(struct misc_system_space_layout,
					     memtag_message),
				    &msg, sizeof(msg), true);
}

int android_misc_read_kcmdline(struct blk_desc *dev_desc,
			       const struct disk_partition *part_info,
			       u64 *flagsp)
{
	struct misc_kcmdline_message msg;
	int ret;

	ret = misc_system_space_io(dev_desc, part_info,
				   offsetof(struct misc_system_space_layout,
					    kcmdline_message),
				   &msg, sizeof(msg), false);
	if (ret)
		return ret;
	if (msg.magic != MISC_KCMDLINE_MAGIC_HEADER ||
	    msg.version > MISC_KCMDLINE_MESSAGE_VERSION)
		return -ENOENT;
	*flagsp = msg.kcmdline_flags;

	return 0;
}
