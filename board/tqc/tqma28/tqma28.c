/*
 * TQ Systems TQMa28 module
 * (C) Copyright 2013-2015 TQ Systems GmbH
 * Author: Michael Krummsdorf <michael.krummsdorf@tq-group.com>
 *
 * Based on m28evk.c:
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <i2c.h>
#include <linux/ctype.h>
#include <linux/mii.h>
#include <miiphy.h>
#include <netdev.h>
#include <errno.h>
#include <mmc.h>

#include "tqma28_bb.h"
#include "../common/tqc_emmc.h"

DECLARE_GLOBAL_DATA_PTR;

static const uint16_t tqma28_emmc_dsr = 0x0100;

/*
 * Functions
 */
static const char *tqma28_get_boardname(void)
{
	return "TQMa28";
}

int checkboard(void)
{
	printf("Board: %s on a %s\n", tqma28_get_boardname(),
		tqma28_bb_get_boardname());
	return 0;
}

int board_early_init_f(void)
{
	/* IO0 clock at 480MHz */
	mxs_set_ioclk(MXC_IOCLK0, 480000);

	/* SSP0 clock at 96MHz */
	mxs_set_sspclk(MXC_SSPCLK0, 96000, 0);
	/* SSP1 clock at 96MHz */
	mxs_set_sspclk(MXC_SSPCLK1, 96000, 0);

	tqma28_bb_board_early_init_f();

	return 0;
}

int dram_init(void)
{
	return mxs_dram_init();
}

void mx28_adjust_mac(int dev_id, unsigned char *mac)
{
	uint32_t data;

	/*
	 * TQ Components Gmbh:      00:D0:93:xx:xx:xx (default)
	 */

	mac[0] = 0x00;
	mac[1] = 0xD0;

	if (dev_id == 1) { /* Let MAC1 be MAC0 + 1 by default */
		data = (mac[3] << 16) | (mac[4] << 8) | mac[5];
		data = data + 1;
		mac[3] = (data >> 16) & 0xFF;
		mac[4] = (data >> 8) & 0xFF;
		mac[5] = data & 0xFF;
	}
}

/*
 * static eeprom: EEPROM layout
 */
struct __attribute__ ((__packed__)) tqma28_eeprom_data {
	u8 hrcw_primary[0x20];
	u8 mac[6];		/* 0x20 ... 0x25 */
	u8 rsv1[10];
	u8 serial[8];		/* 0x30 ... 0x37 */
	u8 rsv2[8];
	u8 id[0x40];		/* 0x40 ... 0x7f */
};

static struct tqma28_eeprom_data tqma28_eeprom;
static int tqma28_eeprom_has_been_read;

int parse_eeprom_mac(struct tqma28_eeprom_data *eeprom, char *buf,
		     size_t len)
{
	u8 *p;
	int ret;

	/* MAC address */
	p = eeprom->mac;
	ret = snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
		       p[0], p[1], p[2], p[3], p[4], p[5]);
	if (ret < 0)
		return ret;
	if (ret >= len)
		return ret;

	return 0;
}

int parse_eeprom_serial(struct tqma28_eeprom_data *eeprom, char *buf,
			size_t len)
{
	unsigned i;

	if (len < (sizeof(eeprom->serial) + 1))
		return -1;

	for (i = 0; i < (sizeof(eeprom->serial)) &&
		isdigit(eeprom->serial[i]); i++)
		buf[i] = eeprom->serial[i];
	buf[i] = '\0';
	if (sizeof(eeprom->serial) != strlen(buf))
		return -1;

	return 0;
}

int parse_eeprom_id(struct tqma28_eeprom_data *eeprom, char *buf,
		    size_t len)
{
	unsigned i;

	if (len < (sizeof(eeprom->id) + 1))
		return -1;


	for (i = 0; i < sizeof(eeprom->id) &&
		isprint(eeprom->id[i]) && isascii(eeprom->id[i]); ++i)
		buf[i] = eeprom->id[i];
	buf[i] = '\0';

	return 0;
}

/*
 * show_eeprom - display the contents of the module EEPROM
 */
static void tqma28_show_eeprom(void)
{
	/* must hold largest field of eeprom data */
	char safe_string[0x41];

	if (tqma28_eeprom_has_been_read) {
		puts("Module EEPROM:\n");
		/* ID */
		parse_eeprom_id(&tqma28_eeprom, safe_string,
				ARRAY_SIZE(safe_string));
		if (0 == strncmp(safe_string, "TQM", 3)) {
			printf("  ID: %s\n", safe_string);
			setenv("boardtype", safe_string);
		} else {
			puts("  unknown hardware variant\n");
			setenv("boardtype", "unknown");
		}
		/* Serial number */
		if (0 == parse_eeprom_serial(&tqma28_eeprom, safe_string,
					     ARRAY_SIZE(safe_string))) {
			printf("  SN: %s\n", safe_string);
			setenv("serial#", safe_string);
		} else {
			puts("  unknown serial number\n");
			setenv("serial#", "???");
		}
		/* MAC address */
		if (0 == parse_eeprom_mac(&tqma28_eeprom, safe_string,
					  ARRAY_SIZE(safe_string)))
			printf("  MAC: %s\n", safe_string);
	}
}

/*
 * read_eeprom - read the given EEPROM into memory
 */
static int read_eeprom(unsigned int bus, unsigned int addr,
		      struct tqma28_eeprom_data *eeprom)
{
	int ret;

	/*
	 * ignore value of parameter bus
	 * select i2c bus with most devices
	 */
	i2c_set_bus_num(1);
	ret = i2c_read(addr, 0, CONFIG_SYS_I2C_EEPROM_ADDR_LEN,
			(uchar *)eeprom, sizeof(*eeprom));
	return ret;
}

int board_init(void)
{
	/* Adress of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	tqma28_eeprom_has_been_read = (read_eeprom(1, CONFIG_SYS_I2C_EEPROM_ADDR,
						  &tqma28_eeprom)) ? 0 : 1;

	return 0;
}

#ifdef	CONFIG_CMD_MMC
unsigned tqma28_get_env_dev(void)
{
	struct mxs_spl_data *data = (struct mxs_spl_data *)
		((CONFIG_SYS_TEXT_BASE - sizeof(struct mxs_spl_data)) & ~0xf);

	/*
	 * Handle bug in combination with Freescale workaround
	 * for ssp clock polarity issue:
	 * When boot mode not set to sd card, look for environment on mmmc.
	 * Booting spi/i2c devices thus loads emmc environment.
	 */
	if (mxs_boot_modes[data->boot_mode_idx].boot_pads == BOOT_MODE_SD1)
		return CONFIG_SD_INDEX;

	return CONFIG_MMC_INDEX;
}

static int tqma28_emmc_cd(int id)
{
	return 1;
}

int board_mmc_init(bd_t *bis)
{
	if (mxsmmc_initialize(bis, CONFIG_MMC_INDEX, NULL, tqma28_emmc_cd)) {
		printf("Error initializing e-MMC\n");
	} else {
		struct mmc *mmc = find_mmc_device(CONFIG_MMC_INDEX);
		if (mmc) {
			mmc->block_dev.removable = 0;
		}
	}

	return tqma28_bb_board_mmc_init(bis);
}

/* board-specific MMC card detection / modification */
void board_mmc_detect_card_type(struct mmc *mmc)
{
	struct mmc *emmc = find_mmc_device(CONFIG_MMC_INDEX);

	if (emmc != mmc)
		return;

	if (tqc_emmc_need_dsr(mmc) > 0)
		mmc_set_dsr(mmc, tqma28_emmc_dsr);
}
#endif

int tqma28_setup_rtc_clocksource(void)
{
#define HW_RTC_PERSISTENT0			(0x00000060)
#define BM_RTC_PERSISTENT0_XTAL32_FREQ		(0x00000040)
#define BM_RTC_PERSISTENT0_XTAL32KHZ_PWRUP	(0x00000020)
#define BM_RTC_PERSISTENT0_CLOCKSOURCE		(0x00000001)
	struct mxs_rtc_regs *rtc_regs = (struct mxs_rtc_regs *)MXS_RTC_BASE;
	uint32_t persistent0;

	persistent0 = readl(&rtc_regs->hw_rtc_persistent0);

	persistent0 |= BM_RTC_PERSISTENT0_XTAL32KHZ_PWRUP |
				BM_RTC_PERSISTENT0_CLOCKSOURCE;
	persistent0 &= ~BM_RTC_PERSISTENT0_XTAL32_FREQ;

	writel(persistent0, &rtc_regs->hw_rtc_persistent0);

	printf("RTC: 32KHz xtal (persistent0 0x%08X)\n", persistent0);

	return 0;
}

int misc_init_r(void)
{
	char *mmccmd[2];

	tqma28_show_eeprom();

	mmccmd[0] = "mmc dev 0";
	mmccmd[1] = "mmc dev 1";

	run_command_list(mmccmd[CONFIG_SYS_MMC_ENV_DEV], -1, 0);
	setenv("mmcdev", CONFIG_SYS_MMC_ENV_DEV?"1":"0");

	tqma28_setup_rtc_clocksource();

	return 0;
}

/*
 * Device Tree Setup
 */
#if defined(CONFIG_OF_BOARD_SETUP) && defined(CONFIG_OF_LIBFDT)
int ft_board_setup(void *blob, bd_t *bd)
{
	struct mmc *mmc = find_mmc_device(CONFIG_MMC_INDEX);

	fdt_shrink_to_minimum(blob);

	/* bring in eMMC dsr settings if needed */
	if (mmc && (!mmc_init(mmc))) {
		if (tqc_emmc_need_dsr(mmc) > 0)
			tqc_ft_fixup_emmc_dsr(blob,
					      "/apb@80000000/apbh@80000000/ssp@80010000",
					      tqma28_emmc_dsr);
	} else {
		puts("e-MMC: not present?\n");
	}

	tqma28_bb_ft_board_setup(blob, bd);

	return 0;
}
#endif /* defined(CONFIG_OF_BOARD_SETUP) && defined(CONFIG_OF_LIBFDT) */
