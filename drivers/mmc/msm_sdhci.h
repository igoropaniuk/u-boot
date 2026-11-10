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

/* DLL configuration */
#define CORE_DLL_EN		BIT(16)
#define CORE_CDR_EN		BIT(17)
#define CORE_CK_OUT_EN		BIT(18)
#define CORE_CDR_EXT_EN		BIT(19)
#define CORE_DLL_LOCK		BIT(7)
#define CORE_CMD_DAT_TRACK_SEL	BIT(0)

#define CDR_SELEXT_SHIFT	20
#define CDR_SELEXT_MASK		(0xf << CDR_SELEXT_SHIFT)

/* MCLK frequency selection (CMUX_SHIFT_PHASE) - required for the DLL to lock */
#define CMUX_SHIFT_PHASE_SHIFT	24
#define CMUX_SHIFT_PHASE_MASK	(7 << CMUX_SHIFT_PHASE_SHIFT)

/* 14LPP DLL reset / Tassadar DLL extras, for SDCC minor >= 0x42 / 0x71 */
#define CORE_DLL_CLOCK_DISABLE	BIT(21)

#define DLL_USR_CTL_POR_VAL	0x10800
#define ENABLE_DLL_LOCK_STATUS	BIT(26)
#define FINE_TUNE_MODE_EN	BIT(27)
#define BIAS_OK_SIGNAL		BIT(29)

#define DLL_CONFIG_3_LOW_FREQ_VAL	0x08
#define DLL_CONFIG_3_HIGH_FREQ_VAL	0x10

#define CORE_CLK_PWRSAVE	BIT(1)

/* Timing mode selection */
#define CORE_HC_MCLK_SEL_DFLT	(2 << 8)
#define CORE_HC_MCLK_SEL_HS400	(3 << 8)
#define CORE_HC_MCLK_SEL_MASK	(3 << 8)
#define CORE_HC_SELECT_IN_EN	BIT(18)
#define CORE_HC_SELECT_IN_HS400	(6 << 19)
#define CORE_HC_SELECT_IN_MASK	(7 << 19)

/* HS400 DDR/SDC4 DLL calibration, needed alongside the SDR CM_DLL above */
#define CORE_DDR_CAL_EN		BIT(0)
#define CORE_DDR_DLL_LOCK	BIT(11)
#define CORE_PWRSAVE_DLL	BIT(3)
#define CORE_CMDIN_RCLK_EN	BIT(1)
#define DDR_CONFIG_POR_VAL	0x80040873

#define CORE_FREQ_100MHZ	(100 * 1000000)
#define MAX_PHASES		16

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

	/* HS200 tuning state */
	bool tuning_done;
	bool calibration_done;
	u8 saved_tuning_phase;

	/* DLL init sequence variant selection, from the SDCC core version */
	u32 dll_config;		/* qcom,dll-config DT override, 0 if absent */
	bool use_14lpp_dll_reset;	/* core_minor >= 0x42 */
	bool uses_tassadar_dll;		/* core_minor >= 0x71 */

	/* HS400 support */
	bool use_cdclp533;		/* core_minor < 0x34, legacy calibration path */
	bool updated_ddr_cfg;		/* core_minor >= 0x49 */
	u32 ddr_config;			/* qcom,ddr-config DT override, else POR value */
};

struct msm_sdhc_variant_info {
	bool mci_removed;

	u32 core_dll_config;
	u32 core_dll_status;
	u32 core_dll_config_2;
	u32 core_dll_config_3;
	u32 core_dll_usr_ctl; /* Present on SDCC5.1 onwards */
	u32 core_vendor_spec;
	u32 core_vendor_spec_capabilities0;

	/* HS400 DDR/SDC4 calibration registers */
	u32 core_ddr_200_cfg;
	u32 core_vendor_spec3;
	u32 core_ddr_config_old; /* Applicable to sdcc minor ver < 0x49; 0 if N/A */
	u32 core_ddr_config;
};

#endif /* __MSM_SDHCI_H__ */
