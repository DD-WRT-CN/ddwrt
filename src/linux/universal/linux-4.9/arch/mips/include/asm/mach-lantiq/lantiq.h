/*
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  Copyright (C) 2010 John Crispin <john@phrozen.org>
 */
#ifndef _LANTIQ_H__
#define _LANTIQ_H__

#include <linux/irq.h>
#include <linux/ioport.h>

/* generic reg access functions */
#define ltq_r32(reg)		__raw_readl(reg)
#define ltq_w32(val, reg)	__raw_writel(val, reg)
#define ltq_w32_mask(clear, set, reg)	\
	ltq_w32((ltq_r32(reg) & ~(clear)) | (set), reg)
#define ltq_r8(reg)		__raw_readb(reg)
#define ltq_w8(val, reg)	__raw_writeb(val, reg)

extern unsigned int ltq_get_cpu_ver(void);
extern unsigned int ltq_get_soc_type(void);

/* clock speeds */
#define CLOCK_60M	60000000
#define CLOCK_83M	83333333
#define CLOCK_100M	100000000
#define CLOCK_111M	111111111
#define CLOCK_133M	133333333
#define CLOCK_167M	166666667
#define CLOCK_200M	200000000
#define CLOCK_266M	266666666
#define CLOCK_333M	333333333
#define CLOCK_400M	400000000

/* spinlock all ebu i/o */
extern spinlock_t ebu_lock;

/* some irq helpers */
extern void ltq_disable_irq(struct irq_data *data);
extern void ltq_mask_and_ack_irq(struct irq_data *data);
extern void ltq_enable_irq(struct irq_data *data);
extern int ltq_eiu_get_irq(int exin);

/* clock handling */
extern int clk_activate(struct clk *clk);
extern void clk_deactivate(struct clk *clk);
extern struct clk *clk_get_cpu(void);
extern struct clk *clk_get_fpi(void);
extern struct clk *clk_get_io(void);
extern struct clk *clk_get_ppe(void);

/* find out what bootsource we have */
extern unsigned char ltq_boot_select(void);
/* find out what caused the last cpu reset */
extern int ltq_reset_cause(void);
/* find out the soc type */
extern int ltq_soc_type(void);

#define IOPORT_RESOURCE_START	0x10000000
#define IOPORT_RESOURCE_END	0xffffffff
#define IOMEM_RESOURCE_START	0x10000000
#define IOMEM_RESOURCE_END	0xffffffff

#endif
