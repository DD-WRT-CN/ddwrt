/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Copyright (C) 2015 Nikolay Martynov <mar.kolya@gmail.com>
 * Copyright (C) 2015 John Crispin <john@phrozen.org>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <dt-bindings/clock/mt7621-clk.h>

#include <asm/mipsregs.h>
#include <asm/smp-ops.h>
#include <asm/mips-cps.h>
#include <asm/mach-ralink/ralink_regs.h>
#include <asm/mach-ralink/mt7621.h>
#include <asm/mips-boards/launch.h>
#include <asm/delay.h>
#include <asm/time.h>

#include <pinmux.h>

#include "common.h"

#define MT7621_GPIO_MODE_UART1		1
#define MT7621_GPIO_MODE_I2C		2
#define MT7621_GPIO_MODE_UART3_MASK	0x3
#define MT7621_GPIO_MODE_UART3_SHIFT	3
#define MT7621_GPIO_MODE_UART3_GPIO	1
#define MT7621_GPIO_MODE_UART2_MASK	0x3
#define MT7621_GPIO_MODE_UART2_SHIFT	5
#define MT7621_GPIO_MODE_UART2_GPIO	1
#define MT7621_GPIO_MODE_JTAG		7
#define MT7621_GPIO_MODE_WDT_MASK	0x3
#define MT7621_GPIO_MODE_WDT_SHIFT	8
#define MT7621_GPIO_MODE_WDT_GPIO	1
#define MT7621_GPIO_MODE_PCIE_RST	0
#define MT7621_GPIO_MODE_PCIE_REF	2
#define MT7621_GPIO_MODE_PCIE_MASK	0x3
#define MT7621_GPIO_MODE_PCIE_SHIFT	10
#define MT7621_GPIO_MODE_PCIE_GPIO	1
#define MT7621_GPIO_MODE_MDIO_MASK	0x3
#define MT7621_GPIO_MODE_MDIO_SHIFT	12
#define MT7621_GPIO_MODE_MDIO_GPIO	1
#define MT7621_GPIO_MODE_RGMII1		14
#define MT7621_GPIO_MODE_RGMII2		15
#define MT7621_GPIO_MODE_SPI_MASK	0x3
#define MT7621_GPIO_MODE_SPI_SHIFT	16
#define MT7621_GPIO_MODE_SPI_GPIO	1
#define MT7621_GPIO_MODE_SDHCI_MASK	0x3
#define MT7621_GPIO_MODE_SDHCI_SHIFT	18
#define MT7621_GPIO_MODE_SDHCI_GPIO	1

static struct rt2880_pmx_func uart1_grp[] =  { FUNC("uart1", 0, 1, 2) };
static struct rt2880_pmx_func i2c_grp[] =  { FUNC("i2c", 0, 3, 2) };
static struct rt2880_pmx_func uart3_grp[] = {
	FUNC("uart3", 0, 5, 4),
	FUNC("i2s", 2, 5, 4),
	FUNC("spdif3", 3, 5, 4),
};
static struct rt2880_pmx_func uart2_grp[] = {
	FUNC("uart2", 0, 9, 4),
	FUNC("pcm", 2, 9, 4),
	FUNC("spdif2", 3, 9, 4),
};
static struct rt2880_pmx_func jtag_grp[] = { FUNC("jtag", 0, 13, 5) };
static struct rt2880_pmx_func wdt_grp[] = {
	FUNC("wdt rst", 0, 18, 1),
	FUNC("wdt refclk", 2, 18, 1),
};
static struct rt2880_pmx_func pcie_rst_grp[] = {
	FUNC("pcie rst", MT7621_GPIO_MODE_PCIE_RST, 19, 1),
	FUNC("pcie refclk", MT7621_GPIO_MODE_PCIE_REF, 19, 1)
};
static struct rt2880_pmx_func mdio_grp[] = { FUNC("mdio", 0, 20, 2) };
static struct rt2880_pmx_func rgmii2_grp[] = { FUNC("rgmii2", 0, 22, 12) };
static struct rt2880_pmx_func spi_grp[] = {
	FUNC("spi", 0, 34, 7),
	FUNC("nand1", 2, 34, 7),
};
static struct rt2880_pmx_func sdhci_grp[] = {
	FUNC("sdhci", 0, 41, 8),
	FUNC("nand2", 2, 41, 8),
};
static struct rt2880_pmx_func rgmii1_grp[] = { FUNC("rgmii1", 0, 49, 12) };

static struct rt2880_pmx_group mt7621_pinmux_data[] = {
	GRP("uart1", uart1_grp, 1, MT7621_GPIO_MODE_UART1),
	GRP("i2c", i2c_grp, 1, MT7621_GPIO_MODE_I2C),
	GRP_G("uart3", uart3_grp, MT7621_GPIO_MODE_UART3_MASK,
		MT7621_GPIO_MODE_UART3_GPIO, MT7621_GPIO_MODE_UART3_SHIFT),
	GRP_G("uart2", uart2_grp, MT7621_GPIO_MODE_UART2_MASK,
		MT7621_GPIO_MODE_UART2_GPIO, MT7621_GPIO_MODE_UART2_SHIFT),
	GRP("jtag", jtag_grp, 1, MT7621_GPIO_MODE_JTAG),
	GRP_G("wdt", wdt_grp, MT7621_GPIO_MODE_WDT_MASK,
		MT7621_GPIO_MODE_WDT_GPIO, MT7621_GPIO_MODE_WDT_SHIFT),
	GRP_G("pcie", pcie_rst_grp, MT7621_GPIO_MODE_PCIE_MASK,
		MT7621_GPIO_MODE_PCIE_GPIO, MT7621_GPIO_MODE_PCIE_SHIFT),
	GRP_G("mdio", mdio_grp, MT7621_GPIO_MODE_MDIO_MASK,
		MT7621_GPIO_MODE_MDIO_GPIO, MT7621_GPIO_MODE_MDIO_SHIFT),
	GRP("rgmii2", rgmii2_grp, 1, MT7621_GPIO_MODE_RGMII2),
	GRP_G("spi", spi_grp, MT7621_GPIO_MODE_SPI_MASK,
		MT7621_GPIO_MODE_SPI_GPIO, MT7621_GPIO_MODE_SPI_SHIFT),
	GRP_G("sdhci", sdhci_grp, MT7621_GPIO_MODE_SDHCI_MASK,
		MT7621_GPIO_MODE_SDHCI_GPIO, MT7621_GPIO_MODE_SDHCI_SHIFT),
	GRP("rgmii1", rgmii1_grp, 1, MT7621_GPIO_MODE_RGMII1),
	{ 0 }
};

static struct clk *clks[MT7621_CLK_MAX];
static struct clk_onecell_data clk_data = {
	.clks = clks,
	.clk_num = ARRAY_SIZE(clks),
};

phys_addr_t mips_cpc_default_phys_base(void)
{
	panic("Cannot detect cpc address");
}

static struct clk *__init mt7621_add_sys_clkdev(
	const char *id, unsigned long rate)
{
	struct clk *clk;
	int err;

	clk = clk_register_fixed_rate(NULL, id, NULL, 0, rate);
	if (IS_ERR(clk))
		panic("failed to allocate %s clock structure", id);

	err = clk_register_clkdev(clk, id, NULL);
	if (err)
		panic("unable to register %s clock device", id);

	return clk;
}

void __init ralink_clk_init(void)
{
	u32 syscfg, xtal_sel, clkcfg, clk_sel, curclk, ffiv, ffrac;
	u32 pll, prediv, fbdiv;
	u32 xtal_clk, cpu_clk, bus_clk;
	const static u32 prediv_tbl[] = {0, 1, 2, 2};

	syscfg = rt_sysc_r32(SYSC_REG_SYSTEM_CONFIG0);
	xtal_sel = (syscfg >> XTAL_MODE_SEL_SHIFT) & XTAL_MODE_SEL_MASK;

	clkcfg = rt_sysc_r32(SYSC_REG_CLKCFG0);
	clk_sel = (clkcfg >> CPU_CLK_SEL_SHIFT) & CPU_CLK_SEL_MASK;

	curclk = rt_sysc_r32(SYSC_REG_CUR_CLK_STS);
	ffiv = (curclk >> CUR_CPU_FDIV_SHIFT) & CUR_CPU_FDIV_MASK;
	ffrac = (curclk >> CUR_CPU_FFRAC_SHIFT) & CUR_CPU_FFRAC_MASK;

	if (xtal_sel <= 2)
		xtal_clk = 20 * 1000 * 1000;
	else if (xtal_sel <= 5)
		xtal_clk = 40 * 1000 * 1000;
	else
		xtal_clk = 25 * 1000 * 1000;

	switch (clk_sel) {
	case 0:
		cpu_clk = 500 * 1000 * 1000;
		break;
	case 1:
		pll = rt_memc_r32(MEMC_REG_CPU_PLL);
		fbdiv = (pll >> CPU_PLL_FBDIV_SHIFT) & CPU_PLL_FBDIV_MASK;
		prediv = (pll >> CPU_PLL_PREDIV_SHIFT) & CPU_PLL_PREDIV_MASK;
		cpu_clk = ((fbdiv + 1) * xtal_clk) >> prediv_tbl[prediv];
		break;
	default:
		cpu_clk = xtal_clk;
	}

	cpu_clk = cpu_clk / ffiv * ffrac;
	bus_clk = cpu_clk / 4;

	clks[MT7621_CLK_CPU] = mt7621_add_sys_clkdev("cpu", cpu_clk);
	clks[MT7621_CLK_BUS] = mt7621_add_sys_clkdev("bus", bus_clk);

	pr_info("CPU Clock: %dMHz\n", cpu_clk / 1000000);
	mips_hpt_frequency = cpu_clk / 2;
}

int getCPUClock(void)
{
    return clk_get_rate(clks[MT7621_CLK_CPU]) / 1000000;
}

u32 get_surfboard_sysclk(void) 
{
    return clk_get_rate(clks[MT7621_CLK_BUS]);
}
EXPORT_SYMBOL(get_surfboard_sysclk);

static void __init mt7621_clocks_init_dt(struct device_node *np)
{
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);
}

CLK_OF_DECLARE(ar7100, "mediatek,mt7621-pll", mt7621_clocks_init_dt);

void __init ralink_of_remap(void)
{
	rt_sysc_membase = plat_of_remap_node("mtk,mt7621-sysc");
	rt_memc_membase = plat_of_remap_node("mtk,mt7621-memc");

	if (!rt_sysc_membase || !rt_memc_membase)
		panic("Failed to remap core resources");
}

bool plat_cpu_core_present(int core)
{
	struct cpulaunch *launch = (struct cpulaunch *)CKSEG0ADDR(CPULAUNCH);

	if (!core)
		return true;
	launch += core * 2; /* 2 VPEs per core */
	if (!(launch->flags & LAUNCH_FREADY))
		return false;
	if (launch->flags & (LAUNCH_FGO | LAUNCH_FGONE))
		return false;
	return true;
}

#define LPS_PREC 8
/*
*  Re-calibration lpj(loop-per-jiffy).
*  (derived from kernel/calibrate.c)
*/
static int udelay_recal(void)
{
	unsigned int i, lpj = 0;
	unsigned long ticks, loopbit;
	int lps_precision = LPS_PREC;

	lpj = (1<<12);

	while ((lpj <<= 1) != 0) {
		/* wait for "start of" clock tick */
		ticks = jiffies;
		while (ticks == jiffies)
			/* nothing */;

		/* Go .. */
		ticks = jiffies;
		__delay(lpj);
		ticks = jiffies - ticks;
		if (ticks)
			break;
	}

	/*
	 * Do a binary approximation to get lpj set to
	 * equal one clock (up to lps_precision bits)
	 */
	lpj >>= 1;
	loopbit = lpj;
	while (lps_precision-- && (loopbit >>= 1)) {
		lpj |= loopbit;
		ticks = jiffies;
		while (ticks == jiffies)
			/* nothing */;
		ticks = jiffies;
		__delay(lpj);
		if (jiffies != ticks)   /* longer than 1 tick */
			lpj &= ~loopbit;
	}
	printk(KERN_INFO "%d CPUs re-calibrate udelay(lpj = %d)\n", NR_CPUS, lpj);

	for(i=0; i< NR_CPUS; i++)
		cpu_data[i].udelay_val = lpj;

	return 0;
}
device_initcall(udelay_recal);

void prom_soc_init(struct ralink_soc_info *soc_info)
{
	void __iomem *sysc = (void __iomem *) KSEG1ADDR(MT7621_SYSC_BASE);
	unsigned char *name = NULL;
	u32 n0;
	u32 n1;
	u32 rev;

	/* Early detection of CMP support */
	mips_cm_probe();
	mips_cpc_probe();

	if (mips_cps_numiocu(0)) {
		/*
		 * mips_cm_probe() wipes out bootloader
		 * config for CM regions and we have to configure them
		 * again. This SoC cannot talk to pamlbus devices
		 * witout proper iocu region set up.
		 *
		 * FIXME: it would be better to do this with values
		 * from DT, but we need this very early because
		 * without this we cannot talk to pretty much anything
		 * including serial.
		 */
		write_gcr_reg0_base(MT7621_PALMBUS_BASE);
		write_gcr_reg0_mask(~MT7621_PALMBUS_SIZE |
				    CM_GCR_REGn_MASK_CMTGT_IOCU0);
		__sync();
	}

	n0 = __raw_readl(sysc + SYSC_REG_CHIP_NAME0);
	n1 = __raw_readl(sysc + SYSC_REG_CHIP_NAME1);

	if (n0 == MT7621_CHIP_NAME0 && n1 == MT7621_CHIP_NAME1) {
		name = "MT7621";
		soc_info->compatible = "mtk,mt7621-soc";
	} else {
		panic("mt7621: unknown SoC, n0:%08x n1:%08x\n", n0, n1);
	}
	ralink_soc = MT762X_SOC_MT7621AT;
	rev = __raw_readl(sysc + SYSC_REG_CHIP_REV);

	snprintf(soc_info->sys_type, RAMIPS_SYS_TYPE_LEN,
		"MediaTek %s ver:%u eco:%u",
		name,
		(rev >> CHIP_REV_VER_SHIFT) & CHIP_REV_VER_MASK,
		(rev & CHIP_REV_ECO_MASK));

	soc_info->mem_size_min = MT7621_DDR2_SIZE_MIN;
	soc_info->mem_size_max = MT7621_DDR2_SIZE_MAX;
	soc_info->mem_base = MT7621_DRAM_BASE;

	rt2880_pinmux_data = mt7621_pinmux_data;


	if (!register_cmp_smp_ops())
		return;
	if (!register_cps_smp_ops())
		return;
	if (!register_vsmp_smp_ops())
		return;
}
