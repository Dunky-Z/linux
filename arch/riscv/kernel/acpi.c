// SPDX-License-Identifier: GPL-2.0-only
/*
 *  RISC-V Specific Low-Level ACPI Boot Support
 *
 *  Copyright (C) 2013-2014, Linaro Ltd.
 *	Author: Al Stone <al.stone@linaro.org>
 *	Author: Graeme Gregory <graeme.gregory@linaro.org>
 *	Author: Hanjun Guo <hanjun.guo@linaro.org>
 *	Author: Tomasz Nowicki <tomasz.nowicki@linaro.org>
 *	Author: Naresh Bhat <naresh.bhat@linaro.org>
 *
 *  Copyright (C) 2021, Ventana Micro Systems Inc.
 *	Author: Sunil V L <sunilvl@ventanamicro.com>
 */

#include <linux/acpi.h>
#include <linux/io.h>

int acpi_noirq = 1;		/* skip ACPI IRQ initialization */
int acpi_disabled = 1;
EXPORT_SYMBOL(acpi_disabled);

int acpi_pci_disabled = 1;	/* skip ACPI PCI scan and IRQ initialization */
EXPORT_SYMBOL(acpi_pci_disabled);

/*
 * __acpi_map_table() will be called before page_init(), so early_ioremap()
 * or early_memremap() should be called here to for ACPI table mapping.
 */
void __init __iomem *__acpi_map_table(unsigned long phys, unsigned long size)
{
	if (!size)
		return NULL;

	return early_memremap(phys, size);
}

void __init __acpi_unmap_table(void __iomem *map, unsigned long size)
{
	if (!map || !size)
		return;

	early_memunmap(map, size);
}

void *acpi_os_ioremap(acpi_physical_address phys, acpi_size size)
{
	return memremap(phys, size, MEMREMAP_WB);
}
