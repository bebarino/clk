/*
 * Copyright 2006 PathScale, Inc.  All Rights Reserved.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/export.h>
#include <linux/io.h>

/**
 * __iowrite32_copy - copy data to MMIO space, in 32-bit units
 * @to: destination, in MMIO space (must be 32-bit aligned)
 * @from: source (must be 32-bit aligned)
 * @count: number of 32-bit quantities to copy
 *
 * Copy data from kernel space to MMIO space, in units of 32 bits at a
 * time.  Order of access is not guaranteed, nor is a memory barrier
 * performed afterwards.
 */
void __attribute__((weak)) __iowrite32_copy(void __iomem *to,
					    const void *from,
					    size_t count)
{
	u32 __iomem *dst = to;
	const u32 *src = from;
	const u32 *end = src + count;

	while (src < end)
		__raw_writel(*src++, dst++);
}
EXPORT_SYMBOL_GPL(__iowrite32_copy);

/**
 * __ioread32_copy - copy data from MMIO space, in 32-bit units
 * @to: destination (must be 32-bit aligned)
 * @from: source, in MMIO space (must be 32-bit aligned)
 * @count: number of 32-bit quantities to copy
 *
 * Copy data from MMIO space to kernel space, in units of 32 bits at a
 * time.  Order of access is not guaranteed, nor is a memory barrier
 * performed afterwards.
 */
void __ioread32_copy(void *to, const void __iomem *from, size_t count)
{
	u32 *dst = to;
	const u32 __iomem *src = from;
	const u32 __iomem *end = src + count;

	while (src < end)
		*dst++ = __raw_readl(src++);
}
EXPORT_SYMBOL_GPL(__ioread32_copy);

/**
 * __iowrite64_copy - copy data to MMIO space, in 64-bit or 32-bit units
 * @to: destination, in MMIO space (must be 64-bit aligned)
 * @from: source (must be 64-bit aligned)
 * @count: number of 64-bit quantities to copy
 *
 * Copy data from kernel space to MMIO space, in units of 32 or 64 bits at a
 * time.  Order of access is not guaranteed, nor is a memory barrier
 * performed afterwards.
 */
void __attribute__((weak)) __iowrite64_copy(void __iomem *to,
					    const void *from,
					    size_t count)
{
#ifdef CONFIG_64BIT
	u64 __iomem *dst = to;
	const u64 *src = from;
	const u64 *end = src + count;

	while (src < end)
		__raw_writeq(*src++, dst++);
#else
	__iowrite32_copy(to, from, count * 2);
#endif
}

EXPORT_SYMBOL_GPL(__iowrite64_copy);

/**
 * __iowrite32_fifo - copy data to 32-bit MMIO FIFO
 * @to: destination, in MMIO space
 * @from: source, in kernel space
 * @count: number of bytes to copy
 *
 * Copy data from kernel space to an MMIO FIFO, in units of 32 bits at a
 * time. A memory barrier is not performed afterwards.
 */
void __iowrite32_fifo(void __iomem *to, const void *from, size_t count)
{
	const u8 *src = from;
	int i;

	while (count) {
		u32 dst = 0;
		size_t left = min(sizeof(u32), count);

		for (i = 0; i < left; i++) {
			u8 val = *src++;
			dst |= val << (i * 8);
			count--;
		}
		__raw_writel(dst, to);
	}
}

EXPORT_SYMBOL_GPL(__iowrite32_fifo);

/**
 * __ioread32_fifo - copy data from 32-bit MMIO FIFO
 * @to: destination, in kernel space
 * @from: source, in MMIO space
 * @count: number of bytes to copy
 *
 * Copy data from an MMIO FIFO to kernel space, in units of 32 bits at a
 * time. A memory barrier is not performed afterwards.
 */
void __ioread32_fifo(void *to, const void __iomem *from, size_t count)
{
	u8 *dst = to;
	int i;

	while (count) {
		u32 src = __raw_readl(from);
		size_t left = min(sizeof(u32), count);

		for (i = 0; i < left; i++) {
			*dst++ = src;
			src >>= 8;
			count--;
		}
	}
}

EXPORT_SYMBOL_GPL(__ioread32_fifo);
