
// @ arch/arm64/kernel/setup.c
// 在 start_kernel(void)中 调用
void __init setup_arch(char **cmdline_p)
{
	// [    0.000000] Boot CPU: AArch64 Processor [410fd042]
	pr_info("*****Boot CPU: AArch64 Processor [%08x]****\n", read_cpuid_id());

	......

    // dts 
	// setup_arch __fdt_pointer:-2095054848
	pr_info("%s __fdt_pointer:%d \n",__func__,__fdt_pointer);
	setup_machine_fdt(__fdt_pointer);

	......

	// setup_arch acpi_disabled:1
	pr_info("%s acpi_disabled:%d \n",__func__,acpi_disabled);
	if (acpi_disabled)
		unflatten_device_tree();

	......
}


static void __init setup_machine_fdt(phys_addr_t dt_phys)
{
	// @ arch/arm64/mm/mmu.c
	void *dt_virt = fixmap_remap_fdt(dt_phys);
	const char *name;

	// early_init_dt_scan @ drivers/of/fdt.c
	// 在 early_init_dt_scan 中
	// 对dtb文件的头进行校验是否合法
	// 对 initial_boot_params 进行赋值
	// 
	if (!dt_virt || !early_init_dt_scan(dt_virt)) {
		pr_crit("\n"
			"Error: invalid device tree blob at physical address %pa (virtual address 0x%p)\n"
			"The dtb must be 8-byte aligned and must not exceed 2 MB in size\n"
			"\nPlease check your bootloader.",
			&dt_phys, dt_virt);

		while (true)
			cpu_relax();
	}

	// 
	name = of_flat_dt_get_machine_name();
	if (!name)
		return;

	// [    0.000000] Machine model: Freescale i.MX8QXP MEK
	pr_info("Machine model: %s\n", name);
	dump_stack_set_arch_desc("%s (DT)", name);
}












