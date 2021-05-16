
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




RESERVEDMEM_OF_DECLARE(cma, "shared-dma-pool", rmem_cma_setup);



static int __init rmem_cma_setup(struct reserved_mem *rmem)
{
	phys_addr_t align = PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	phys_addr_t mask = align - 1;
	unsigned long node = rmem->fdt_node;
	struct cma *cma;
	int err;

	if (!of_get_flat_dt_prop(node, "reusable", NULL) ||
	    of_get_flat_dt_prop(node, "no-map", NULL))
		return -EINVAL;

	if ((rmem->base & mask) || (rmem->size & mask)) {
		pr_err("Reserved memory: incorrect alignment of CMA region\n");
		return -EINVAL;
	}

	err = cma_init_reserved_mem(rmem->base, rmem->size, 0, rmem->name, &cma);
	if (err) {
		pr_err("Reserved memory: unable to setup CMA region\n");
		return err;
	}
	/* Architecture specific contiguous memory fixup. */
	dma_contiguous_early_fixup(rmem->base, rmem->size);

	if (of_get_flat_dt_prop(node, "linux,cma-default", NULL))
		dma_contiguous_set_default(cma);

	rmem->ops = &rmem_cma_ops;
	rmem->priv = cma;

	pr_info("Reserved memory: created CMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}
