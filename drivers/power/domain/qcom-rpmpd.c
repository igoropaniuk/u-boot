// SPDX-License-Identifier: GPL-2.0-only
/*
 * Qualcomm RPM power domains (CX, MX and friends) voted over SMD RPM.
 *
 * Powering a domain on sends the enable vote only, as Linux does until a
 * consumer sets a performance state: the level the boot firmware voted
 * stays in place, and U-Boot neither raises it nor clobbers the sleep set
 * an always-on peer shares with the domain.
 *
 * Based on the Linux rpmpd driver:
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include <dm.h>
#include <power-domain-uclass.h>
#include <dm/device_compat.h>
#include <dt-bindings/power/qcom-rpmpd.h>
#include <linux/errno.h>
#include <soc/qcom/smd-rpm.h>

#define RPMPD_RWCX	0x78637772
#define RPMPD_RWMX	0x786d7772
#define RPMPD_RWLC	0x636c7772
#define RPMPD_RWLM	0x6d6c7772

#define KEY_ENABLE		0x6e657773 /* swen */

struct rpmpd_req {
	__le32 key;
	__le32 nbytes;
	__le32 value;
};

/*
 * The AO and VFL variants of a domain vote different keys or sets of the
 * same resource; for the enable vote they are all the same resource.
 */
struct rpmpd {
	u32 res_type;
	u32 res_id;
};

struct rpmpd_desc {
	const struct rpmpd *rpmpds;
	unsigned int num_pds;
};

struct rpmpd_priv {
	const struct rpmpd_desc *desc;
	unsigned long enabled;
};

#define RPMPD(_type, _id)		{ .res_type = _type, .res_id = _id }

static const struct rpmpd qcm2290_rpmpds[] = {
	[QCM2290_VDDCX] =	RPMPD(RPMPD_RWCX, 0),
	[QCM2290_VDDCX_AO] =	RPMPD(RPMPD_RWCX, 0),
	[QCM2290_VDDCX_VFL] =	RPMPD(RPMPD_RWCX, 0),
	[QCM2290_VDDMX] =	RPMPD(RPMPD_RWMX, 0),
	[QCM2290_VDDMX_AO] =	RPMPD(RPMPD_RWMX, 0),
	[QCM2290_VDDMX_VFL] =	RPMPD(RPMPD_RWMX, 0),
	[QCM2290_VDD_LPI_CX] =	RPMPD(RPMPD_RWLC, 0),
	[QCM2290_VDD_LPI_MX] =	RPMPD(RPMPD_RWLM, 0),
};

static const struct rpmpd_desc qcm2290_desc = {
	.rpmpds = qcm2290_rpmpds,
	.num_pds = ARRAY_SIZE(qcm2290_rpmpds),
};

static const struct rpmpd sm6125_rpmpds[] = {
	[RPMPD_VDDCX] =		RPMPD(RPMPD_RWCX, 0),
	[RPMPD_VDDCX_AO] =	RPMPD(RPMPD_RWCX, 0),
	[RPMPD_VDDCX_VFL] =	RPMPD(RPMPD_RWCX, 0),
	[RPMPD_VDDMX] =		RPMPD(RPMPD_RWMX, 0),
	[RPMPD_VDDMX_AO] =	RPMPD(RPMPD_RWMX, 0),
	[RPMPD_VDDMX_VFL] =	RPMPD(RPMPD_RWMX, 0),
};

static const struct rpmpd_desc shikra_desc = {
	.rpmpds = sm6125_rpmpds,
	.num_pds = ARRAY_SIZE(sm6125_rpmpds),
};

static int rpmpd_request(struct power_domain *power_domain)
{
	struct rpmpd_priv *priv = dev_get_priv(power_domain->dev);

	if (power_domain->id >= priv->desc->num_pds ||
	    !priv->desc->rpmpds[power_domain->id].res_type)
		return -EINVAL;

	return 0;
}

/* As Linux's rpmpd_power_on() with no performance state set */
static int rpmpd_on(struct power_domain *power_domain)
{
	struct udevice *dev = power_domain->dev;
	struct rpmpd_priv *priv = dev_get_priv(dev);
	const struct rpmpd *pd = &priv->desc->rpmpds[power_domain->id];
	struct rpmpd_req req = {
		.key = cpu_to_le32(KEY_ENABLE),
		.nbytes = cpu_to_le32(sizeof(u32)),
		.value = cpu_to_le32(1),
	};
	int ret;

	if (priv->enabled & BIT(power_domain->id))
		return 0;

	ret = qcom_rpm_smd_write(dev_get_parent(dev), QCOM_SMD_RPM_ACTIVE_STATE,
				 pd->res_type, pd->res_id, &req, sizeof(req));
	if (ret) {
		dev_err(dev, "failed to enable domain %lu: %d\n",
			power_domain->id, ret);
		return ret;
	}

	priv->enabled |= BIT(power_domain->id);

	return 0;
}

/*
 * Keep the vote, unlike Linux's rpmpd_power_off(): U-Boot switches its
 * devices off before the OS starts, and the RPM holds the last vote until
 * the kernel's driver sends its own. Dropping CX or MX in between would
 * take the buses down under the booting kernel.
 */
static int rpmpd_off(struct power_domain *power_domain)
{
	return 0;
}

static int rpmpd_probe(struct udevice *dev)
{
	struct rpmpd_priv *priv = dev_get_priv(dev);

	priv->desc = (const struct rpmpd_desc *)dev_get_driver_data(dev);

	/*
	 * Without a link to the RPM report "no driver", so that consumers
	 * carry on with the votes the boot firmware left.
	 */
	return qcom_rpm_smd_ready(dev_get_parent(dev)) ? -ENODEV : 0;
}

static const struct power_domain_ops rpmpd_ops = {
	.request = rpmpd_request,
	.on = rpmpd_on,
	.off = rpmpd_off,
};

static const struct udevice_id rpmpd_ids[] = {
	{ .compatible = "qcom,qcm2290-rpmpd", .data = (ulong)&qcm2290_desc },
	{ .compatible = "qcom,shikra-rpmpd", .data = (ulong)&shikra_desc },
	{ }
};

U_BOOT_DRIVER(qcom_rpmpd) = {
	.name = "qcom_rpmpd",
	.id = UCLASS_POWER_DOMAIN,
	.of_match = rpmpd_ids,
	.probe = rpmpd_probe,
	.priv_auto = sizeof(struct rpmpd_priv),
	.ops = &rpmpd_ops,
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
