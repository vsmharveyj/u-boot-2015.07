/*
 * TQ Systems MBa28 starterkit for TQMa28L module
 * (C) Copyright 2015 TQ Systems GmbH
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
#include <asm/arch/iomux-mx28.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <linux/mii.h>
#include <miiphy.h>
#include <netdev.h>
#include <errno.h>
#include <mmc.h>
#include <malloc.h>

#include "tqma28l_bb.h"

DECLARE_GLOBAL_DATA_PTR;

static uint8_t tqma28_enet_clk_int = 0;

const char *tqma28l_bb_get_boardname(void)
{
	return "MBa28L";
}

/*
 * Functions
 */
#ifdef CONFIG_SPL_BUILD
const iomux_cfg_t tqma28l_bb_iomux_setup[] = {
#ifdef CONFIG_MXS_AUART
	/* AUART1 */
	MX28_PAD_AUART1_RX__AUART1_RX | MUX_CONFIG_PU,
	MX28_PAD_AUART1_TX__AUART1_TX | MUX_CONFIG_NOPU,
#endif

#ifdef CONFIG_TQMA28L_AA
	/* only TQMa28L-AA (i.MX287) */
	/* AUART3 */
	MX28_PAD_AUART3_RX__AUART3_RX | MUX_CONFIG_PU,
	MX28_PAD_AUART3_TX__AUART3_TX | MUX_CONFIG_NOPU,
	MX28_PAD_AUART3_CTS__AUART3_CTS | MUX_CONFIG_PU,
	MX28_PAD_AUART3_RTS__AUART3_RTS | MUX_CONFIG_NOPU,
#endif

	/* SD */
	MX28_PAD_SSP0_DATA0__SSP0_D0 | MUX_CONFIG_SD,
	MX28_PAD_SSP0_DATA1__SSP0_D1 | MUX_CONFIG_SD,
	MX28_PAD_SSP0_DATA2__SSP0_D2 | MUX_CONFIG_SD,
	MX28_PAD_SSP0_DATA3__SSP0_D3 | MUX_CONFIG_SD,
	MX28_PAD_SSP0_CMD__SSP0_CMD | MUX_CONFIG_SD,
	MX28_PAD_SSP0_DETECT__SSP0_CARD_DETECT | MUX_CONFIG_SD,
	MX28_PAD_SSP0_SCK__SSP0_SCK | MUX_CONFIG_PU,
	MX28_PAD_GPMI_RESETN__GPIO_0_28 | MUX_CONFIG_SD,

	/* FEC0 */
	MX28_PAD_ENET0_MDC__ENET0_MDC | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_MDIO__ENET0_MDIO | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_RX_EN__ENET0_RX_EN | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_TX_EN__ENET0_TX_EN | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_RXD0__ENET0_RXD0 | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_RXD1__ENET0_RXD1 | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_TXD0__ENET0_TXD0 | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_TXD1__ENET0_TXD1 | MUX_CONFIG_ENET,
	MX28_PAD_ENET_CLK__CLKCTRL_ENET | MUX_CONFIG_ENET,
	/* FEC0 Reset */
	MX28_PAD_GPMI_CE0N__GPIO_0_16 | MUX_CONFIG_PU,
	/* FEC INT */
	MX28_PAD_ENET0_TX_CLK__GPIO_4_5 | MUX_CONFIG_NOPU,

#ifdef CONFIG_TQMA28L_AA
	/* only TQMa28L-AA (i.MX287) */
	/* FEC1 */
	MX28_PAD_ENET0_COL__ENET1_TX_EN | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_CRS__ENET1_RX_EN | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_RXD2__ENET1_RXD0 | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_RXD3__ENET1_RXD1 | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_TXD2__ENET1_TXD0 | MUX_CONFIG_ENET,
	MX28_PAD_ENET0_TXD3__ENET1_TXD1 | MUX_CONFIG_ENET,
#endif
};

void tqma28l_bb_board_init_ll(const uint32_t arg, const uint32_t *resptr)
{
	mxs_iomux_setup_multiple_pads(tqma28l_bb_iomux_setup,
			   ARRAY_SIZE(tqma28l_bb_iomux_setup));
}
#endif

int tqma28l_bb_board_early_init_f(void)
{
#ifdef	CONFIG_CMD_USB
	mxs_iomux_setup_pad(MX28_PAD_SSP2_SS2__USB0_OVERCURRENT);
	mxs_iomux_setup_pad(MX28_PAD_GPMI_RDY0__GPIO_0_20 |
			MXS_PAD_4MA | MXS_PAD_3V3 | MXS_PAD_NOPULL);
	gpio_direction_output(MX28_PAD_GPMI_RDY0__GPIO_0_20, 0);

	mxs_iomux_setup_pad(MX28_PAD_SSP2_SS1__USB1_OVERCURRENT);
	mxs_iomux_setup_pad(MX28_PAD_SSP0_DATA4__GPIO_2_4 |
			MXS_PAD_4MA | MXS_PAD_3V3 | MXS_PAD_NOPULL);
	gpio_direction_output(MX28_PAD_SSP0_DATA4__GPIO_2_4, 0);
#endif

	return 0;
}

#ifdef CONFIG_CMD_USB
int board_ehci_hcd_init(int port)
{
	return (gpio_direction_output(MX28_PAD_GPMI_RDY0__GPIO_0_20, 1) |
		gpio_direction_output(MX28_PAD_SSP0_DATA4__GPIO_2_4, 1));
}

int board_ehci_hcd_exit(int port)
{
	return (gpio_direction_output(MX28_PAD_GPMI_RDY0__GPIO_0_20, 0) |
		gpio_direction_output(MX28_PAD_SSP0_DATA4__GPIO_2_4, 0));
}
#endif

#ifdef	CONFIG_CMD_MMC
static int tqma28l_sd_wp(int id)
{
	if (id != CONFIG_SD_INDEX) {
		printf("MXS MMC: Invalid card selected (card id = %d)\n", id);
		return 1;
	}

	return gpio_get_value(MX28_PAD_GPMI_RESETN__GPIO_0_28);
}

int tqma28l_bb_board_mmc_init(bd_t *bis)
{
	/* Configure SD WP as input */
	gpio_request(MX28_PAD_GPMI_RESETN__GPIO_0_28, "sd_wp");
	gpio_direction_input(MX28_PAD_GPMI_RESETN__GPIO_0_28);

	return mxsmmc_initialize(bis, CONFIG_SD_INDEX, tqma28l_sd_wp, NULL);
}
#endif

int board_eth_init(bd_t *bis)
{
	struct mxs_clkctrl_regs *clkctrl_regs =
		(struct mxs_clkctrl_regs *)MXS_CLKCTRL_BASE;
	struct eth_device *dev;
	int ret;

	/* Check env for var to enable enet clk output */
	tqma28_enet_clk_int = (1 == getenv_yesno("enet_clk_internal"));

	ret = cpu_eth_init(bis);

	/* MX28EVK uses ENET_CLK PAD to drive FEC clock */
	if (tqma28_enet_clk_int) {
		puts("[FEC MXS: Using internal ENET_CLK]\n");
		writel(CLKCTRL_ENET_TIME_SEL_RMII_CLK | CLKCTRL_ENET_CLK_OUT_EN,
				&clkctrl_regs->hw_clkctrl_enet);
	} else {
		puts("[FEC MXS: Using external ENET_CLK]\n");
		writel(CLKCTRL_ENET_TIME_SEL_RMII_CLK,
				&clkctrl_regs->hw_clkctrl_enet);
		clrbits_le32(&clkctrl_regs->hw_clkctrl_enet,
				CLKCTRL_ENET_CLK_OUT_EN);
	}

	/* Reset FEC PHYs */
	gpio_direction_output(MX28_PAD_GPMI_CE0N__GPIO_0_16, 1);
	udelay(50);
	gpio_set_value(MX28_PAD_GPMI_CE0N__GPIO_0_16, 0);
	udelay(200);
	gpio_set_value(MX28_PAD_GPMI_CE0N__GPIO_0_16, 1);

	ret = fecmxc_initialize_multi(bis, 0, 0, MXS_ENET0_BASE);
	if (ret) {
		puts("FEC MXS: Unable to init FEC0\n");
		return ret;
	}

	dev = eth_get_dev_by_name("FEC0");
	if (!dev) {
		puts("FEC MXS: Unable to get FEC0 device entry\n");
		return -EINVAL;
	}

#ifdef CONFIG_TQMA28L_AA
	/* only TQMa28L-AA (i.MX287) */
	ret = fecmxc_initialize_multi(bis, 1, 1, MXS_ENET1_BASE);
	if (ret) {
		puts("FEC MXS: Unable to init FEC1\n");
		return ret;
	}

	dev = eth_get_dev_by_name("FEC1");
	if (!dev) {
		puts("FEC MXS: Unable to get FEC1 device entry\n");
		return -EINVAL;
	}
#endif

	return ret;
}

/*
 * When user wants enet_clk driven by external source
 * tell the kernel to not enable the internal driver.
 *
 * By default the internal driver is enabled in mainline device tree
 * so we need to remove it in case we want external source.
 */
struct clkref {
	u32 ref;
	u32 id;
};
void tqma28_dt_remove_enet_out(void *blob)
{
	const char *names_prop = "clock-names";
	const char *names;
	const char *n;
	char *n_names;
	char *n_n;

	const char *clks_prop = "clocks";
	const u32 *clocks;
	const u32 *c;
	u32 *n_clocks;
	u32 *n_c;
	u8 clks_size = (2* sizeof(u32));

	int mac0_ofs;
	int clocks_len, names_len;
	int ret;
	int n_clocks_len = 0;
	int n_names_len = 0;
	int word = 0;

	mac0_ofs = fdt_path_offset(blob, "/ahb@80080000/ethernet@800f0000");

	clocks = fdt_getprop(blob, mac0_ofs, clks_prop, &clocks_len);
	n_clocks = calloc(1, clocks_len);

	names = fdt_getprop(blob, mac0_ofs, names_prop, &names_len);
	n_names = calloc(1, names_len);

	/*
	 * parse clock names for enet_out to remove
	 * by creating new properties without enet_out
	 */
	n = names;
	n_n = n_names;
	c = clocks;
	n_c = n_clocks;
	while (n < names + names_len)
	{
		/* string + null byte*/
		word = (strlen(n)+1);
		if (strcmp(n, "enet_out")) {
			/* not string we look for, keep it in n_names */
			memcpy(n_n, n, word);
			n_n += word;
			n_names_len += word;

			/*
			 * keep also the corresponding clocks entry
			 * consisting of two u32 values.
			 * TODO: use struct?
			 */
			memcpy(n_c, c, clks_size);
			n_c++; n_c++;
			n_clocks_len += clks_size;
		}
		n += word;
		c++; c++;
	}

	ret = fdt_setprop(blob, mac0_ofs, clks_prop, n_clocks, n_clocks_len);
	ret |= fdt_setprop(blob, mac0_ofs, names_prop, n_names, n_names_len);
	if (ret)
		printf("fdt_setprop(): %s\n", fdt_strerror(ret));
}

/*
 * Device Tree Setup
 */
#if defined(CONFIG_OF_BOARD_SETUP) && defined(CONFIG_OF_LIBFDT)
int tqma28l_bb_ft_board_setup(void *blob, bd_t *bd)
{
	if (!tqma28_enet_clk_int)
		tqma28_dt_remove_enet_out(blob);

	return 0;
}
#endif /* defined(CONFIG_OF_BOARD_SETUP) && defined(CONFIG_OF_LIBFDT) */
