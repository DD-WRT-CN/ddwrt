/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006  Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2007  Felix Fietkau <nbd@openwrt.org>
 *
 */
#ifndef __ASM_MACH_GENERIC_DMA_COHERENCE_H
#define __ASM_MACH_GENERIC_DMA_COHERENCE_H

#define PCI_DMA_OFFSET	0x20000000

#include <linux/device.h>

static inline dma_addr_t ar231x_dev_offset(struct device *dev)
{
#ifdef CONFIG_PCI
	extern struct bus_type pci_bus_type;

	if (dev && dev->bus == &pci_bus_type)
		return PCI_DMA_OFFSET;
	else
#endif
		return 0;
}

static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr, size_t size)
{
	return virt_to_phys(addr) + ar231x_dev_offset(dev);
}

static inline dma_addr_t plat_map_dma_mem_page(struct device *dev, struct page *page)
{
	return page_to_phys(page) + ar231x_dev_offset(dev);
}

static inline unsigned long plat_dma_addr_to_phys(struct device *dev,
	dma_addr_t dma_addr)
{
	return dma_addr - ar231x_dev_offset(dev);
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr,
	size_t size, enum dma_data_direction direction)
{
}

static inline int plat_dma_supported(struct device *dev, u64 mask)
{
	return 1;
}

static inline int plat_device_is_coherent(struct device *dev)
{
#ifdef CONFIG_DMA_COHERENT
	return 1;
#endif
#ifdef CONFIG_DMA_NONCOHERENT
	return 0;
#endif
}

static inline void plat_post_dma_flush(struct device *dev)
{
}

#endif /* __ASM_MACH_GENERIC_DMA_COHERENCE_H */
