

[    0.000000] Virtual kernel memory layout:


// 							 MODULES_VADDR        MODULES_END          (MODULES_END - MODULES_VADDR) >> 10
[    0.000000]     modules : 0xffff000000000000 - 0xffff000008000000   (   128 MB)

//							 VMALLOC_START        VMALLOC_END          (VMALLOC_END - VMALLOC_START) >> 20
[    0.000000]     vmalloc : 0xffff000008000000 - 0xffff7dffbfff0000   (129022 GB)
[    0.000000]       .text : 0xffff000008080000 - 0xffff000008ab0000   ( 10432 KB)
[    0.000000]     .rodata : 0xffff000008ab0000 - 0xffff000008e70000   (  3840 KB)
[    0.000000]       .init : 0xffff000008e70000 - 0xffff000009110000   (  2688 KB)
[    0.000000]       .data : 0xffff000009110000 - 0xffff00000925f200   (  1341 KB)
[    0.000000]        .bss : 0xffff00000925f200 - 0xffff0000092f3d70   (   595 KB)



[    0.000000]     fixed   : 0xffff7dfffe7fb000 - 0xffff7dfffec00000   (  4116 KB)

[    0.000000]     PCI I/O : 0xffff7dfffee00000 - 0xffff7dffffe00000   (    16 MB)

[    0.000000]     vmemmap : 0xffff7e0000000000 - 0xffff800000000000   (  2048 GB maximum)
[    0.000000]               0xffff7e0000008000 - 0xffff7e0021000000   (   527 MB actual)
[    0.000000]     memory  : 0xffff800000200000 - 0xffff800840000000   ( 33790 MB)




/*
 * mem_init() marks the free areas in the mem_map and tells us how much memory
 * is free.  This is done after various parts of the system have claimed their
 * memory after the kernel image.
 */
void __init mem_init(void)
{
	if (swiotlb_force == SWIOTLB_FORCE ||
	    max_pfn > (arm64_dma_phys_limit >> PAGE_SHIFT))
		swiotlb_init(1);
	else
		swiotlb_force = SWIOTLB_NO_FORCE;

	set_max_mapnr(pfn_to_page(max_pfn) - mem_map);

#ifndef CONFIG_SPARSEMEM_VMEMMAP
	free_unused_memmap();
#endif
	/* this will put all unused low memory onto the freelists */
	free_all_bootmem();

	kexec_reserve_crashkres_pages();

	mem_init_print_info(NULL);

#define MLK(b, t) 					b, t, ((t) - (b)) >> 10
#define MLM(b, t) 					b, t, ((t) - (b)) >> 20
#define MLG(b, t) 					b, t, ((t) - (b)) >> 30
#define MLK_ROUNDUP(b, t) 			b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

	pr_notice("Virtual kernel memory layout:\n");
#ifdef CONFIG_KASAN
	pr_notice("    kasan   : 0x%16lx - 0x%16lx   (%6ld GB)\n", MLG(KASAN_SHADOW_START, KASAN_SHADOW_END));
#endif
	// [    0.000000]     modules : 0xffff000000000000 - 0xffff000008000000   (   128 MB)
	pr_notice("    modules : 0x%16lx - 0x%16lx   (%6ld MB)\n", MLM(MODULES_VADDR, MODULES_END));



	// 其中 vmalloc 包含了 .text 、.rodata 、.init、.data、.bss
	// [    0.000000]     vmalloc : 0xffff000008000000 - 0xffff7dffbfff0000   (129022 GB)
	pr_notice("    vmalloc : 0x%16lx - 0x%16lx   (%6ld GB)\n", MLG(VMALLOC_START, VMALLOC_END));

	// [    0.000000]       .text : 0xffff000008080000 - 0xffff000008ab0000   ( 10432 KB)
	pr_notice("      .text : 0x%p" " - 0x%p" "   (%6ld KB)\n", MLK_ROUNDUP(_text, _etext));

	// [    0.000000]     .rodata : 0xffff000008ab0000 - 0xffff000008e70000   (  3840 KB)
	pr_notice("    .rodata : 0x%p" " - 0x%p" "   (%6ld KB)\n", MLK_ROUNDUP(__start_rodata, __init_begin));

	// [    0.000000]       .init : 0xffff000008e70000 - 0xffff000009110000   (  2688 KB)
	pr_notice("      .init : 0x%p" " - 0x%p" "   (%6ld KB)\n", MLK_ROUNDUP(__init_begin, __init_end));

	// [    0.000000]       .data : 0xffff000009110000 - 0xffff00000925f200   (  1341 KB)
	pr_notice("      .data : 0x%p" " - 0x%p" "   (%6ld KB)\n", MLK_ROUNDUP(_sdata, _edata));

	// [    0.000000]        .bss : 0xffff00000925f200 - 0xffff0000092f3d70   (   595 KB)
	pr_notice("       .bss : 0x%p" " - 0x%p" "   (%6ld KB)\n", MLK_ROUNDUP(__bss_start, __bss_stop));



	// [    0.000000]     fixed   : 0xffff7dfffe7fb000 - 0xffff7dfffec00000   (  4116 KB)
	pr_notice("    fixed   : 0x%16lx - 0x%16lx   (%6ld KB)\n", MLK(FIXADDR_START, FIXADDR_TOP));

	// [    0.000000]     PCI I/O : 0xffff7dfffee00000 - 0xffff7dffffe00000   (    16 MB)
	pr_notice("    PCI I/O : 0x%16lx - 0x%16lx   (%6ld MB)\n", MLM(PCI_IO_START, PCI_IO_END));

#ifdef CONFIG_SPARSEMEM_VMEMMAP
	// [    0.000000]     vmemmap : 0xffff7e0000000000 - 0xffff800000000000   (  2048 GB maximum)
	pr_notice("    vmemmap : 0x%16lx - 0x%16lx   (%6ld GB maximum)\n", MLG(VMEMMAP_START, VMEMMAP_START + VMEMMAP_SIZE));
	// [    0.000000]               0xffff7e0000008000 - 0xffff7e0021000000   (   527 MB actual)
	pr_notice("              0x%16lx - 0x%16lx   (%6ld MB actual)\n",
		MLM((unsigned long)phys_to_page(memblock_start_of_DRAM()), (unsigned long)virt_to_page(high_memory)));
#endif

	// [    0.000000]     memory  : 0xffff800000200000 - 0xffff800840000000   ( 33790 MB)
	pr_notice("    memory  : 0x%16lx - 0x%16lx   (%6ld MB)\n",
		MLM(__phys_to_virt(memblock_start_of_DRAM()), (unsigned long)high_memory));

#undef MLK
#undef MLM
#undef MLK_ROUNDUP

	/*
	 * Check boundaries twice: Some fundamental inconsistencies can be
	 * detected at build time already.
	 */
#ifdef CONFIG_COMPAT
	BUILD_BUG_ON(TASK_SIZE_32			> TASK_SIZE_64);
#endif

#ifdef CONFIG_SPARSEMEM_VMEMMAP  // CONFIG_SPARSEMEM_VMEMMAP 定义
	/*
	 * Make sure we chose the upper bound of sizeof(struct page)
	 * correctly when sizing the VMEMMAP array.
	 */
	BUILD_BUG_ON(sizeof(struct page) > (1 << STRUCT_PAGE_MAX_SHIFT));
#endif

	if (PAGE_SIZE >= 16384 && get_num_physpages() <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get anywhere without
		 * overcommit, so turn it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}




/* lowest address */
phys_addr_t __init_memblock memblock_start_of_DRAM(void)
{
	return memblock.memory.regions[0].base;
}
