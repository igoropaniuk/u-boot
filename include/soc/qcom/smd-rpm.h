/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Qualcomm SMD RPM interface, after Linux include/linux/soc/qcom/smd-rpm.h.
 */
#ifndef __QCOM_SMD_RPM_H__
#define __QCOM_SMD_RPM_H__

#include <linux/types.h>

struct udevice;

#define QCOM_SMD_RPM_ACTIVE_STATE	0
#define QCOM_SMD_RPM_SLEEP_STATE	1

/**
 * qcom_rpm_smd_write() - send a resource request to the RPM and wait for
 *			  its acknowledgment
 *
 * @rpm:	the rpm-requests device (the parent of the RPM consumers)
 * @state:	QCOM_SMD_RPM_ACTIVE_STATE or QCOM_SMD_RPM_SLEEP_STATE
 * @type:	resource type, e.g. "rwcx" as a little-endian u32
 * @id:		resource id
 * @buf:	key/length/value triplets
 * @count:	size of @buf in bytes
 *
 * Return: 0 on success, -ENXIO if the RPM does not know the resource, -EIO
 * if it rejects the request, another negative errno on failure.
 */
int qcom_rpm_smd_write(struct udevice *rpm, int state, u32 type, u32 id,
		       void *buf, size_t count);

/**
 * qcom_rpm_smd_ready() - bring the link to the RPM up if it is not yet
 *
 * @rpm:	the rpm-requests device
 *
 * Return: 0 when requests can be sent, a negative errno otherwise.
 */
int qcom_rpm_smd_ready(struct udevice *rpm);

#endif /* __QCOM_SMD_RPM_H__ */
