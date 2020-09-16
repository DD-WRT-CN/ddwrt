/* Copyright (c) 2010-2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <asm/mach/arch.h>

static const char * const qcom_dt_match[] __initconst = {
	"qcom,apq8064",
	"qcom,apq8074-dragonboard",
	"qcom,apq8084",
	"qcom,ipq8062",
	"qcom,ipq8064",
	"qcom,msm8660-surf",
	"qcom,msm8960-cdp",
	NULL
};

/* Watchdog bite time set to default reset value */
#define RESET_WDT_BITE_TIME 0x31F3

/* Watchdog bark time value is ketp larger than the watchdog timeout
 * of 0x31F3, effectively disabling the watchdog bark interrupt
 */
#define RESET_WDT_BARK_TIME (5 * RESET_WDT_BITE_TIME)

#define MSM_CLK_CTL_BASE    0x00900000
#define MSM_TMR_BASE        0x0200A000
#define APCS_WDT0_EN        (0x0040)
#define APCS_WDT0_RST       (0x0038)
#define APCS_WDT0_BARK_TIME (0x004C)
#define APCS_WDT0_BITE_TIME (0x005C)
#define APCS_WDT0_CPU0_WDOG_EXPIRED_ENABLE (0x3820)

static void qcom_restart(enum reboot_mode mode, const char *cmd)
{
	void __iomem		*tmrbase;
	void __iomem		*clkbase;
	printk(KERN_INFO "\nResetting with watch dog!\n");
	tmrbase = ioremap(MSM_TMR_BASE,0x1000);
	clkbase = ioremap(MSM_CLK_CTL_BASE,0x4000);
	writel(0, tmrbase + APCS_WDT0_EN);
	writel(1, tmrbase + APCS_WDT0_RST);
	writel(RESET_WDT_BARK_TIME, tmrbase + APCS_WDT0_BARK_TIME);
	writel(RESET_WDT_BITE_TIME, tmrbase + APCS_WDT0_BITE_TIME);
	writel(1, tmrbase + APCS_WDT0_EN);
	writel(1, clkbase + APCS_WDT0_CPU0_WDOG_EXPIRED_ENABLE);


}
DT_MACHINE_START(QCOM_DT, "Qualcomm (Flattened Device Tree)")
	.dt_compat = qcom_dt_match,
	.restart = qcom_restart,
MACHINE_END
