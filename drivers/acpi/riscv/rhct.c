// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022, Ventana Micro Systems Inc
 *	Author: Sunil V L <sunilvl@ventanamicro.com>
 *
 */

#define pr_fmt(fmt)	"ACPI: RHCT: " fmt

#include <linux/acpi.h>

static void acpi_rhct_warn_missing(void)
{
	pr_warn_once("No RHCT table found\n");
}

static struct acpi_table_header *acpi_get_rhct(void)
{
	static struct acpi_table_header *rhct;
	acpi_status status;

	/*
	 * RHCT will be used at runtime on every CPU, so we
	 * don't need to call acpi_put_table() to release the table mapping.
	 */
	if (!rhct) {
		status = acpi_get_table(ACPI_SIG_RHCT, 0, &rhct);
		if (ACPI_FAILURE(status))
			acpi_rhct_warn_missing();
	}

	return rhct;
}

/*
 * During early boot, the caller should call acpi_get_table() and pass its pointer to
 * these functions(and free up later). At run time, since this table can be used
 * multiple times, pass NULL so that the table remains in memory
 */
int acpi_get_riscv_isa(struct acpi_table_header *table, unsigned int acpi_cpu_id, char *isa)
{
	struct acpi_rhct_node *node, *ref_node, *end;
	struct acpi_table_rhct *rhct;
	struct acpi_rhct_isa_string *isa_node;
	int i, j;

	if (acpi_disabled) {
		pr_info("acpi_get_riscv_isa: acpi is disabled\n");
		return -1;
	}

	if (!table) {
		rhct = (struct acpi_table_rhct *)acpi_get_rhct();
		if (!rhct)
			return -ENOENT;
	}
	else {
		rhct = (struct acpi_table_rhct *)table;
	}
	node = ACPI_ADD_PTR(struct acpi_rhct_node, rhct, rhct->node_offset);
	end = ACPI_ADD_PTR(struct acpi_rhct_node, rhct, rhct->header.length);

	for (i = 0; i < rhct->node_count; i++) {
		if (node >= end)
			break;
		switch (node->type) {
			struct acpi_rhct_hart_info *hart_info;

		case ACPI_RHCT_NODE_HART_INFO:
			hart_info = (struct acpi_rhct_hart_info *)node->node_data;
			if (acpi_cpu_id != hart_info->acpi_proc_id)
				break;
			for (j = 0; j < hart_info->num_offsets; j++) {
				ref_node = ACPI_ADD_PTR(struct acpi_rhct_node, rhct, hart_info->node_offset[j]);
				if (ref_node->type == ACPI_RHCT_NODE_ISA_STRING) {
					isa_node = (struct acpi_rhct_isa_string *)ref_node->node_data;
					strncpy(isa, isa_node->isa, isa_node->isa_length);
					return 0;
				}
			}
			break;
		}
		node = ACPI_ADD_PTR(struct acpi_rhct_node, node, node->length);
	}

	return -1;
}

/*
 * During early boot, the caller should call acpi_get_table() and pass its pointer to
 * these functions(and free up later). At run time, since this table can be used
 * multiple times, pass NULL so that the table remains in memory
 */
int acpi_get_cbom_block_size(struct acpi_table_header *table, unsigned int acpi_cpu_id, u32 *cbom_size)
{
	struct acpi_rhct_node *node, *ref_node, *end;
	struct acpi_table_rhct *rhct;
	struct acpi_rhct_cmo_node *cmo_node;
	int i, j;

	if (acpi_disabled) {
		pr_info("acpi_get_riscv_isa: acpi is disabled\n");
		return -ENOENT;
	}

	if (!table) {
		rhct = (struct acpi_table_rhct *)acpi_get_rhct();
		if (!rhct)
			return -ENOENT;
	}
	else {
		rhct = (struct acpi_table_rhct *)table;
	}

	node = ACPI_ADD_PTR(struct acpi_rhct_node, rhct, rhct->node_offset);
	end = ACPI_ADD_PTR(struct acpi_rhct_node, rhct, rhct->header.length);

	for (i = 0; i < rhct->node_count; i++) {
		if (node >= end)
			break;
		switch (node->type) {
			struct acpi_rhct_hart_info *hart_info;

		case ACPI_RHCT_NODE_HART_INFO:
			hart_info = (struct acpi_rhct_hart_info *)node->node_data;
			if (acpi_cpu_id != hart_info->acpi_proc_id)
				break;
			for (j = 0; j < hart_info->num_offsets; j++) {
				ref_node = ACPI_ADD_PTR(struct acpi_rhct_node, rhct, hart_info->node_offset[j]);
				if (ref_node->type == ACPI_RHCT_NODE_CMO) {
					cmo_node = (struct acpi_rhct_cmo_node *)ref_node->node_data;
					*cbom_size = cmo_node->cbom_size;
					return 0;
				}
			}
			break;
		}
		node = ACPI_ADD_PTR(struct acpi_rhct_node, node, node->length);
	}

	return -ENOENT;
}
