

// @	include/linux/mm_types.h

// 进程地址空间在内核中使用 struct vm_area_struct 数据结构来描述，
// 也被称为进程地址空间或进程线性区间，由于这些地址空间归属于各个用户进程，所以在用户进程的 struct mm_struct 数据结构中有相应的成员

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
struct vm_area_struct {
	/* The first cache line has the info for VMA tree walking. */

	// 指定 VMA 在进程地址空间的起始地址和结束地址
	unsigned long vm_start;		/* Our start address within vm_mm. */
	unsigned long vm_end;		/* The first byte after our end address within vm_mm. */

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next, *vm_prev;

	// VMA作为一个节点加入红黑树中，每个进城的struct mm_struct数据结构中都有这样一个红黑树 mm->mm_rb
	struct rb_node vm_rb;

	/*
	 * Largest free memory gap in bytes to the left of this VMA.
	 * Either between this VMA and vma->vm_prev, or between one of the
	 * VMAs below us in the VMA rbtree and its ->vm_prev. This helps
	 * get_unmapped_area find a free area of the right size.
	 */
	unsigned long rb_subtree_gap;

	/* Second cache line starts here. */

	// 用于指向该VMA所属的进程 struct mm_struct 数据结构
	struct mm_struct *vm_mm;	/* The address space we belong to. */
	// VMA的访问权限
	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */

	// 描述该VMA的一组标志位
	unsigned long vm_flags;		/* Flags, see mm.h. */

	/*
	 * For areas with an address space and backing store,
	 * linkage into the address_space->i_mmap interval tree.
	 *
	 * For private anonymous mappings, a pointer to a null terminated string
	 * in the user process containing the name given to the vma, or NULL
	 * if unnamed.
	 */
	union {
		struct {
			struct rb_node rb;
			unsigned long rb_subtree_last;
		} shared;
		const char __user *anon_name;
	};

	/*
	 * A file's MAP_PRIVATE vma can be in both i_mmap tree and anon_vma
	 * list, after a COW of one of the file pages.	A MAP_SHARED vma
	 * can only be in the i_mmap tree.  An anonymous MAP_PRIVATE, stack
	 * or brk vma (with NULL file) can only be in an anon_vma list.
	 */
	// anon_vma_chain 和 anon_vma 用于管理RMAP反向映射
	struct list_head anon_vma_chain; /* Serialized by mmap_sem & page_table_lock */
	struct anon_vma *anon_vma;	/* Serialized by page_table_lock */

	/* Function pointers to deal with this struct. */
	// 指向许多方向的集合，这些方法用于在VMA中执行各种操作，通常用于文件映射
	const struct vm_operations_struct *vm_ops;

	/* Information about our backing store: */
	// 指定文件映射的偏移量，这个变量的单位不是Byte，而是页面的大小(PAGE_SIZE)
	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE units */
	// 指向file的实例，描述一个被映射的文件
	struct file * vm_file;		/* File we map to (can be NULL). */
	void * vm_private_data;		/* was vm_pte (shared mem) */

	atomic_long_t swap_readahead_info;
#ifndef CONFIG_MMU
	struct vm_region *vm_region;	/* NOMMU mapping region */
#endif


#ifdef CONFIG_NUMA // Arm platform CONFIG_NUMA is not set
	struct mempolicy *vm_policy;	/* NUMA policy for the VMA */
#endif
	struct vm_userfaultfd_ctx vm_userfaultfd_ctx;
} __randomize_layout;