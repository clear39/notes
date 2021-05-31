
/*
reserved-memory {
	#address-cells = <0x00000002>;
	#size-cells = <0x00000002>;
	ranges;
	decoder_boot@0x84000000 {
		no-map;
		reg = <0x00000000 0x84000000 0x00000000 0x02000000>;
		linux,phandle = <0x000000e8>;
		phandle = <0x000000e8>;
	};
	encoder_boot@0x86000000 {
		no-map;
		reg = <0x00000000 0x86000000 0x00000000 0x00200000>;
		linux,phandle = <0x000000ea>;
		phandle = <0x000000ea>;
	};
	rpmsg@0x90000000 {
		no-map;
		reg = <0x00000000 0x90000000 0x00000000 0x00400000>;
		linux,phandle = <0x000000e4>;
		phandle = <0x000000e4>;
	};
	rpmsg_dma@0x90400000 {
		compatible = "shared-dma-pool";
		no-map;
		reg = <0x00000000 0x90400000 0x00000000 0x00800000>;
		linux,phandle = <0x000000ef>;
		phandle = <0x000000ef>;
	};
	decoder_rpc@0x92000000 {
		no-map;
		reg = <0x00000000 0x92000000 0x00000000 0x00200000>;
		linux,phandle = <0x000000e9>;
		phandle = <0x000000e9>;
	};
	encoder_rpc@0x92200000 {
		no-map;
		reg = <0x00000000 0x92200000 0x00000000 0x00200000>;
		linux,phandle = <0x000000eb>;
		phandle = <0x000000eb>;
	};
	dsp@0x92400000 {
		no-map;
		reg = <0x00000000 0x92400000 0x00000000 0x02000000>;
		linux,phandle = <0x000000dc>;
		phandle = <0x000000dc>;
	};
	encoder_reserved@0x94400000 {
		no-map;
		reg = <0x00000000 0x94400000 0x00000000 0x00800000>;
		linux,phandle = <0x000000ec>;
		phandle = <0x000000ec>;
	};
	imx_ion@0 {
		compatible = "imx-ion-pool";
		reg = <0x00000000 0xf8000000 0x00000000 0x08000000>;
		status = "disabled";
	};
};
*/

/*
setup_arch
arm64_memblock_init  
-->early_init_fdt_scan_reserved_mem  // 调用在 dma_contiguous_reserve之前  @ drivers/of/fdt.c
----->__fdt_scan_reserved_mem
      ----> __reserved_mem_reserve_reg
    		 ----->  fdt_reserved_mem_save_node
----->fdt_init_reserved_mem__fdt_scan_reserved_mem
      ----> __reserved_mem_init_node
*/


// 初始化 reserved_mem 和 reserved_mem_count
#define MAX_RESERVED_REGIONS    32
static struct reserved_mem reserved_mem[MAX_RESERVED_REGIONS];
static int reserved_mem_count;


//	@ drivers/of/fdt.c
/**
 * early_init_fdt_scan_reserved_mem() - create reserved memory regions
 *
 * This function grabs memory from early allocator for device exclusive use
 * defined in device tree structures. It should be called by arch specific code
 * once the early allocator (i.e. memblock) has been fully activated.
 */
void __init early_init_fdt_scan_reserved_mem(void)
{
	int n;
	u64 base, size;

	if (!initial_boot_params)
		return;

	/* Process header /memreserve/ fields */
	for (n = 0; ; n++) {
		fdt_get_mem_rsv(initial_boot_params, n, &base, &size);
		if (!size)
			break;
		early_init_dt_reserve_memory_arch(base, size, 0);
	}

	//
	of_scan_flat_dt(__fdt_scan_reserved_mem, NULL);

	//
	fdt_init_reserved_mem();
}


/**
 * fdt_scan_reserved_mem() - scan a single FDT node for reserved memory
 */
static int __init __fdt_scan_reserved_mem(unsigned long node, const char *uname, int depth, void *data)
{
    static int found;
    const char *status;
    int err;
    
    if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {
        if (__reserved_mem_check_root(node) != 0) {
            pr_err("Reserved memory: unsupported node format, ignoring\n");
            /* break scan */
            return 1;
        }
        found = 1;
        /* scan next node */
        return 0;
    } else if (!found) {
        /* scan next node */
        return 0;
    } else if (found && depth < 2) {
        /* scanning of /reserved-memory has been finished */
        return 1;
    }
    
    status = of_get_flat_dt_prop(node, "status", NULL);
    if (status && strcmp(status, "okay") != 0 && strcmp(status, "ok") != 0)
        return 0;
    
    err = __reserved_mem_reserve_reg(node, uname);
    if (err == -ENOENT && of_get_flat_dt_prop(node, "size", NULL))
        fdt_reserved_mem_save_node(node, uname, 0, 0);

    /* scan next node */
    return 0;
}


/**
 * res_mem_reserve_reg() - reserve all memory described in 'reg' property
 */
static int __init __reserved_mem_reserve_reg(unsigned long node, const char *uname)
{
    int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(__be32);
    phys_addr_t base, size;
    int len;
    const __be32 *prop;
    int nomap, first = 1;
    
    prop = of_get_flat_dt_prop(node, "reg", &len);
    if (!prop) 
        return -ENOENT;
    
    if (len && len % t_len != 0) {
        pr_err("Reserved memory: invalid reg property in '%s', skipping node.\n", uname);
        return -EINVAL;
    }
    
    nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;
    
    while (len >= t_len) {
        base = dt_mem_next_cell(dt_root_addr_cells, &prop);
        size = dt_mem_next_cell(dt_root_size_cells, &prop);
        
        if (size &&
            early_init_dt_reserve_memory_arch(base, size, nomap) == 0)
            pr_debug("Reserved memory: reserved region for node '%s': base %pa, size %ld MiB\n", uname, &base, (unsigned long)size / SZ_1M);
        else
            pr_info("Reserved memory: failed to reserve memory for node '%s': base %pa, size %ld MiB\n", uname, &base, (unsigned long)size / SZ_1M);
        
        len -= t_len;
        if (first) {
            // 这里初始化reserved_mem_count 和 reserved_mem[MAX_RESERVED_REGIONS] 
            fdt_reserved_mem_save_node(node, uname, base, size);
            first = 0;
        }
    }
    return 0;
}

int __init __weak early_init_dt_reserve_memory_arch(phys_addr_t base, phys_addr_t size, bool nomap)
{
    if (nomap)
        return memblock_remove(base, size);
        
    return memblock_reserve(base, size);
}


/**
 * res_mem_save_node() - save fdt node for second pass initialization
 */
void __init fdt_reserved_mem_save_node(unsigned long node, const char *uname, phys_addr_t base, phys_addr_t size)
{
    struct reserved_mem *rmem = &reserved_mem[reserved_mem_count];

    if (reserved_mem_count == ARRAY_SIZE(reserved_mem)) {
        pr_err("not enough space all defined regions.\n");
        return;
    }

    rmem->fdt_node = node;
    rmem->name = uname;
    rmem->base = base;
    rmem->size = size;

    reserved_mem_count++;                                                                                                                                                                                      
    return;
}




/**
 * fdt_init_reserved_mem - allocate and init all saved reserved memory regions
 */
void __init fdt_init_reserved_mem(void)
{
	int i;

	/* check for overlapping reserved regions */
	__rmem_check_for_overlap();

	for (i = 0; i < reserved_mem_count; i++) {
		struct reserved_mem *rmem = &reserved_mem[i];
		unsigned long node = rmem->fdt_node;
		int len;
		const __be32 *prop;
		int err = 0;

		prop = of_get_flat_dt_prop(node, "phandle", &len);
		if (!prop)
			prop = of_get_flat_dt_prop(node, "linux,phandle", &len);
		if (prop)
			rmem->phandle = of_read_number(prop, len/4);

		if (rmem->size == 0)
			err = __reserved_mem_alloc_size(node, rmem->name, &rmem->base, &rmem->size);

		if (err == 0)
			__reserved_mem_init_node(rmem);
	}
}

/**
 * res_mem_alloc_size() - allocate reserved memory described by 'size', 'align'
 *			  and 'alloc-ranges' properties
 */
static int __init __reserved_mem_alloc_size(unsigned long node,
	const char *uname, phys_addr_t *res_base, phys_addr_t *res_size)
{
	int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(__be32);
	phys_addr_t start = 0, end = 0;
	phys_addr_t base = 0, align = 0, size;
	int len;
	const __be32 *prop;
	int nomap;
	int ret;

	prop = of_get_flat_dt_prop(node, "size", &len);
	if (!prop)
		return -EINVAL;

	if (len != dt_root_size_cells * sizeof(__be32)) {
		pr_err("invalid size property in '%s' node.\n", uname);
		return -EINVAL;
	}
	size = dt_mem_next_cell(dt_root_size_cells, &prop);

	nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;

	prop = of_get_flat_dt_prop(node, "alignment", &len);
	if (prop) {
		if (len != dt_root_addr_cells * sizeof(__be32)) {
			pr_err("invalid alignment property in '%s' node.\n", uname);
			return -EINVAL;
		}
		align = dt_mem_next_cell(dt_root_addr_cells, &prop);
	}

	/* Need adjust the alignment to satisfy the CMA requirement */
	if (IS_ENABLED(CONFIG_CMA)
	    && of_flat_dt_is_compatible(node, "shared-dma-pool")
	    && of_get_flat_dt_prop(node, "reusable", NULL)
	    && !of_get_flat_dt_prop(node, "no-map", NULL)) {
		unsigned long order = max_t(unsigned long, MAX_ORDER - 1, pageblock_order);

		align = max(align, (phys_addr_t)PAGE_SIZE << order);
	}

	prop = of_get_flat_dt_prop(node, "alloc-ranges", &len);
	if (prop) {

		if (len % t_len != 0) {
			pr_err("invalid alloc-ranges property in '%s', skipping node.\n", uname);
			return -EINVAL;
		}

		base = 0;

		while (len > 0) {
			start = dt_mem_next_cell(dt_root_addr_cells, &prop);
			end = start + dt_mem_next_cell(dt_root_size_cells, &prop);

			ret = early_init_dt_alloc_reserved_memory_arch(node, size, align, start, end, nomap, &base);
			if (ret == 0) {
				pr_debug("allocated memory for '%s' node: base %pa, size %ld MiB\n",
					uname, &base,
					(unsigned long)size / SZ_1M);
				break;
			}
			len -= t_len;
		}

	} else {
		ret = early_init_dt_alloc_reserved_memory_arch(node, size, align, 0, 0, nomap, &base);
		if (ret == 0)
			pr_debug("allocated memory for '%s' node: base %pa, size %ld MiB\n",
				uname, &base, (unsigned long)size / SZ_1M);
	}

	if (base == 0) {
		pr_info("failed to allocate memory for node '%s'\n", uname);
		return -ENOMEM;
	}

	*res_base = base;
	*res_size = size;

	return 0;
}


int __init __weak early_init_dt_alloc_reserved_memory_arch(unsigned long node,
	phys_addr_t size, phys_addr_t align, phys_addr_t start, phys_addr_t end,
	bool nomap, phys_addr_t *res_base)
{
	phys_addr_t base;
	phys_addr_t highmem_start;

	highmem_start = __pa(high_memory - 1) + 1;

	/*
	 * We use __memblock_alloc_base() because memblock_alloc_base()
	 * panic()s on allocation failure.
	 */
	end = !end ? MEMBLOCK_ALLOC_ANYWHERE : end;
	base = __memblock_alloc_base(size, align, end);
	if (!base)
		return -ENOMEM;

	/*
	 * Check if the allocated region fits in to start..end window
	 */
	if (base < start) {
		memblock_free(base, size);
		return -ENOMEM;
	}

	/*
	 * Sanity check for the cma reserved region:If the reserved region
	 * crosses the low/high memory boundary, try to fix it up and then
	 * fall back to allocate the cma region from the low mememory space.
	 */
	// CONFIG_CMA 已经定义
	if (IS_ENABLED(CONFIG_CMA)
	    && of_flat_dt_is_compatible(node, "shared-dma-pool")
	    && of_get_flat_dt_prop(node, "reusable", NULL) && !nomap) {
		if (base < highmem_start && (base + size) > highmem_start) {
			memblock_free(base, size);
			base = memblock_alloc_range(size, align, start,
						    highmem_start,
						    MEMBLOCK_NONE);
			if (!base)
				return -ENOMEM;
		}
	}

	*res_base = base;
	if (nomap)
		return memblock_remove(base, size);
	return 0;
}


/**
 * res_mem_init_node() - call region specific reserved memory init code
 */
static int __init __reserved_mem_init_node(struct reserved_mem *rmem)
{
	extern const struct of_device_id __reservedmem_of_table[];
	const struct of_device_id *i;

	for (i = __reservedmem_of_table; i < &__rmem_of_table_sentinel; i++) {
		reservedmem_of_init_fn initfn = i->data;
		const char *compat = i->compatible;

		if (!of_flat_dt_is_compatible(rmem->fdt_node, compat))
			continue;

		// 举例 RESERVEDMEM_OF_DECLARE(dma, "shared-dma-pool", rmem_dma_setup);	中的 rmem_dma_setup 函数
		if (initfn(rmem) == 0) {
			pr_info("initialized node %s, compatible id %s\n", rmem->name, compat);
			return 0;
		}
	}
	return -ENOENT;
}