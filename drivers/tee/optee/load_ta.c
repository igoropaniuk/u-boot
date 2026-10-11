// SPDX-License-Identifier: GPL-2.0+
/*
 * OP-TEE supplicant request: load a Trusted Application (TA)
 *
 * OP-TEE issues OPTEE_MSG_RPC_CMD_LOAD_TA when a session is opened to a TA
 * which is not embedded in the OP-TEE image. The request is made twice: a
 * first time without a buffer to learn the size of the TA binary, and a
 * second time with a shared memory buffer large enough to hold it.
 *
 * U-Boot only provides the bytes of the TA binary. OP-TEE verifies the
 * signature of the TA against its trusted key before loading it, so the
 * storage the TA is read from does not have to be trusted.
 */

#include <env.h>
#include <fs.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <tee.h>
#include <vsprintf.h>
#include <u-boot/uuid.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>

#include "optee_msg.h"
#include "optee_msg_supplicant.h"
#include "optee_private.h"

/**
 * typedef ta_source_fn - look up a TA in one kind of storage
 * @uuid_str:	UUID of the TA in standard textual form
 * @buf:	destination buffer, NULL if OP-TEE only asks for the size
 * @buf_size:	size of @buf in bytes
 * @ta_size:	returns the size of the TA binary when the TA is found
 *
 * When the TA is found and @buf is large enough to hold it, the TA binary
 * is copied to @buf.
 *
 * Return: 0 when the TA is found, -ENOENT when it is not and the next
 * source should be tried, another negative error code to stop searching
 */
typedef int (*ta_source_fn)(const char *uuid_str, void *buf, size_t buf_size,
			    size_t *ta_size);

/*
 * The FIT image is expected to be in memory already, at the address held by
 * the "optee_ta_fit" environment variable. Each TA is a sub-image whose
 * node name is the UUID of the TA.
 */
static int load_ta_from_fit(const char *uuid_str, void *buf, size_t buf_size,
			    size_t *ta_size)
{
	const char *addr_str;
	const void *data;
	const void *fit;
	size_t size;
	int noffset;
	int rc;

	if (!IS_ENABLED(CONFIG_OPTEE_LOAD_TA_FIT))
		return -ENOENT;

	addr_str = env_get("optee_ta_fit");
	if (!addr_str)
		return -ENOENT;

	fit = map_sysmem(hextoul(addr_str, NULL), 0);
	if (fit_check_format(fit, IMAGE_SIZE_INVAL)) {
		/* Report it, but let the other sources provide the TA */
		log_err("optee: no valid FIT image at optee_ta_fit=%s\n",
			addr_str);
		rc = -ENOENT;
		goto out;
	}

	noffset = fit_image_get_node(fit, uuid_str);
	if (noffset < 0) {
		rc = -ENOENT;
		goto out;
	}

	if (!fit_image_verify(fit, noffset)) {
		log_err("optee: TA %s failed FIT verification\n", uuid_str);
		rc = -EPERM;
		goto out;
	}

	rc = fit_image_get_data(fit, noffset, &data, &size);
	if (rc)
		goto out;

	*ta_size = size;
	if (buf && buf_size >= size)
		memcpy(buf, data, size);
	rc = 0;

out:
	unmap_sysmem(fit);
	return rc;
}

static const char *fs_setting(const char *env_name, const char *dflt)
{
	const char *val = env_get(env_name);

	return val ? val : dflt;
}

/*
 * TAs are "<uuid>.ta" files in a directory on a block device, the same
 * layout the Linux tee-supplicant uses under /lib/optee_armtz.
 */
static int load_ta_from_fs(const char *uuid_str, void *buf, size_t buf_size,
			   size_t *ta_size)
{
	const char *iface;
	const char *dev_part;
	const char *dir;
	loff_t actread;
	loff_t size;
	char path[256];
	int rc;

	if (!IS_ENABLED(CONFIG_OPTEE_LOAD_TA_FS))
		return -ENOENT;

	iface = fs_setting("optee_ta_fs_iface", CONFIG_OPTEE_LOAD_TA_FS_IFACE);
	dev_part = fs_setting("optee_ta_fs_dev_part",
			      CONFIG_OPTEE_LOAD_TA_FS_DEV_PART);
	dir = fs_setting("optee_ta_fs_dir", CONFIG_OPTEE_LOAD_TA_FS_DIR);

	rc = snprintf(path, sizeof(path), "%s/%s.ta", dir, uuid_str);
	if (rc < 0 || rc >= sizeof(path))
		return -ENAMETOOLONG;

	/*
	 * A missing device or filesystem is the normal state of a board that
	 * keeps its TAs elsewhere: treat it as "not found" so that the other
	 * sources are tried. Each fs_*() call closes the filesystem again, so
	 * it has to be selected before every call.
	 */
	if (fs_set_blk_dev(iface, dev_part, FS_TYPE_ANY)) {
		log_debug("optee: no filesystem on %s %s\n", iface, dev_part);
		return -ENOENT;
	}
	if (fs_size(path, &size))
		return -ENOENT;

	*ta_size = size;
	if (!buf || buf_size < size)
		return 0;

	if (fs_set_blk_dev(iface, dev_part, FS_TYPE_ANY))
		return -ENODEV;
	rc = fs_read(path, map_to_sysmem(buf), 0, size, &actread);
	if (rc || actread != size) {
		log_err("optee: failed to read TA %s from %s %s\n", path, iface,
			dev_part);
		return -EIO;
	}

	return 0;
}

/* Sources are tried in this order; the first one that finds the TA wins */
static const ta_source_fn ta_sources[] = {
	load_ta_from_fit,
	load_ta_from_fs,
};

void optee_suppl_cmd_load_ta(struct optee_msg_arg *arg)
{
	struct optee_msg_param *param;
	struct tee_shm *shm;
	u8 octets[TEE_UUID_LEN];
	char uuid_str[UUID_STR_LEN + 1];
	void *buf = NULL;
	size_t buf_size = 0;
	size_t ta_size = 0;
	int rc = -ENOENT;
	unsigned int i;

	if (arg->num_params != 2 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEE_ERROR_BAD_PARAMETERS;
		return;
	}

	/* OP-TEE stores the 16 UUID octets across value.a and value.b */
	memcpy(octets, &arg->params[0].u.value.a, sizeof(u64));
	memcpy(octets + sizeof(u64), &arg->params[0].u.value.b, sizeof(u64));
	uuid_bin_to_str(octets, uuid_str, UUID_STR_FORMAT_STD);

	/*
	 * The size query is a memory reference without a buffer, which OP-TEE
	 * encodes as a temporary memory parameter with a NULL pointer. The
	 * data request carries the shared memory this driver registered for
	 * OP-TEE (OPTEE_MSG_RPC_CMD_SHM_ALLOC), a registered memory parameter.
	 */
	param = &arg->params[1];
	if (param->attr == OPTEE_MSG_ATTR_TYPE_RMEM_OUTPUT) {
		shm = (struct tee_shm *)(ulong)param->u.rmem.shm_ref;
		if (!shm || param->u.rmem.offs > shm->size ||
		    param->u.rmem.size > shm->size - param->u.rmem.offs) {
			arg->ret = TEE_ERROR_BAD_PARAMETERS;
			return;
		}
		buf = (u8 *)shm->addr + param->u.rmem.offs;
		buf_size = param->u.rmem.size;
	} else if (param->attr != OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT ||
		   param->u.tmem.buf_ptr) {
		arg->ret = TEE_ERROR_BAD_PARAMETERS;
		return;
	}

	for (i = 0; i < ARRAY_SIZE(ta_sources); i++) {
		rc = ta_sources[i](uuid_str, buf, buf_size, &ta_size);
		if (rc != -ENOENT)
			break;
	}

	switch (rc) {
	case 0:
		/* rmem.size and tmem.size share the same location */
		param->u.rmem.size = ta_size;
		if (buf && ta_size > buf_size) {
			arg->ret = TEE_ERROR_SHORT_BUFFER;
		} else {
			log_debug("optee: TA %s, %zu bytes%s\n", uuid_str,
				  ta_size, buf ? " loaded" : "");
			arg->ret = TEE_SUCCESS;
		}
		break;
	case -ENOENT:
		log_err("optee: TA %s not found\n", uuid_str);
		arg->ret = TEE_ERROR_ITEM_NOT_FOUND;
		break;
	default:
		arg->ret = TEE_ERROR_GENERIC;
		break;
	}
}
