

/**
imx_ion@0 {
	compatible = "imx-ion-pool";
	reg = <0x00000000 0xf8000000 0x00000000 0x08000000>;
	status = "disabled";
};
*/

//	@	drivers/staging/android/ion/ion_carveout_heap.c
RESERVEDMEM_OF_DECLARE(carveout, "imx-ion-pool", rmem_carveout_setup);

static int __init rmem_carveout_setup(struct reserved_mem *rmem)
{
	carveout_data.base = rmem->base;
	carveout_data.size = rmem->size;
	rmem->ops = &rmem_dma_ops;
	pr_info("Reserved memory: ION carveout pool at %pa, size %ld MiB\n", &rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

static const struct reserved_mem_ops rmem_dma_ops = {
	.device_init    = rmem_carveout_device_init,
	.device_release = rmem_carveout_device_release,
};
