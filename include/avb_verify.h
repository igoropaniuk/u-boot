/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2018, Linaro Limited
 */

#ifndef	_AVB_VERIFY_H
#define _AVB_VERIFY_H

#include <../lib/libavb/libavb.h>
#include <mapmem.h>
#include <mmc.h>

#define AVB_MAX_ARGS			1024
#define VERITY_TABLE_OPT_RESTART	"restart_on_corruption"
#define VERITY_TABLE_OPT_LOGGING	"ignore_corruption"
#define ALLOWED_BUF_ALIGN		8

enum avb_boot_state {
	AVB_GREEN,
	AVB_YELLOW,
	AVB_ORANGE,
	AVB_RED,
};

#define AVB_MAX_PRELOADED	4

/*
 * Persistent value holding the user-provisioned root key ("custom key"):
 * a vbmeta signed by it verifies with the boot state yellow instead of
 * green. Provisioned with "fastboot flash avb_custom_key <key blob>" and
 * removed with "fastboot erase avb_custom_key"; the blob is the output of
 * "avbtool extract_public_key".
 */
#define AVB_CUSTOM_KEY_PVALUE	"avb_custom_key"
/* AvbRSAPublicKeyHeader + 2 * 8192-bit modulus */
#define AVB_CUSTOM_KEY_MAX_SIZE	(8 + 2 * 1024)

/**
 * struct avb_preloaded - a partition already loaded into memory by the caller
 *
 * libavb hashes the preloaded bytes instead of reading the partition again,
 * so what gets verified is exactly what the bootloader goes on to boot.
 *
 * @name: partition name including the slot suffix, e.g. "boot_a"
 * @addr: start of the buffer holding the partition data
 * @size: number of bytes loaded so far
 * @max_size: capacity of the buffer (at most the partition size)
 */
struct avb_preloaded {
	char name[32];
	void *addr;
	size_t size;
	size_t max_size;
};

struct AvbOpsData {
	struct AvbOps ops;
	int mmc_dev;
	enum avb_boot_state boot_state;
	/* the vbmeta key matched the custom key, not the root key */
	bool custom_key_used;
#ifdef CONFIG_OPTEE_TA_AVB
	struct udevice *tee;
	u32 session;
#endif
	struct avb_preloaded preloaded[AVB_MAX_PRELOADED];
	int num_preloaded;
};

struct mmc_part {
	int dev_num;
	struct mmc *mmc;
	struct blk_desc *mmc_blk;
	struct disk_partition info;
};

enum mmc_io_type {
	IO_READ,
	IO_WRITE
};

AvbOps *avb_ops_alloc(int boot_device);

/**
 * avb_ops_add_preloaded() - register a partition that is already in memory
 *
 * @ops: AvbOps from avb_ops_alloc()
 * @partition: partition name including the slot suffix, e.g. "boot_a"
 * @addr: buffer holding the first @size bytes of the partition
 * @size: bytes loaded in @addr
 * @max_size: capacity of @addr; a hash descriptor covering more than this
 *	fails verification instead of overflowing the buffer
 * Return: 0 on success, -ENOSPC when no slot is left, -EINVAL on a bad name
 */
int avb_ops_add_preloaded(AvbOps *ops, const char *partition, void *addr,
			  size_t size, size_t max_size);
#ifdef CONFIG_OPTEE_TA_AVB
AvbIOResult avb_write_lock_state(AvbOps *ops, bool locked);
#endif

/**
 * avb_custom_key_used() - Did the last verification trust the custom key?
 *
 * @ops: AvbOps from avb_ops_alloc()
 * Return: true if the vbmeta signing key was accepted because it matched the
 *	user-provisioned custom key (yellow boot state), false if it matched
 *	the root key (green)
 */
bool avb_custom_key_used(AvbOps *ops);

/**
 * avb_write_custom_key() - Provision or remove the custom root key
 *
 * @ops: AvbOps from avb_ops_alloc()
 * @key: public key blob (avbtool extract_public_key format), or NULL to
 *	remove the custom key
 * @size: size of @key in bytes, 0 to remove
 * Return: AVB_IO_RESULT_OK or an error
 */
AvbIOResult avb_write_custom_key(AvbOps *ops, const void *key, size_t size);

/**
 * avb_update_rollback_indexes() - Store the rollback indexes of a verified slot
 *
 * libavb never writes the rollback indexes: once the bootloader knows the
 * slot is good (it verified and, with A/B, booted successfully before), it
 * raises every stored index that is below the one in the slot's vbmeta
 * images, so that older images are refused from then on.
 *
 * @ops: AvbOps from avb_ops_alloc()
 * @data: result of a successful avb_slot_verify()
 * Return: AVB_IO_RESULT_OK, or the error of the first failed read or write
 */
AvbIOResult avb_update_rollback_indexes(AvbOps *ops,
					const AvbSlotVerifyData *data);
void avb_ops_free(AvbOps *ops);

/**
 * avb_read_root_key_digest() - get the SHA-256 digest of the trusted AVB
 * root public key
 *
 * This is the root of trust for AVB: validate_vbmeta_public_key() hashes the
 * key embedded in the (untrusted) vbmeta and compares it against the digest
 * returned here. The provider is selected via CONFIG_AVB_ROOT_KEY_* and, for
 * CONFIG_AVB_ROOT_KEY_BOARD, may be overridden by a SoC/board specific strong
 * definition (e.g. reading a hash fused into OTP).
 *
 * @ops: AvbOps, contains AVB ops handlers (needed by the TEE provider)
 * @digest: output buffer of AVB_SHA256_DIGEST_SIZE bytes
 *
 * @return:
 *      AVB_IO_RESULT_OK on success (@digest populated)
 *      any other AvbIOResult fails verification closed (key not trusted)
 */
AvbIOResult avb_read_root_key_digest(AvbOps *ops, uint8_t *digest);

char *avb_set_state(AvbOps *ops, enum avb_boot_state boot_state);
char *avb_set_enforce_verity(const char *cmdline);
char *avb_set_ignore_corruption(const char *cmdline);

char *append_cmd_line(char *cmdline_orig, char *cmdline_new);
const char *str_avb_io_error(AvbIOResult res);
const char *str_avb_slot_error(AvbSlotVerifyResult res);
/**
 * ============================================================================
 * I/O helper inline functions
 * ============================================================================
 */
static inline uint64_t calc_offset(struct mmc_part *part, int64_t offset)
{
	u64 part_size = part->info.size * part->info.blksz;

	if (offset < 0)
		return part_size + offset;

	return offset;
}

static inline size_t get_sector_buf_size(void)
{
	return (size_t)CONFIG_AVB_BUF_SIZE;
}

static inline void *get_sector_buf(void)
{
	return map_sysmem(CONFIG_AVB_BUF_ADDR, CONFIG_AVB_BUF_SIZE);
}

static inline bool is_buf_unaligned(void *buffer)
{
	return (bool)((uintptr_t)buffer % ALLOWED_BUF_ALIGN);
}

static inline int get_boot_device(AvbOps *ops)
{
	struct AvbOpsData *data;

	if (ops) {
		data = ops->user_data;
		if (data)
			return data->mmc_dev;
	}

	return -1;
}

#endif /* _AVB_VERIFY_H */
