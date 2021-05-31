

//	@	drivers/base/dma-coherent.c

RESERVEDMEM_OF_DECLARE(dma, "shared-dma-pool", rmem_dma_setup);	



static const struct reserved_mem_ops rmem_dma_ops = {
	.device_init	= rmem_dma_device_init,
	.device_release	= rmem_dma_device_release,
};


/*
rpmsg_dma@0x90400000 {
	compatible = "shared-dma-pool";
	no-map;
	reg = <0x00000000 0x90400000 0x00000000 0x00800000>;
	linux,phandle = <0x000000ef>;
	phandle = <0x000000ef>;
};
*/
static int __init rmem_dma_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;

	if (of_get_flat_dt_prop(node, "reusable", NULL))
		return -EINVAL;

#ifdef CONFIG_ARM
	if (!of_get_flat_dt_prop(node, "no-map", NULL)) {
		pr_err("Reserved memory: regions without no-map are not yet supported\n");
		return -EINVAL;
	}

	if (of_get_flat_dt_prop(node, "linux,dma-default", NULL)) {
		WARN(dma_reserved_default_memory,
		     "Reserved memory: region for default DMA coherent area is redefined\n");
		dma_reserved_default_memory = rmem;
	}
#endif

	rmem->ops = &rmem_dma_ops;
	
	// [    0.000000] Reserved memory: created DMA memory pool at 0x0000000090400000, size 8 MiB
	pr_info("Reserved memory: created DMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}