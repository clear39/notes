
// @ include/linux/mm_types.h

// 该数据结构是描述进程内存管理的核心数据结构，该数据结构也提供了管理VMA(struct vm_area_struct) 所需要的信息

struct mm_struct {
	// 
	struct vm_area_struct *mmap;		/* list of VMAs */

	// 红黑树的根节点，用来链接进程地址空间struct vm_area_struct中的vm_rb
	// 每一个进程有一棵VMA的红黑树
	struct rb_root mm_rb;
	u64 vmacache_seqnum;                   /* per-thread vmacache */
#ifdef CONFIG_MMU
	unsigned long (*get_unmapped_area) (struct file *filp,
				unsigned long addr, unsigned long len,
				unsigned long pgoff, unsigned long flags);
#endif
	unsigned long mmap_base;		/* base of mmap area */
	unsigned long mmap_legacy_base;         /* base of mmap area in bottom-up allocations */

#ifdef CONFIG_HAVE_ARCH_COMPAT_MMAP_BASES
	/* Base adresses for compatible mmap() */
	unsigned long mmap_compat_base;
	unsigned long mmap_compat_legacy_base;
#endif

	unsigned long task_size;		/* size of task vm space */
	unsigned long highest_vm_end;		/* highest vma end address */
	pgd_t * pgd;

	/**
	 * @mm_users: The number of users including userspace.
	 *
	 * Use mmget()/mmget_not_zero()/mmput() to modify. When this drops
	 * to 0 (i.e. when the task exits and there are no other temporary
	 * reference holders), we also release a reference on @mm_count
	 * (which may then free the &struct mm_struct if @mm_count also
	 * drops to 0).
	 */
	atomic_t mm_users;

	/**
	 * @mm_count: The number of references to &struct mm_struct
	 * (@mm_users count as 1).
	 *
	 * Use mmgrab()/mmdrop() to modify. When this drops to 0, the
	 * &struct mm_struct is freed.
	 */
	atomic_t mm_count;

	atomic_long_t nr_ptes;			/* PTE page table pages */

#if CONFIG_PGTABLE_LEVELS > 2
	atomic_long_t nr_pmds;			/* PMD page table pages */
#endif

	int map_count;				/* number of VMAs */

	spinlock_t page_table_lock;		/* Protects page tables and some counters */
	struct rw_semaphore mmap_sem;

	struct list_head mmlist;		/* List of maybe swapped mm's.	These are globally strung
						 * together off init_mm.mmlist, and are protected
						 * by mmlist_lock
						 */


	unsigned long hiwater_rss;	/* High-watermark of RSS usage */
	unsigned long hiwater_vm;	/* High-water virtual memory usage */

	unsigned long total_vm;		/* Total pages mapped */
	unsigned long locked_vm;	/* Pages that have PG_mlocked set */
	unsigned long pinned_vm;	/* Refcount permanently increased */
	unsigned long data_vm;		/* VM_WRITE & ~VM_SHARED & ~VM_STACK */
	unsigned long exec_vm;		/* VM_EXEC & ~VM_WRITE & ~VM_STACK */
	unsigned long stack_vm;		/* VM_STACK */
	unsigned long def_flags;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack;
	unsigned long arg_start, arg_end, env_start, env_end;

	unsigned long saved_auxv[AT_VECTOR_SIZE]; /* for /proc/PID/auxv */

	/*
	 * Special counters, in some configurations protected by the
	 * page_table_lock, in other configurations by being atomic.
	 */
	struct mm_rss_stat rss_stat;

	struct linux_binfmt *binfmt;

	cpumask_var_t cpu_vm_mask_var;

	/* Architecture-specific MM context */
	mm_context_t context;

	unsigned long flags; /* Must use atomic bitops to access the bits */

	struct core_state *core_state; /* coredumping support */
#ifdef CONFIG_MEMBARRIER
	atomic_t membarrier_state;
#endif

#ifdef CONFIG_AIO
	spinlock_t			ioctx_lock;
	struct kioctx_table __rcu	*ioctx_table;
#endif
	
#ifdef CONFIG_MEMCG
	/*
	 * "owner" points to a task that is regarded as the canonical
	 * user/owner of this mm. All of the following must be true in
	 * order for it to be changed:
	 *
	 * current == mm->owner
	 * current->mm != mm
	 * new_owner->mm == mm
	 * new_owner->alloc_lock is held
	 */
	struct task_struct __rcu *owner;
#endif
	struct user_namespace *user_ns;

	/* store ref to file /proc/<pid>/exe symlink points to */
	struct file __rcu *exe_file;
#ifdef CONFIG_MMU_NOTIFIER
	struct mmu_notifier_mm *mmu_notifier_mm;
#endif
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && !USE_SPLIT_PMD_PTLOCKS
	pgtable_t pmd_huge_pte; /* protected by page_table_lock */
#endif
#ifdef CONFIG_CPUMASK_OFFSTACK
	struct cpumask cpumask_allocation;
#endif
#ifdef CONFIG_NUMA_BALANCING
	/*
	 * numa_next_scan is the next time that the PTEs will be marked
	 * pte_numa. NUMA hinting faults will gather statistics and migrate
	 * pages to new nodes if necessary.
	 */
	unsigned long numa_next_scan;

	/* Restart point for scanning and setting pte_numa */
	unsigned long numa_scan_offset;

	/* numa_scan_seq prevents two threads setting pte_numa */
	int numa_scan_seq;
#endif
	/*
	 * An operation with batched TLB flushing is going on. Anything that
	 * can move process memory needs to flush the TLB when moving a
	 * PROT_NONE or PROT_NUMA mapped page.
	 */
	atomic_t tlb_flush_pending;
#ifdef CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH
	/* See flush_tlb_batched_pending() */
	bool tlb_flush_batched;
#endif
	struct uprobes_state uprobes_state;
#ifdef CONFIG_HUGETLB_PAGE
	atomic_long_t hugetlb_usage;
#endif
	struct work_struct async_put_work;

#if IS_ENABLED(CONFIG_HMM)
	/* HMM needs to track a few things per mm */
	struct hmm *hmm;
#endif
} __randomize_layout;
