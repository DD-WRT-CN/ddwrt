/*
 *  $Id: irq.c 9263 2007-10-11 15:09:50Z juhosg $
 *
 *  ADM5120 specific interrupt handlers
 *
 *  Copyright (C) 2007 Gabor Juhos <juhosg at openwrt.org>
 *  Copyright (C) 2007 OpenWrt.org
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <linux/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/bitops.h>

#include <adm5120_defs.h>
#include <adm5120_irq.h>

#define INTC_WRITE(reg, val)	__raw_writel((val), \
	(void __iomem *)(KSEG1ADDR(ADM5120_INTC_BASE) + reg))

#define INTC_READ(reg)		__raw_readl(\
	(void __iomem *)(KSEG1ADDR(ADM5120_INTC_BASE) + reg))

static void adm5120_intc_irq_unmask(unsigned int irq);
static void adm5120_intc_irq_mask(unsigned int irq);
static int  adm5120_intc_irq_set_type(unsigned int irq, unsigned int flow_type);

static struct irq_chip adm5120_intc_irq_chip = {
	.name 		= "INTC",
	.unmask 	= adm5120_intc_irq_unmask,
	.mask 		= adm5120_intc_irq_mask,
	.mask_ack	= adm5120_intc_irq_mask,
	.set_type	= adm5120_intc_irq_set_type
};

static struct irqaction adm5120_intc_irq_action = {
	.handler 	= no_action,
	.name 		= "cascade [INTC]"
};

static void adm5120_intc_irq_unmask(unsigned int irq)
{
	irq -= ADM5120_INTC_IRQ_BASE;
	INTC_WRITE(INTC_REG_IRQ_ENABLE, 1 << irq);
}

static void adm5120_intc_irq_mask(unsigned int irq)
{
	irq -= ADM5120_INTC_IRQ_BASE;
	INTC_WRITE(INTC_REG_IRQ_DISABLE, 1 << irq);
}

static int adm5120_intc_irq_set_type(unsigned int irq, unsigned int flow_type)
{
	/* TODO: not yet tested */
#if 1
	unsigned int sense;
	unsigned long mode;
	int err = 0;

	sense = flow_type & (IRQ_TYPE_SENSE_MASK);
	switch (sense) {
	case IRQ_TYPE_NONE:
	case IRQ_TYPE_LEVEL_HIGH:
		break;
	case IRQ_TYPE_LEVEL_LOW:
		switch (irq) {
		case ADM5120_IRQ_GPIO2:
		case ADM5120_IRQ_GPIO4:
			break;
		default:
		printk(KERN_EMERG "error, bad irq %d\n",irq);
			err = -EINVAL;
			break;
		}
		break;
	default:
		printk(KERN_EMERG "error, bad sense %d\n",sense);
		err = -EINVAL;
		break;
	}

	if (err)
		return err;

	switch (irq) {
	case ADM5120_IRQ_GPIO2:
	case ADM5120_IRQ_GPIO4:
		mode = INTC_READ(INTC_REG_INT_MODE);
		if (sense == IRQ_TYPE_LEVEL_LOW)
			mode |= (1 << (irq-ADM5120_INTC_IRQ_BASE));
		else
			mode &= (1 << (irq-ADM5120_INTC_IRQ_BASE));

		INTC_WRITE(INTC_REG_INT_MODE, mode);
		/* fallthrogh */
	default:
		irq_desc[irq].status &= ~IRQ_TYPE_SENSE_MASK;
		irq_desc[irq].status |= sense;
		break;
	}
#endif
	return 0;
}

static void adm5120_intc_irq_dispatch(void)
{
	unsigned long status;
	int irq;

	/* dispatch only one IRQ at a time */
	status = INTC_READ(INTC_REG_IRQ_STATUS) & INTC_INT_ALL;

	if (status) {
		irq = ADM5120_INTC_IRQ_BASE+fls(status)-1;
		do_IRQ(irq);
	} else
		spurious_interrupt();
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned long pending;

	pending = read_c0_status() & read_c0_cause() & ST0_IM;

	if (pending & STATUSF_IP7)
		do_IRQ(ADM5120_IRQ_COUNTER);
	else if (pending & STATUSF_IP2)
		adm5120_intc_irq_dispatch();
	else
		spurious_interrupt();
}

#define INTC_IRQ_STATUS (IRQ_LEVEL | IRQ_TYPE_LEVEL_HIGH | IRQ_DISABLED)

static void __init adm5120_intc_irq_init(int base)
{
	int i;

	/* disable all interrupts */
	INTC_WRITE(INTC_REG_IRQ_DISABLE, INTC_INT_ALL);
	/* setup all interrupts to generate IRQ instead of FIQ */
	INTC_WRITE(INTC_REG_INT_MODE, 0);
	/* set active level for all external interrupts to HIGH */
	INTC_WRITE(INTC_REG_INT_LEVEL, 0);
	/* disable usage of the TEST_SOURCE register */
	INTC_WRITE(INTC_REG_IRQ_SOURCE_SELECT, 0);

	for (i = ADM5120_INTC_IRQ_BASE;
		i <= ADM5120_INTC_IRQ_BASE+INTC_IRQ_LAST;
		i++) {
		irq_desc[i].status = INTC_IRQ_STATUS;
		set_irq_chip_and_handler(i, &adm5120_intc_irq_chip,
			handle_level_irq);
	}

	setup_irq(ADM5120_IRQ_INTC, &adm5120_intc_irq_action);
}

void __init arch_init_irq(void) {
	mips_cpu_irq_init();
	adm5120_intc_irq_init(ADM5120_INTC_IRQ_BASE);
}
