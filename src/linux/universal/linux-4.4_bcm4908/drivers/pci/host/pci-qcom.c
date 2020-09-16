/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

/*
 * QCOM MSM PCIe controller driver.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/delay.h>

/* Root Complex Port vendor/device IDs */
#define PCIE_VENDOR_ID_RCP		0x17cb
#define PCIE_DEVICE_ID_RCP		0x0101

#define __set(v, a, b)	(((v) << (b)) & GENMASK(a, b))

#define PCIE20_PARF_PCS_DEEMPH		0x34
#define PCIE20_PARF_PCS_DEEMPH_TX_DEEMPH_GEN1(x)	__set(x, 21, 16)
#define PCIE20_PARF_PCS_DEEMPH_TX_DEEMPH_GEN2_3_5DB(x)	__set(x, 13, 8)
#define PCIE20_PARF_PCS_DEEMPH_TX_DEEMPH_GEN2_6DB(x)	__set(x, 5, 0)

#define PCIE20_PARF_PCS_SWING		0x38
#define PCIE20_PARF_PCS_SWING_TX_SWING_FULL(x)		__set(x, 14, 8)
#define PCIE20_PARF_PCS_SWING_TX_SWING_LOW(x)		__set(x, 6, 0)

#define PCIE20_PARF_PHY_CTRL		0x40
#define PCIE20_PARF_PHY_CTRL_PHY_TX0_TERM_OFFST(x)	__set(x, 20, 16)
#define PCIE20_PARF_PHY_CTRL_PHY_LOS_LEVEL(x)		__set(x, 12, 8)
#define PCIE20_PARF_PHY_CTRL_PHY_RTUNE_REQ		(1 << 4)
#define PCIE20_PARF_PHY_CTRL_PHY_TEST_BURNIN		(1 << 2)
#define PCIE20_PARF_PHY_CTRL_PHY_TEST_BYPASS		(1 << 1)
#define PCIE20_PARF_PHY_CTRL_PHY_TEST_PWR_DOWN		(1 << 0)

#define PCIE20_PARF_PHY_REFCLK		0x4C
#define PCIE20_PARF_CONFIG_BITS		0x50

#define PCIE20_ELBI_SYS_CTRL		0x04
#define PCIE20_ELBI_SYS_CTRL_LTSSM_EN	0x01

#define PCIE20_CAP			0x70
#define PCIE20_CAP_LINKCTRLSTATUS	(PCIE20_CAP + 0x10)

#define PCIE20_COMMAND_STATUS		0x04
#define PCIE20_BUSNUMBERS		0x18
#define PCIE20_MEMORY_BASE_LIMIT	0x20

#define PCIE20_AXI_MSTR_RESP_COMP_CTRL0 0x818
#define PCIE20_AXI_MSTR_RESP_COMP_CTRL1 0x81c
#define PCIE20_PLR_IATU_VIEWPORT	0x900
#define PCIE20_PLR_IATU_CTRL1		0x904
#define PCIE20_PLR_IATU_CTRL2		0x908
#define PCIE20_PLR_IATU_LBAR		0x90C
#define PCIE20_PLR_IATU_UBAR		0x910
#define PCIE20_PLR_IATU_LAR		0x914
#define PCIE20_PLR_IATU_LTAR		0x918
#define PCIE20_PLR_IATU_UTAR		0x91c
#define PCIE20_LNK_CAS2		0xA0

#define MSM_PCIE_DEV_CFG_ADDR		0x01000000

#define RD 0
#define WR 1

#define MAX_RC_NUM	3
#define PCIE_BUS_PRIV_DATA(pdev) \
	(((struct pci_sys_data *)pdev->bus->sysdata)->private_data)

/* PCIe TLP types that we are interested in */
#define PCI_CFG0_RDWR	0x4
#define PCI_CFG1_RDWR	0x5

#define readl_poll_timeout(addr, val, cond, sleep_us, timeout_us) \
({ \
	unsigned long timeout = jiffies + usecs_to_jiffies(timeout_us); \
	might_sleep_if(timeout_us); \
	for (;;) { \
		(val) = readl(addr); \
		if (cond) \
			break; \
		if (timeout_us && time_after(jiffies, timeout)) { \
			(val) = readl(addr); \
			break; \
		} \
		if (sleep_us) \
			usleep_range(DIV_ROUND_UP(sleep_us, 4), sleep_us); \
	} \
	(cond) ? 0 : -ETIMEDOUT; \
})

struct qcom_pcie {
	void __iomem		*elbi_base;
	void __iomem		*parf_base;
	void __iomem		*dwc_base;
	void __iomem		*cfg_base;
	struct device		*dev;
	int			reset_gpio;
	struct clk		*iface_clk;
	struct clk		*bus_clk;
	struct clk		*phy_clk;
	int			irq_int[4];
	int			root_bus_nr;
	struct reset_control	*axi_reset;
	struct reset_control	*ahb_reset;
	struct reset_control	*por_reset;
	struct reset_control	*pci_reset;
	struct reset_control	*phy_reset;

	struct resource		conf;
	struct resource		io;
	struct resource		mem;
};

static int nr_controllers;
static DEFINE_SPINLOCK(qcom_hw_pci_lock);

static inline struct qcom_pcie *sys_to_pcie(struct pci_sys_data *sys)
{
	return sys->private_data;
}

inline int is_msm_pcie_rc(struct pci_bus *bus)
{
	struct qcom_pcie *dev = sys_to_pcie(bus->sysdata);

	return (bus->number == dev->root_bus_nr);
}

static int qcom_pcie_is_link_up(struct qcom_pcie *dev)
{
	return readl_relaxed(dev->dwc_base + PCIE20_CAP_LINKCTRLSTATUS) &
			     BIT(29);
}

inline int msm_pcie_get_cfgtype(struct pci_bus *bus)
{
	struct qcom_pcie *dev = sys_to_pcie(bus->sysdata);
	/*
	 * http://www.tldp.org/LDP/tlk/dd/pci.html
	 * Pass it onto the secondary bus interface unchanged if the
	 * bus number specified is greater than the secondary bus
	 * number and less than or equal to the subordinate bus
	 * number.
	 *
	 * Read/Write to the RC and Device/Switch connected to the RC
	 * are CFG0 type transactions. Rest have to be forwarded
	 * down stream as CFG1 transactions.
	 *
	 */
	if (bus->parent && (bus->parent->number == dev->root_bus_nr))
		return PCI_CFG0_RDWR;
	if (bus->number == 0)
		return PCI_CFG0_RDWR;

	return PCI_CFG1_RDWR;
}

void msm_pcie_config_cfgtype(struct pci_bus *bus, u32 devfn)
{
	uint32_t bdf, cfgtype;
	struct qcom_pcie *dev = sys_to_pcie(bus->sysdata);

	cfgtype = msm_pcie_get_cfgtype(bus);

	if (cfgtype == PCI_CFG0_RDWR) {
		bdf = MSM_PCIE_DEV_CFG_ADDR;
	} else {
		/*
		 * iATU Lower Target Address Register
		 *	Bits	Description
		 *	*-1:0	Forms bits [*:0] of the
		 *		start address of the new
		 *		address of the translated
		 *		region. The start address
		 *		must be aligned to a
		 *		CX_ATU_MIN_REGION_SIZE kB
		 *		boundary, so these bits are
		 *		always 0. A write to this
		 *		location is ignored by the
		 *		PCIe core.
		 *	31:*1	Forms bits [31:*] of the of
		 *		the new address of the
		 *		translated region.
		 *
		 *	* is log2(CX_ATU_MIN_REGION_SIZE)
		 */
		bdf = (((bus->number & 0xff) << 24) & 0xff000000) |
				(((devfn & 0xff) << 16) & 0x00ff0000);
	}

	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_VIEWPORT);
	wmb();

	/* Program Bdf Address */
	writel_relaxed(bdf, dev->dwc_base + PCIE20_PLR_IATU_LTAR);
	wmb();

	/* Write Config Request Type */
	writel_relaxed(cfgtype, dev->dwc_base + PCIE20_PLR_IATU_CTRL1);
	wmb();
}

static inline int msm_pcie_oper_conf(struct pci_bus *bus, u32 devfn, int oper,
				     int where, int size, u32 *val)
{
	uint32_t word_offset, byte_offset, mask;
	uint32_t rd_val, wr_val;
	struct qcom_pcie *dev = sys_to_pcie(bus->sysdata);
	void __iomem *config_base;
	int rc;

	rc = is_msm_pcie_rc(bus);

	/*
	 * For downstream bus, make sure link is up
	 */
	if (rc && (devfn != 0)) {
		*val = ~0;
		return PCIBIOS_DEVICE_NOT_FOUND;
	} else if ((!rc) && (!qcom_pcie_is_link_up(dev))) {
		*val = ~0;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	msm_pcie_config_cfgtype(bus, devfn);

	word_offset = where & ~0x3;
	byte_offset = where & 0x3;
	mask = (~0 >> (8 * (4 - size))) << (8 * byte_offset);

	config_base = (rc) ? dev->dwc_base : dev->cfg_base;
	rd_val = readl_relaxed(config_base + word_offset);

	if (oper == RD) {
		*val = ((rd_val & mask) >> (8 * byte_offset));
	} else {
		wr_val = (rd_val & ~mask) |
				((*val << (8 * byte_offset)) & mask);
		writel_relaxed(wr_val, config_base + word_offset);
		wmb(); /* ensure config data is written to hardware register */
	}

	return 0;
}

static int msm_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			    int size, u32 *val)
{
	return msm_pcie_oper_conf(bus, devfn, RD, where, size, val);
}

static int msm_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			    int where, int size, u32 val)
{
	/*
	 *Attempt to reset secondary bus is causing PCIE core to reset.
	 *Disable secondary bus reset functionality.
	 */
	if ((bus->number == 0) && (where == PCI_BRIDGE_CONTROL) &&
	    (val & PCI_BRIDGE_CTL_BUS_RESET)) {
		pr_info("PCIE secondary bus reset not supported\n");
		val &= ~PCI_BRIDGE_CTL_BUS_RESET;
	}

	return msm_pcie_oper_conf(bus, devfn, WR, where, size, &val);
}

static struct pci_ops qcom_pcie_ops = {
	.read = msm_pcie_rd_conf,
	.write = msm_pcie_wr_conf,
};

static int qcom_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct qcom_pcie *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	return pcie_dev->irq_int[pin-1];
}

static struct pci_bus *qcom_pcie_scan_bus(int nr, struct pci_sys_data *sys)
{
	struct pci_bus *bus;
	struct qcom_pcie *qcom_pcie = sys->private_data;

	if (qcom_pcie) {
		qcom_pcie->root_bus_nr = sys->busnr;
		bus = pci_scan_root_bus(qcom_pcie->dev, sys->busnr, &qcom_pcie_ops,
					sys, &sys->resources);
	} else {
		bus = NULL;
		BUG();
	}

	return bus;
}

static int qcom_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct qcom_pcie *qcom_pcie = sys->private_data;

	/*
	 * specify linux PCI framework to allocate device memory (BARs)
	 * from msm_pcie_dev.dev_mem_res resource.
	 */
	sys->mem_offset = 0;
	sys->io_offset = 0;

	pci_add_resource(&sys->resources, &qcom_pcie->mem);
	pci_add_resource(&sys->resources, &qcom_pcie->io);

	return 1;
}

static struct hw_pci qcom_hw_pci[MAX_RC_NUM] = {
	{
#ifdef CONFIG_PCI_DOMAINS
		.domain = 0,
#endif
		.ops		= &qcom_pcie_ops,
		.nr_controllers	= 1,
		.swizzle	= pci_common_swizzle,
		.setup		= qcom_pcie_setup,
		.scan		= qcom_pcie_scan_bus,
		.map_irq	= qcom_pcie_map_irq,
	},
	{
#ifdef CONFIG_PCI_DOMAINS
		.domain = 1,
#endif
		.ops		= &qcom_pcie_ops,
		.nr_controllers	= 1,
		.swizzle	= pci_common_swizzle,
		.setup		= qcom_pcie_setup,
		.scan		= qcom_pcie_scan_bus,
		.map_irq	= qcom_pcie_map_irq,
	},
	{
#ifdef CONFIG_PCI_DOMAINS
		.domain = 2,
#endif
		.ops		= &qcom_pcie_ops,
		.nr_controllers	= 1,
		.swizzle	= pci_common_swizzle,
		.setup		= qcom_pcie_setup,
		.scan		= qcom_pcie_scan_bus,
		.map_irq	= qcom_pcie_map_irq,
	},
};

static inline void qcom_elbi_writel_relaxed(struct qcom_pcie *pcie,
					    u32 val, u32 reg)
{
	writel_relaxed(val, pcie->elbi_base + reg);
}

static inline u32 qcom_elbi_readl_relaxed(struct qcom_pcie *pcie, u32 reg)
{
	return readl_relaxed(pcie->elbi_base + reg);
}

static inline void qcom_parf_writel_relaxed(struct qcom_pcie *pcie,
					    u32 val, u32 reg)
{
	writel_relaxed(val, pcie->parf_base + reg);
}

static inline u32 qcom_parf_readl_relaxed(struct qcom_pcie *pcie, u32 reg)
{
	return readl_relaxed(pcie->parf_base + reg);
}

static void msm_pcie_write_mask(void __iomem *addr,
				uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t val;

	val = (readl_relaxed(addr) & ~clear_mask) | set_mask;
	writel_relaxed(val, addr);
	wmb();  /* ensure data is written to hardware register */
}

static void qcom_pcie_config_controller(struct qcom_pcie *dev)
{
	/*
	 * program and enable address translation region 0 (device config
	 * address space); region type config;
	 * axi config address range to device config address range
	 */
	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_VIEWPORT);
	/* ensure that hardware locks the region before programming it */
	wmb();

	writel_relaxed(4, dev->dwc_base + PCIE20_PLR_IATU_CTRL1);
	writel_relaxed(BIT(31), dev->dwc_base + PCIE20_PLR_IATU_CTRL2);
	writel_relaxed(dev->conf.start, dev->dwc_base + PCIE20_PLR_IATU_LBAR);
	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_UBAR);
	writel_relaxed(dev->conf.end, dev->dwc_base + PCIE20_PLR_IATU_LAR);
	writel_relaxed(MSM_PCIE_DEV_CFG_ADDR,
		       dev->dwc_base + PCIE20_PLR_IATU_LTAR);
	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_UTAR);
	/* ensure that hardware registers the configuration */
	wmb();

	/*
	 * program and enable address translation region 2 (device resource
	 * address space); region type memory;
	 * axi device bar address range to device bar address range
	 */
	writel_relaxed(2, dev->dwc_base + PCIE20_PLR_IATU_VIEWPORT);
	/* ensure that hardware locks the region before programming it */
	wmb();

	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_CTRL1);
	writel_relaxed(BIT(31), dev->dwc_base + PCIE20_PLR_IATU_CTRL2);
	writel_relaxed(dev->mem.start, dev->dwc_base + PCIE20_PLR_IATU_LBAR);
	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_UBAR);
	writel_relaxed(dev->mem.end, dev->dwc_base + PCIE20_PLR_IATU_LAR);
	writel_relaxed(dev->mem.start,
		       dev->dwc_base + PCIE20_PLR_IATU_LTAR);
	writel_relaxed(0, dev->dwc_base + PCIE20_PLR_IATU_UTAR);
	/* ensure that hardware registers the configuration */
	wmb();

	/* 256B PCIE buffer setting */
	writel_relaxed(0x1, dev->dwc_base + PCIE20_AXI_MSTR_RESP_COMP_CTRL0);
	writel_relaxed(0x1, dev->dwc_base + PCIE20_AXI_MSTR_RESP_COMP_CTRL1);
	/* ensure that hardware registers the configuration */
	wmb();
}

static int qcom_pcie_parse_dt(struct qcom_pcie *qcom_pcie,
			      struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *elbi_base, *parf_base, *dwc_base;
	struct of_pci_range range;
	struct of_pci_range_parser parser;
	int ret, i;

	elbi_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	qcom_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, elbi_base);
	if (IS_ERR(qcom_pcie->elbi_base)) {
		dev_err(&pdev->dev, "Failed to ioremap elbi space\n");
		return PTR_ERR(qcom_pcie->elbi_base);
	}

	parf_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "parf");
	qcom_pcie->parf_base = devm_ioremap_resource(&pdev->dev, parf_base);
	if (IS_ERR(qcom_pcie->parf_base)) {
		dev_err(&pdev->dev, "Failed to ioremap parf space\n");
		return PTR_ERR(qcom_pcie->parf_base);
	}

	dwc_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	qcom_pcie->dwc_base = devm_ioremap_resource(&pdev->dev, dwc_base);
	if (IS_ERR(qcom_pcie->dwc_base)) {
		dev_err(&pdev->dev, "Failed to ioremap dwc_base space\n");
		return PTR_ERR(qcom_pcie->dwc_base);
	}

	if (of_pci_range_parser_init(&parser, np)) {
		dev_err(&pdev->dev, "missing ranges property\n");
		return -EINVAL;
	}

	/* Get the I/O and memory ranges from DT */
	for_each_of_pci_range(&parser, &range) {
		switch (range.pci_space & 0x3) {
		case 0:		/* cfg */
			of_pci_range_to_resource(&range, np, &qcom_pcie->conf);
			qcom_pcie->conf.flags = IORESOURCE_MEM;
			break;
		case 1:		/* io */
			of_pci_range_to_resource(&range, np, &qcom_pcie->io);
			break;
		default:	/* mem */
			of_pci_range_to_resource(&range, np, &qcom_pcie->mem);
			break;
		}
	}

	qcom_pcie->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (!gpio_is_valid(qcom_pcie->reset_gpio)) {
		dev_err(&pdev->dev, "pcie reset gpio is not valid\n");
		return -EINVAL;
	}

	ret = devm_gpio_request_one(&pdev->dev, qcom_pcie->reset_gpio,
				    GPIOF_DIR_OUT, "pcie_reset");
	if (ret) {
		dev_err(&pdev->dev, "Failed to request pcie reset gpio\n");
		return ret;
	}

	qcom_pcie->iface_clk = devm_clk_get(&pdev->dev, "iface");
	if (IS_ERR(qcom_pcie->iface_clk)) {
		dev_err(&pdev->dev, "Failed to get pcie iface clock\n");
		return PTR_ERR(qcom_pcie->iface_clk);
	}

	qcom_pcie->phy_clk = devm_clk_get(&pdev->dev, "phy");
	if (IS_ERR(qcom_pcie->phy_clk)) {
		dev_err(&pdev->dev, "Failed to get pcie phy clock\n");
		return PTR_ERR(qcom_pcie->phy_clk);
	}

	qcom_pcie->bus_clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(qcom_pcie->bus_clk)) {
		dev_err(&pdev->dev, "Failed to get pcie core clock\n");
		return PTR_ERR(qcom_pcie->bus_clk);
	}

	qcom_pcie->axi_reset = devm_reset_control_get(&pdev->dev, "axi");
	if (IS_ERR(qcom_pcie->axi_reset)) {
		dev_err(&pdev->dev, "Failed to get axi reset\n");
		return PTR_ERR(qcom_pcie->axi_reset);
	}

	qcom_pcie->ahb_reset = devm_reset_control_get(&pdev->dev, "ahb");
	if (IS_ERR(qcom_pcie->ahb_reset)) {
		dev_err(&pdev->dev, "Failed to get ahb reset\n");
		return PTR_ERR(qcom_pcie->ahb_reset);
	}

	qcom_pcie->por_reset = devm_reset_control_get(&pdev->dev, "por");
	if (IS_ERR(qcom_pcie->por_reset)) {
		dev_err(&pdev->dev, "Failed to get por reset\n");
		return PTR_ERR(qcom_pcie->por_reset);
	}

	qcom_pcie->pci_reset = devm_reset_control_get(&pdev->dev, "pci");
	if (IS_ERR(qcom_pcie->pci_reset)) {
		dev_err(&pdev->dev, "Failed to get pci reset\n");
		return PTR_ERR(qcom_pcie->pci_reset);
	}

	qcom_pcie->phy_reset = devm_reset_control_get(&pdev->dev, "phy");
	if (IS_ERR(qcom_pcie->phy_reset)) {
		dev_err(&pdev->dev, "Failed to get phy reset\n");
		return PTR_ERR(qcom_pcie->phy_reset);
	}

	for (i = 0; i < 4; i++) {
		qcom_pcie->irq_int[i] = platform_get_irq(pdev, i+1);
		if (qcom_pcie->irq_int[i] < 0) {
			dev_err(&pdev->dev, "failed to get irq resource\n");
			return qcom_pcie->irq_int[i];
		}
	}

	return 0;
}

static int qcom_pcie_probe(struct platform_device *pdev)
{
	unsigned long flags;
	struct qcom_pcie *qcom_pcie;
	struct device_node *np = pdev->dev.of_node;
	struct hw_pci *hw;
	int ret;
	u32 val;
	uint32_t force_gen1 = 0;

	qcom_pcie = devm_kzalloc(&pdev->dev, sizeof(*qcom_pcie), GFP_KERNEL);
	if (!qcom_pcie) {
		dev_err(&pdev->dev, "no memory for qcom_pcie\n");
		return -ENOMEM;
	}
	qcom_pcie->dev = &pdev->dev;

	ret = qcom_pcie_parse_dt(qcom_pcie, pdev);
	if (IS_ERR_VALUE(ret))
		return ret;

	qcom_pcie->cfg_base = devm_ioremap_resource(&pdev->dev,
						    &qcom_pcie->conf);
	if (IS_ERR(qcom_pcie->cfg_base)) {
		dev_err(&pdev->dev, "Failed to ioremap PCIe cfg space\n");
		return PTR_ERR(qcom_pcie->cfg_base);
	}

	gpio_set_value(qcom_pcie->reset_gpio, 0);
	usleep_range(10000, 15000);

	/* assert PCIe PARF reset while powering the core */
	reset_control_assert(qcom_pcie->ahb_reset);

	/* enable clocks */
	ret = clk_prepare_enable(qcom_pcie->iface_clk);
	if (ret)
		return ret;
	ret = clk_prepare_enable(qcom_pcie->phy_clk);
	if (ret)
		return ret;
	ret = clk_prepare_enable(qcom_pcie->bus_clk);
	if (ret)
		return ret;

	/*
	 * de-assert PCIe PARF reset;
	 * wait 1us before accessing PARF registers
	 */
	reset_control_deassert(qcom_pcie->ahb_reset);
	udelay(1);

	/* enable PCIe clocks and resets */
	msm_pcie_write_mask(qcom_pcie->parf_base + PCIE20_PARF_PHY_CTRL,
			    BIT(0), 0);

	/* Set Tx Termination Offset */
	val = qcom_parf_readl_relaxed(qcom_pcie, PCIE20_PARF_PHY_CTRL);
	if (of_device_is_compatible(np, "qcom,pcie-ipq8064-v2"))
		val |= PCIE20_PARF_PHY_CTRL_PHY_TX0_TERM_OFFST(0);
	else
		val |= PCIE20_PARF_PHY_CTRL_PHY_TX0_TERM_OFFST(7);

	qcom_parf_writel_relaxed(qcom_pcie, val, PCIE20_PARF_PHY_CTRL);

	/* PARF programming */
	qcom_parf_writel_relaxed(qcom_pcie,
			PCIE20_PARF_PCS_DEEMPH_TX_DEEMPH_GEN1(0x18) |
			PCIE20_PARF_PCS_DEEMPH_TX_DEEMPH_GEN2_3_5DB(0x18) |
			PCIE20_PARF_PCS_DEEMPH_TX_DEEMPH_GEN2_6DB(0x22),
			PCIE20_PARF_PCS_DEEMPH);
	qcom_parf_writel_relaxed(qcom_pcie,
			PCIE20_PARF_PCS_SWING_TX_SWING_FULL(0x78) |
			PCIE20_PARF_PCS_SWING_TX_SWING_LOW(0x78),
			PCIE20_PARF_PCS_SWING);
	qcom_parf_writel_relaxed(qcom_pcie, (4<<24), PCIE20_PARF_CONFIG_BITS);
	/* ensure that hardware registers the PARF configuration */
	wmb();

	/* enable reference clock */
	msm_pcie_write_mask(qcom_pcie->parf_base + PCIE20_PARF_PHY_REFCLK,
			    BIT(12), BIT(16));

	/* ensure that access is enabled before proceeding */
	wmb();

	/* de-assert PICe PHY, Core, POR and AXI clk domain resets */
	reset_control_deassert(qcom_pcie->phy_reset);
	reset_control_deassert(qcom_pcie->pci_reset);
	reset_control_deassert(qcom_pcie->por_reset);
	reset_control_deassert(qcom_pcie->axi_reset);

	/* wait 150ms for clock acquisition */
	usleep_range(10000, 15000);

	/* de-assert PCIe reset link to bring EP out of reset */
	gpio_set_value(qcom_pcie->reset_gpio, 1);
	usleep_range(10000, 15000);

	/*
	 * Force Gen1 in Gen1 marked PCIe SLOT
	 */
	if (of_device_is_compatible(np, "qcom,pcie-ipq8064")) {
		ret = of_property_read_u32(np, "force_gen1", &force_gen1);
		if (!ret && force_gen1) {
			writel_relaxed((readl_relaxed(
				qcom_pcie->dwc_base + PCIE20_LNK_CAS2) | 1),
				qcom_pcie->dwc_base + PCIE20_LNK_CAS2);
		}
	}

	/* enable link training */
	val = qcom_elbi_readl_relaxed(qcom_pcie, PCIE20_ELBI_SYS_CTRL);
	val |= PCIE20_ELBI_SYS_CTRL_LTSSM_EN;
	qcom_elbi_writel_relaxed(qcom_pcie, val, PCIE20_ELBI_SYS_CTRL);
	wmb();

	/* poll for link to come up for upto 100ms */
	ret = readl_poll_timeout(
			(qcom_pcie->dwc_base + PCIE20_CAP_LINKCTRLSTATUS),
			val, (val & BIT(29)), 10000, 100000);

	dev_info(&pdev->dev, "link initialized %d\n", ret);

	qcom_pcie_config_controller(qcom_pcie);

	platform_set_drvdata(pdev, qcom_pcie);

	spin_lock_irqsave(&qcom_hw_pci_lock, flags);
	qcom_hw_pci[nr_controllers].private_data = (void **)&qcom_pcie;
	hw = &qcom_hw_pci[nr_controllers];
	nr_controllers++;
	spin_unlock_irqrestore(&qcom_hw_pci_lock, flags);

	pci_common_init_dev(qcom_pcie->dev, hw);

	return 0;
}

static int __exit qcom_pcie_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id qcom_pcie_match[] = {
	{	.compatible = "qcom,pcie-ipq8064", },
	{}
};

static struct platform_driver qcom_pcie_driver = {
	.probe	= qcom_pcie_probe,
	.remove	= qcom_pcie_remove,
	.driver	= {
		.name		= "qcom_pcie",
		.owner		= THIS_MODULE,
		.of_match_table	= qcom_pcie_match,
	},
};

static int qcom_pcie_init(void)
{
	return platform_driver_register(&qcom_pcie_driver);
}
subsys_initcall(qcom_pcie_init);

/* RC do not represent the right class; set it to PCI_CLASS_BRIDGE_PCI */
static void msm_pcie_fixup_early(struct pci_dev *dev)
{
	if (dev->hdr_type == 1)
		dev->class = (dev->class & 0xff) | (PCI_CLASS_BRIDGE_PCI << 8);
}
DECLARE_PCI_FIXUP_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP, msm_pcie_fixup_early);
