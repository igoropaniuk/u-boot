/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Qualcomm SDHCI driver - SD/eMMC controller
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 */

#ifndef __MSM_SDHCI_H__
#define __MSM_SDHCI_H__

#include <sdhci.h>

/* Non-standard registers needed for SDHCI startup */
#define SDCC_MCI_POWER   0x0
#define SDCC_MCI_POWER_SW_RST BIT(7)

/* This is undocumented register */
#define SDCC_MCI_VERSION		0x50
#define SDCC_V5_VERSION			0x318

#define SDCC_VERSION_MAJOR_SHIFT	28
#define SDCC_VERSION_MAJOR_MASK		(0xf << SDCC_VERSION_MAJOR_SHIFT)
#define SDCC_VERSION_MINOR_MASK		0xff

#define SDCC_MCI_STATUS2 0x6C
#define SDCC_MCI_STATUS2_MCI_ACT 0x1
#define SDCC_MCI_HC_MODE 0x78

#define CORE_VENDOR_SPEC_POR_VAL 0xa9c

#define CORE_DLL_PDN		BIT(29)
#define CORE_DLL_RST		BIT(30)

#define MHZ(X) ((X) * 1000000UL)

struct msm_sdhc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct msm_sdhc {
	struct sdhci_host host;
	void *base;
	struct clk_bulk clks;
	struct udevice *vqmmc;
};

struct msm_sdhc_variant_info {
	bool mci_removed;

	u32 core_dll_config;
	u32 core_vendor_spec;
	u32 core_vendor_spec_capabilities0;
};

#endif /* __MSM_SDHCI_H__ */
