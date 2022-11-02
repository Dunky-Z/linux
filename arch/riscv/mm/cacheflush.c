// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 SiFive
 */

#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_SMP

#include <asm/sbi.h>

static void ipi_remote_fence_i(void *info)
{
	return local_flush_icache_all();
}

void flush_icache_all(void)
{
	local_flush_icache_all();

	if (IS_ENABLED(CONFIG_RISCV_SBI) && !riscv_use_ipi_for_rfence())
		sbi_remote_fence_i(NULL);
	else
		on_each_cpu(ipi_remote_fence_i, NULL, 1);
}
EXPORT_SYMBOL(flush_icache_all);

/*
 * Performs an icache flush for the given MM context.  RISC-V has no direct
 * mechanism for instruction cache shoot downs, so instead we send an IPI that
 * informs the remote harts they need to flush their local instruction caches.
 * To avoid pathologically slow behavior in a common case (a bunch of
 * single-hart processes on a many-hart machine, ie 'make -j') we avoid the
 * IPIs for harts that are not currently executing a MM context and instead
 * schedule a deferred local instruction cache flush to be performed before
 * execution resumes on each hart.
 */
void flush_icache_mm(struct mm_struct *mm, bool local)
{
	unsigned int cpu;
	cpumask_t others, *mask;

	preempt_disable();

	/* Mark every hart's icache as needing a flush for this MM. */
	mask = &mm->context.icache_stale_mask;
	cpumask_setall(mask);
	/* Flush this hart's I$ now, and mark it as flushed. */
	cpu = smp_processor_id();
	cpumask_clear_cpu(cpu, mask);
	local_flush_icache_all();

	/*
	 * Flush the I$ of other harts concurrently executing, and mark them as
	 * flushed.
	 */
	cpumask_andnot(&others, mm_cpumask(mm), cpumask_of(cpu));
	local |= cpumask_empty(&others);
	if (mm == current->active_mm && local) {
		/*
		 * It's assumed that at least one strongly ordered operation is
		 * performed on this hart between setting a hart's cpumask bit
		 * and scheduling this MM context on that hart.  Sending an SBI
		 * remote message will do this, but in the case where no
		 * messages are sent we still need to order this hart's writes
		 * with flush_icache_deferred().
		 */
		smp_mb();
	} else if (IS_ENABLED(CONFIG_RISCV_SBI) &&
		   !riscv_use_ipi_for_rfence()) {
		sbi_remote_fence_i(&others);
	} else {
		on_each_cpu_mask(&others, ipi_remote_fence_i, NULL, 1);
	}

	preempt_enable();
}

#endif /* CONFIG_SMP */

#ifdef CONFIG_MMU
void flush_icache_pte(pte_t pte)
{
	struct page *page = pte_page(pte);

	if (!test_and_set_bit(PG_dcache_clean, &page->flags))
		flush_icache_all();
}
#endif /* CONFIG_MMU */

unsigned int riscv_cbom_block_size;
EXPORT_SYMBOL_GPL(riscv_cbom_block_size);

void riscv_init_cbom_blocksize(void)
{
	static struct acpi_table_header *rhct;
	unsigned long cbom_hartid;
	u32 val, probed_block_size;
	acpi_status status;
	unsigned int cpu;
	int ret;

	probed_block_size = 0;

#ifdef CONFIG_ACPI
	if (!acpi_disabled) {
		status = acpi_get_table(ACPI_SIG_RHCT, 0, &rhct);
		if (ACPI_FAILURE(status))
			return;
	}
#endif
	for_each_possible_cpu(cpu) {
		unsigned long hartid;

		if (acpi_disabled) {
			struct device_node *node;
			node = of_cpu_device_node_get(cpu);
			if (node)
				ret = riscv_of_processor_hartid(node, &hartid);
			if (ret)
				continue;

			/* set block-size for cbom extension if available */
			ret = of_property_read_u32(node, "riscv,cbom-block-size", &val);
			if (ret)
				continue;
		} else {
			ret = acpi_get_cbom_block_size(rhct, get_acpi_id_for_cpu(cpu), &val);
			hartid = acpi_get_madt_rintc(cpu)->hartid;
		}

		if (!probed_block_size) {
			probed_block_size = val;
			cbom_hartid = hartid;
		} else {
			if (probed_block_size != val)
				pr_warn("cbom-block-size mismatched between harts %lu and %lu\n",
					cbom_hartid, hartid);
		}
	}

#ifdef CONFIG_ACPI
	if (!acpi_disabled)
		acpi_put_table((struct acpi_table_header *)rhct);
#endif

	if (probed_block_size)
		riscv_cbom_block_size = probed_block_size;
}
