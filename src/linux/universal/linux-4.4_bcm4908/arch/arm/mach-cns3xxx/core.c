/*
 * Copyright 1999 - 2003 ARM Limited
 * Copyright 2000 Deep Blue Solutions Ltd
 * Copyright 2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/export.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_twd.h>
#include <linux/gpio.h>
#include <mach/cns3xxx.h>
#include "core.h"

#define IRQ_LOCALTIMER 29


static struct map_desc cns3xxx_io_desc[] __initdata = {
	{
		.virtual	= CNS3XXX_TC11MP_SCU_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_TC11MP_SCU_BASE),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_TIMER1_2_3_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_TIMER1_2_3_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_GPIOA_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_GPIOA_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_GPIOB_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_GPIOB_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_MISC_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_MISC_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PM_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PM_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
//	}, {
//		.virtual	= CNS3XXX_SWITCH_BASE_VIRT,
//		.pfn		= __phys_to_pfn(CNS3XXX_SWITCH_BASE),
//		.length		= SZ_4K,
//		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_SSP_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_SSP_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
//	}, {
//		.virtual	= CNS3XXX_L2C_BASE_VIRT,
//		.pfn		= __phys_to_pfn(CNS3XXX_L2C_BASE),
//		.length		= SZ_4K,
//		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE0_HOST_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE0_HOST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE0_CFG0_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE0_CFG0_BASE),
		.length		= SZ_64K, /* really 4 KiB at offset 32 KiB */
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE0_CFG1_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE0_CFG1_BASE),
		.length		= SZ_16M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE1_HOST_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE1_HOST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE1_CFG0_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE1_CFG0_BASE),
		.length		= SZ_64K, /* really 4 KiB at offset 32 KiB */
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE1_CFG1_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE1_CFG1_BASE),
		.length		= SZ_16M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE0_IO_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE0_IO_BASE),
		.length		= SZ_16M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= CNS3XXX_PCIE1_IO_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_PCIE1_IO_BASE),
		.length		= SZ_16M,
		.type		= MT_DEVICE,
	},
};



void __init cns3xxx_common_init(void)
{
	iotable_init(cns3xxx_io_desc, ARRAY_SIZE(cns3xxx_io_desc));
}
/* used by entry-macro.S */
void __init cns3xxx_init_irq(void)
{
	gic_init(0, 29, (void __iomem *) CNS3XXX_TC11MP_GIC_DIST_BASE_VIRT,
		 (void __iomem *) CNS3XXX_TC11MP_GIC_CPU_BASE_VIRT);
}

void cns3xxx_power_off(void)
{
	u32 __iomem *pm_base = (void __iomem *) CNS3XXX_PM_BASE_VIRT;
	u32 clkctrl;

	printk(KERN_INFO "powering system down...\n");

	clkctrl = readl(pm_base + PM_SYS_CLK_CTRL_OFFSET);
	clkctrl &= 0xfffff1ff;
	clkctrl |= (0x5 << 9);		/* Hibernate */
	writel(clkctrl, pm_base + PM_SYS_CLK_CTRL_OFFSET);

}

/*
 * Timer
 */
static void __iomem *cns3xxx_tmr1;

static int cns3xxx_shutdown(struct clock_event_device *clk)
{
	writel(0, cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);
	return 0;
}

static int cns3xxx_set_oneshot(struct clock_event_device *clk)
{
	unsigned long ctrl = readl(cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);

	/* period set, and timer enabled in 'next_event' hook */
	ctrl |= (1 << 2) | (1 << 9);
	writel(0, cns3xxx_tmr1 + TIMER1_AUTO_RELOAD_OFFSET);
	writel(ctrl, cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);
	return 0;
}

static int cns3xxx_set_periodic(struct clock_event_device *clk)
{
	unsigned long ctrl = readl(cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);
	int pclk = cns3xxx_cpu_clock() / 8;
	int reload;

	reload = pclk * 1000000 / HZ;
	writel(reload, cns3xxx_tmr1 + TIMER1_AUTO_RELOAD_OFFSET);
	ctrl |= (1 << 0) | (1 << 2) | (1 << 9);
	writel(ctrl, cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);
	return 0;
}

static int cns3xxx_timer_set_next_event(unsigned long evt,
					struct clock_event_device *unused)
{
	unsigned long ctrl = readl(cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);

	writel(evt, cns3xxx_tmr1 + TIMER1_AUTO_RELOAD_OFFSET);
	writel(ctrl | (1 << 0), cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);

	return 0;
}

static struct clock_event_device cns3xxx_tmr1_clockevent = {
	.name			= "cns3xxx timer1",
	.features		= CLOCK_EVT_FEAT_PERIODIC |
				  CLOCK_EVT_FEAT_ONESHOT,
	.set_state_shutdown	= cns3xxx_shutdown,
	.set_state_periodic	= cns3xxx_set_periodic,
	.set_state_oneshot	= cns3xxx_set_oneshot,
	.tick_resume		= cns3xxx_shutdown,
	.set_next_event		= cns3xxx_timer_set_next_event,
	.rating			= 300,
	.cpumask		= cpu_all_mask,
};

static void __init cns3xxx_clockevents_init(unsigned int timer_irq)
{
	cns3xxx_tmr1_clockevent.irq = timer_irq;
	clockevents_config_and_register(&cns3xxx_tmr1_clockevent,
					(cns3xxx_cpu_clock() >> 3) * 1000000,
					0xf, 0xffffffff);
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t cns3xxx_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &cns3xxx_tmr1_clockevent;
	u32 __iomem *stat = cns3xxx_tmr1 + TIMER1_2_INTERRUPT_STATUS_OFFSET;
	u32 val;

	/* Clear the interrupt */
	val = readl(stat);
	writel(val & ~(1 << 2), stat);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction cns3xxx_timer_irq = {
	.name		= "timer",
	.flags		= IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= cns3xxx_timer_interrupt,
};

static void __init cns3xxx_init_twd(void)
{
	static DEFINE_TWD_LOCAL_TIMER(cns3xx_twd_local_timer,
		CNS3XXX_TC11MP_TWD_BASE,
		IRQ_LOCALTIMER);

	twd_local_timer_register(&cns3xx_twd_local_timer);
}

static cycle_t cns3xxx_get_cycles(struct clocksource *cs)
{
  u64 val;

  val = readl(cns3xxx_tmr1 + TIMER_FREERUN_CONTROL_OFFSET);
  val &= 0xffff;

  return ((val << 32) | readl(cns3xxx_tmr1 + TIMER_FREERUN_OFFSET));
}

static struct clocksource clocksource_cns3xxx = {
	.name = "freerun",
	.rating = 200,
	.read = cns3xxx_get_cycles,
	.mask = CLOCKSOURCE_MASK(48),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init cns3xxx_clocksource_init(void)
{
	/* Reset the FreeRunning counter */
	writel((1 << 16), cns3xxx_tmr1 + TIMER_FREERUN_CONTROL_OFFSET);

	clocksource_register_khz(&clocksource_cns3xxx, 100);
}

/*
 * Set up the clock source and clock events devices
 */
static void __init __cns3xxx_timer_init(unsigned int timer_irq)
{
	u32 val;
	u32 irq_mask;

	/*
	 * Initialise to a known state (all timers off)
	 */

	/* disable timer1 and timer2 */
	writel(0, cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);
	/* stop free running timer3 */
	writel(0, cns3xxx_tmr1 + TIMER_FREERUN_CONTROL_OFFSET);

	writel(0, cns3xxx_tmr1 + TIMER1_MATCH_V1_OFFSET);
	writel(0, cns3xxx_tmr1 + TIMER1_MATCH_V2_OFFSET);

	val = (cns3xxx_cpu_clock() >> 3) * 1000000 / HZ;
	writel(val, cns3xxx_tmr1 + TIMER1_COUNTER_OFFSET);

	/* mask irq, non-mask timer1 overflow */
	irq_mask = readl(cns3xxx_tmr1 + TIMER1_2_INTERRUPT_MASK_OFFSET);
	irq_mask &= ~(1 << 2);
	irq_mask |= 0x03;
	writel(irq_mask, cns3xxx_tmr1 + TIMER1_2_INTERRUPT_MASK_OFFSET);

	/* down counter */
	val = readl(cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);
	val |= (1 << 9);
	writel(val, cns3xxx_tmr1 + TIMER1_2_CONTROL_OFFSET);

	setup_irq(timer_irq, &cns3xxx_timer_irq);

	cns3xxx_clocksource_init();
	cns3xxx_clockevents_init(timer_irq);
	cns3xxx_init_twd();
}

void __init cns3xxx_timer_init(void)
{
	cns3xxx_tmr1 = IOMEM(CNS3XXX_TIMER1_2_3_BASE_VIRT);

	__cns3xxx_timer_init(IRQ_CNS3XXX_TIMER0);
}

#ifdef CONFIG_CACHE_L2X0

static int cns3xxx_l2x0_enable = 1;

static int __init cns3xxx_l2x0_disable(char *s)
{
	cns3xxx_l2x0_enable = 0;
	return 1;
}
__setup("nol2x0", cns3xxx_l2x0_disable);

static int __init cns3xxx_l2x0_init(void)
{
	void __iomem *base;
	u32 val;

	if (!cns3xxx_l2x0_enable)
		return 0;
	printk(KERN_INFO "init l2x0\n");
	base = ioremap(CNS3XXX_L2C_BASE, SZ_4K);
	if (WARN_ON(!base))
		return 0;

	/*
	 * Tag RAM Control register
	 *
	 * bit[10:8]	- 1 cycle of write accesses latency
	 * bit[6:4]	- 1 cycle of read accesses latency
	 * bit[3:0]	- 1 cycle of setup latency
	 *
	 * 1 cycle of latency for setup, read and write accesses
	 */
	val = readl(base + L310_TAG_LATENCY_CTRL);
	val &= 0xfffff888;
	writel(val, base + L310_TAG_LATENCY_CTRL);

	/*
	 * Data RAM Control register
	 *
	 * bit[10:8]	- 1 cycles of write accesses latency
	 * bit[6:4]	- 1 cycles of read accesses latency
	 * bit[3:0]	- 1 cycle of setup latency
	 *
	 * 1 cycle of latency for setup, read and write accesses
	 */
	val = readl(base + L310_DATA_LATENCY_CTRL);
	val &= 0xfffff888;
	writel(val, base + L310_DATA_LATENCY_CTRL);

	/* 32 KiB, 8-way, parity disable */
	l2x0_init(base, 0x00500000, 0xfe0f0fff);

	return 0;
}
arch_initcall(cns3xxx_l2x0_init);

#endif /* CONFIG_CACHE_L2X0 */



