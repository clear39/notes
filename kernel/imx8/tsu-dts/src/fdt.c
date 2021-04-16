

// @ drivers/of/fdt.c
bool __init early_init_dt_scan(void *params)
{
	bool status;

	status = early_init_dt_verify(params);
	if (!status)
		return false;

	early_init_dt_scan_nodes();
	return true;
}


bool __init early_init_dt_verify(void *params)
{
	if (!params)
		return false;

	/* check device tree validity */
	if (fdt_check_header(params))
		return false;

	/* Setup flat device-tree pointer */
	// 这里initial_boot_params很重要
	initial_boot_params = params;

	of_fdt_crc32 = crc32_be(~0, initial_boot_params, fdt_totalsize(initial_boot_params));
	return true;
}


struct fdt_header {
	fdt32_t magic;			 /* magic word FDT_MAGIC */
	fdt32_t totalsize;		 /* total size of DT block */
	fdt32_t off_dt_struct;		 /* offset to structure */
	fdt32_t off_dt_strings;		 /* offset to strings */
	fdt32_t off_mem_rsvmap;		 /* offset to memory reserve map */
	fdt32_t version;		 /* format version */
	fdt32_t last_comp_version;	 /* last compatible version */

	/* version 2 fields below */
	fdt32_t boot_cpuid_phys;	 /* Which physical CPU id we're
					    booting on */
	/* version 3 fields below */
	fdt32_t size_dt_strings;	 /* size of the strings block */

	/* version 17 fields below */
	fdt32_t size_dt_struct;		 /* size of the structure block */
};


int fdt_check_header(const void *fdt)
{
	// #define FDT_MAGIC	0xd00dfeed	/* 4: version, 4: total size */
	// fdt_magic 内部将 fdt 强制转成 fdt_header 读取 magic 成员
	if (fdt_magic(fdt) == FDT_MAGIC) {
		/* Complete tree */
		if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION)
			return -FDT_ERR_BADVERSION;
		if (fdt_last_comp_version(fdt) > FDT_LAST_SUPPORTED_VERSION)
			return -FDT_ERR_BADVERSION;
	} else if (fdt_magic(fdt) == FDT_SW_MAGIC) { // #define FDT_SW_MAGIC		(~FDT_MAGIC)
		/* Unfinished sequential-write blob */
		if (fdt_size_dt_struct(fdt) == 0)
			return -FDT_ERR_BADSTATE;
	} else {
		return -FDT_ERR_BADMAGIC;
	}

	return 0;
}



void __init early_init_dt_scan_nodes(void)
{
	/* Retrieve various information from the /chosen node */
	of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);

	/* Initialize {size,address}-cells info */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);

	/* Setup memory, calling early_init_dt_add_memory_arch */
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);
}


int __init of_scan_flat_dt(int (*it)(unsigned long node, const char *uname, int depth, void *data), void *data)
{
	// initial_boot_params 是执行dtb文件的指针
	const void *blob = initial_boot_params;
	const char *pathp;
	int offset, rc = 0, depth = -1;

	if (!blob)
		return 0;

	for (offset = fdt_next_node(blob, -1, &depth);
	     offset >= 0 && depth >= 0 && !rc;
	     offset = fdt_next_node(blob, offset, &depth)) {

		pathp = fdt_get_name(blob, offset, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);
		rc = it(offset, pathp, depth, data);
	}
	return rc;
}

// 搜索 chosen 节点
// fsl-imx8qxp-autolink-tsu.dtsi
/*
chosen {
	bootargs = "console=ttyLP0,115200 earlycon=lpuart32,0x5a060000,115200";
	stdout-path = &lpuart0;
};
*/
int __init early_init_dt_scan_chosen(unsigned long node, const char *uname, int depth, void *data)
{
	int l = 0;
	const char *p = NULL;
	char *cmdline = data;

	pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

	// 非 chosen 节点直接返回
	if (depth != 1 || !cmdline || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	// 
	early_init_dt_check_for_initrd(node);

	/* Put CONFIG_CMDLINE in if forced or if data had nothing in it to start */
	if (overwrite_incoming_cmdline || !cmdline[0])
		strlcpy(cmdline, config_cmdline, COMMAND_LINE_SIZE);

	/* Retrieve command line unless forcing */
	if (read_dt_cmdline)
		p = of_get_flat_dt_prop(node, "bootargs", &l);

	if (p != NULL && l > 0) {
		if (concat_cmdline) {
			int cmdline_len;
			int copy_len;
			strlcat(cmdline, " ", COMMAND_LINE_SIZE);
			cmdline_len = strlen(cmdline);
			copy_len = COMMAND_LINE_SIZE - cmdline_len - 1;
			copy_len = min((int)l, copy_len);
			strncpy(cmdline + cmdline_len, p, copy_len);
			cmdline[cmdline_len + copy_len] = '\0';
		} else {
			strlcpy(cmdline, p, min((int)l, COMMAND_LINE_SIZE));
		}
	}

/*
[    0.000000] Command line is: console=ttyLP0,115200 earlycon=lpuart32,0x5a060000,115200 androidboot.console=ttyLP0 androidboot.xen_boot=default init=/init androidboot.hardware=freescale androidboot.fbTileS
upport=enable cma=800M@0x960M-0xe00M androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never androidboot.wificountrycode=CN galcore.contiguousSize=33554432 video=H
DMI-A-2:d androidboot.selinux=permissive loglevel=7 printk.devkmsg=on buildvariant=userdebug androidboot.serialno=1709600e82964e28 androidboot.btmacaddr=17:09:60:0e:82:96 androidboot.soc_type=imx8qxp android
boot.storage_type=emmc androidboot.boottime=1BLL:0,1BLE:6725916,KL:0,KD:0,AVB:348,ODT:0,SW:0 androidboot.bootreason=reboot androidboot.verifiedbootstate=orange androidboot.slot_suffix=_a root=PARTUUID=7743fd
ed-6e91-4203-8a2a-26cd9626fbf1 androidboot.vbmeta.device=PARTUUID=74c6cd89-5881-4b88-b099-122fb78e8fa2 androidboot.vbmeta.avb_version=1.1 androidboot.vbmeta.device_state=unlocked
*/
	pr_debug("Command line is: %s\n", (char*)data);

	/* break now */
	return 1;
}

static void __init early_init_dt_check_for_initrd(unsigned long node)
{
	u64 start, end;
	int len;
	const __be32 *prop;

	pr_debug("Looking for initrd properties... ");

	prop = of_get_flat_dt_prop(node, "linux,initrd-start", &len);
	if (!prop)
		return;
	start = of_read_number(prop, len/4);

	prop = of_get_flat_dt_prop(node, "linux,initrd-end", &len);
	if (!prop)
		return;
	end = of_read_number(prop, len/4);

	__early_init_dt_declare_initrd(start, end);

	pr_debug("initrd_start=0x%llx  initrd_end=0x%llx\n", (unsigned long long)start, (unsigned long long)end);
}


/*
memory@80000000 {
	device_type = "memory";
	reg = <0x00000000 0x80000000 0 0x40000000>; // DRAM space - 1, size : 1 GB DRAM 
};
*/
int __init early_init_dt_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	const __be32 *reg, *endp;
	int l;
	bool hotpluggable;

	pr_debug("'device_type' type: %s\n", type);

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (!IS_ENABLED(CONFIG_PPC32) || depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
	if (reg == NULL)
		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;
	
	endp = reg + (l / sizeof(__be32));
	hotpluggable = of_get_flat_dt_prop(node, "hotpluggable", NULL);

	// memory scan node memory@80000000, reg size 64
	pr_debug("memory scan node %s, reg size %d,\n", uname, l);

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
		pr_debug(" - %llx ,  %llx\n", (unsigned long long)base, (unsigned long long)size);

		early_init_dt_add_memory_arch(base, size);

		if (!hotpluggable)
			continue;

		if (early_init_dt_mark_hotplug_memory_arch(base, size))
			pr_warn("failed to mark hotplug range 0x%llx - 0x%llx\n", base, base + size);
	}

	return 0;
}

u64 __init dt_mem_next_cell(int s, const __be32 **cellp)
{
	const __be32 *p = *cellp;

	*cellp = p + s;
	return of_read_number(p, s);
}

static inline u64 of_read_number(const __be32 *cell, int size)
{
	u64 r = 0;
	while (size--)
		r = (r << 32) | be32_to_cpu(*(cell++));
	return r;
}

void __init __weak early_init_dt_add_memory_arch(u64 base, u64 size)
{
	const u64 phys_offset = MIN_MEMBLOCK_ADDR;

	if (!PAGE_ALIGNED(base)) {
		if (size < PAGE_SIZE - (base & ~PAGE_MASK)) {
			pr_warn("Ignoring memory block 0x%llx - 0x%llx\n", base, base + size);
			return;
		}
		size -= PAGE_SIZE - (base & ~PAGE_MASK);
		base = PAGE_ALIGN(base);
	}
	size &= PAGE_MASK;

	if (base > MAX_MEMBLOCK_ADDR) {
		pr_warning("Ignoring memory block 0x%llx - 0x%llx\n", base, base + size);
		return;
	}

	if (base + size - 1 > MAX_MEMBLOCK_ADDR) {
		pr_warning("Ignoring memory range 0x%llx - 0x%llx\n", ((u64)MAX_MEMBLOCK_ADDR) + 1, base + size);
		size = MAX_MEMBLOCK_ADDR - base + 1;
	}

	if (base + size < phys_offset) {
		pr_warning("Ignoring memory block 0x%llx - 0x%llx\n", base, base + size);
		return;
	}
	if (base < phys_offset) {
		pr_warning("Ignoring memory range 0x%llx - 0x%llx\n", base, phys_offset);
		size -= phys_offset - base;
		base = phys_offset;
	}
	memblock_add(base, size);
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
const char * __init of_flat_dt_get_machine_name(void)
{
	const char *name;
	// of_get_flat_dt_root 返回值为 0
	unsigned long dt_root = of_get_flat_dt_root();

	// model = "Freescale i.MX8DX";
	// dt_root = 0
	name = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!name) 
		// compatible = "fsl,imx8dx", "fsl,imx8qxp";
		name = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	return name;
}

// node = 0
// 
const void *__init of_get_flat_dt_prop(unsigned long node, const char *name, int *size)
{
	// initial_boot_params 指向dtb内存地址
	// 定义在 fdt_ro.c
	// node = 0
	// name = "model"
	// size = NULL
	return fdt_getprop(initial_boot_params, node, name, size);
}





///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
void __init unflatten_device_tree(void)
{
	__unflatten_device_tree(initial_boot_params, NULL, &of_root,early_init_dt_alloc_memory_arch, false);

	/* Get pointer to "/chosen" and "/aliases" nodes for use everywhere */
	of_alias_scan(early_init_dt_alloc_memory_arch);

	unittest_unflatten_overlay_base();
}