
// 缺页中断

//	@	arch/arm64/mm/fault.c



static const struct fault_info fault_info[] = {
	{ do_bad,		SIGBUS,  0,		"ttbr address size fault"	},
	{ do_bad,		SIGBUS,  0,		"level 1 address size fault"	},
	{ do_bad,		SIGBUS,  0,		"level 2 address size fault"	},
	{ do_bad,		SIGBUS,  0,		"level 3 address size fault"	},
	{ do_translation_fault,	SIGSEGV, SEGV_MAPERR,	"level 0 translation fault"	},
	{ do_translation_fault,	SIGSEGV, SEGV_MAPERR,	"level 1 translation fault"	},
	{ do_translation_fault,	SIGSEGV, SEGV_MAPERR,	"level 2 translation fault"	},
	{ do_translation_fault,	SIGSEGV, SEGV_MAPERR,	"level 3 translation fault"	},
	{ do_bad,		SIGBUS,  0,		"unknown 8"			},
	{ do_page_fault,	SIGSEGV, SEGV_ACCERR,	"level 1 access flag fault"	},
	{ do_page_fault,	SIGSEGV, SEGV_ACCERR,	"level 2 access flag fault"	},
	{ do_page_fault,	SIGSEGV, SEGV_ACCERR,	"level 3 access flag fault"	},
	{ do_bad,		SIGBUS,  0,		"unknown 12"			},
	{ do_page_fault,	SIGSEGV, SEGV_ACCERR,	"level 1 permission fault"	},
	{ do_page_fault,	SIGSEGV, SEGV_ACCERR,	"level 2 permission fault"	},
	{ do_page_fault,	SIGSEGV, SEGV_ACCERR,	"level 3 permission fault"	},
	{ do_sea,		SIGBUS,  0,		"synchronous external abort"	},
	{ do_bad,		SIGBUS,  0,		"unknown 17"			},
	{ do_bad,		SIGBUS,  0,		"unknown 18"			},
	{ do_bad,		SIGBUS,  0,		"unknown 19"			},
	{ do_sea,		SIGBUS,  0,		"level 0 (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"level 1 (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"level 2 (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"level 3 (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"synchronous parity or ECC error" },
	{ do_bad,		SIGBUS,  0,		"unknown 25"			},
	{ do_bad,		SIGBUS,  0,		"unknown 26"			},
	{ do_bad,		SIGBUS,  0,		"unknown 27"			},
	{ do_sea,		SIGBUS,  0,		"level 0 synchronous parity error (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"level 1 synchronous parity error (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"level 2 synchronous parity error (translation table walk)"	},
	{ do_sea,		SIGBUS,  0,		"level 3 synchronous parity error (translation table walk)"	},
	{ do_bad,		SIGBUS,  0,		"unknown 32"			},
	{ do_alignment_fault,	SIGBUS,  BUS_ADRALN,	"alignment fault"		},
	{ do_bad,		SIGBUS,  0,		"unknown 34"			},
	{ do_bad,		SIGBUS,  0,		"unknown 35"			},
	{ do_bad,		SIGBUS,  0,		"unknown 36"			},
	{ do_bad,		SIGBUS,  0,		"unknown 37"			},
	{ do_bad,		SIGBUS,  0,		"unknown 38"			},
	{ do_bad,		SIGBUS,  0,		"unknown 39"			},
	{ do_bad,		SIGBUS,  0,		"unknown 40"			},
	{ do_bad,		SIGBUS,  0,		"unknown 41"			},
	{ do_bad,		SIGBUS,  0,		"unknown 42"			},
	{ do_bad,		SIGBUS,  0,		"unknown 43"			},
	{ do_bad,		SIGBUS,  0,		"unknown 44"			},
	{ do_bad,		SIGBUS,  0,		"unknown 45"			},
	{ do_bad,		SIGBUS,  0,		"unknown 46"			},
	{ do_bad,		SIGBUS,  0,		"unknown 47"			},
	{ do_bad,		SIGBUS,  0,		"TLB conflict abort"		},
	{ do_bad,		SIGBUS,  0,		"unknown 49"			},
	{ do_bad,		SIGBUS,  0,		"unknown 50"			},
	{ do_bad,		SIGBUS,  0,		"unknown 51"			},
	{ do_bad,		SIGBUS,  0,		"implementation fault (lockdown abort)" },
	{ do_bad,		SIGBUS,  0,		"implementation fault (unsupported exclusive)" },
	{ do_bad,		SIGBUS,  0,		"unknown 54"			},
	{ do_bad,		SIGBUS,  0,		"unknown 55"			},
	{ do_bad,		SIGBUS,  0,		"unknown 56"			},
	{ do_bad,		SIGBUS,  0,		"unknown 57"			},
	{ do_bad,		SIGBUS,  0,		"unknown 58" 			},
	{ do_bad,		SIGBUS,  0,		"unknown 59"			},
	{ do_bad,		SIGBUS,  0,		"unknown 60"			},
	{ do_bad,		SIGBUS,  0,		"section domain fault"		},
	{ do_bad,		SIGBUS,  0,		"page domain fault"		},
	{ do_bad,		SIGBUS,  0,		"unknown 63"			},
};


/*
 * Dispatch a data abort to the relevant handler.
 */
asmlinkage void __exception do_mem_abort(unsigned long addr, unsigned int esr, struct pt_regs *regs)
{
	const struct fault_info *inf = esr_to_fault_info(esr);
	struct siginfo info;

	if (!inf->fn(addr, esr, regs))
		return;

	pr_alert("Unhandled fault: %s (0x%08x) at 0x%016lx\n", inf->name, esr, addr);

	mem_abort_decode(esr);

	info.si_signo = inf->sig;
	info.si_errno = 0;
	info.si_code  = inf->code;
	info.si_addr  = (void __user *)addr;
	arm64_notify_die("", regs, &info, esr);
}

static inline const struct fault_info *esr_to_fault_info(unsigned int esr)
{
	return fault_info + (esr & 63);
}


/*
 * First Level Translation Fault Handler
 *
 * We enter here because the first level page table doesn't contain a valid
 * entry for the address.
 *
 * If the address is in kernel space (>= TASK_SIZE), then we are probably
 * faulting in the vmalloc() area.
 *
 * If the init_task's first level page tables contains the relevant entry, we
 * copy the it to this task.  If not, we send the process a signal, fixup the
 * exception, or oops the kernel.
 *
 * NOTE! We MUST NOT take any locks for this case. We may be in an interrupt
 * or a critical region, and should only copy the information from the master
 * page table, nothing more.
 */
static int __kprobes do_translation_fault(unsigned long addr, unsigned int esr, struct pt_regs *regs)
{
	// 用户空间虚拟地址
	if (addr < TASK_SIZE)
		return do_page_fault(addr, esr, regs);

	do_bad_area(addr, esr, regs);
	return 0;
}





static int __kprobes do_page_fault(unsigned long addr, unsigned int esr, struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	int fault, sig, code, major = 0;
	unsigned long vm_flags = VM_READ | VM_WRITE;
	unsigned int mm_flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE;

	if (notify_page_fault(regs, esr))
		return 0;

	tsk = current;
	mm  = tsk->mm;

	/*
	 * If we're in an interrupt or have no user context, we must not take
	 * the fault.
	 */
	if (faulthandler_disabled() || !mm)
		goto no_context;

	if (user_mode(regs))
		mm_flags |= FAULT_FLAG_USER;

	if (is_el0_instruction_abort(esr)) {
		vm_flags = VM_EXEC;
	} else if ((esr & ESR_ELx_WNR) && !(esr & ESR_ELx_CM)) {
		vm_flags = VM_WRITE;
		mm_flags |= FAULT_FLAG_WRITE;
	}

	if (addr < TASK_SIZE && is_permission_fault(esr, regs, addr)) {
		/* regs->orig_addr_limit may be 0 if we entered from EL0 */
		if (regs->orig_addr_limit == KERNEL_DS)
			die("Accessing user space memory with fs=KERNEL_DS", regs, esr);

		if (is_el1_instruction_abort(esr))
			die("Attempting to execute userspace memory", regs, esr);

		if (!search_exception_tables(regs->pc))
			die("Accessing user space memory outside uaccess.h routines", regs, esr);
	}

	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, addr);

	/*
	 * As per x86, we may deadlock here. However, since the kernel only
	 * validly references user space from well defined areas of the code,
	 * we can bug out early if this is from code which shouldn't.
	 */
	if (!down_read_trylock(&mm->mmap_sem)) {
		if (!user_mode(regs) && !search_exception_tables(regs->pc))
			goto no_context;
retry:
		down_read(&mm->mmap_sem);
	} else {
		/*
		 * The above down_read_trylock() might have succeeded in which
		 * case, we'll have missed the might_sleep() from down_read().
		 */
		might_sleep();
#ifdef CONFIG_DEBUG_VM
		if (!user_mode(regs) && !search_exception_tables(regs->pc))
			goto no_context;
#endif
	}

	fault = __do_page_fault(mm, addr, mm_flags, vm_flags, tsk);
	major |= fault & VM_FAULT_MAJOR;

	if (fault & VM_FAULT_RETRY) {
		/*
		 * If we need to retry but a fatal signal is pending,
		 * handle the signal first. We do not need to release
		 * the mmap_sem because it would already be released
		 * in __lock_page_or_retry in mm/filemap.c.
		 */
		if (fatal_signal_pending(current)) {
			if (!user_mode(regs))
				goto no_context;
			return 0;
		}

		/*
		 * Clear FAULT_FLAG_ALLOW_RETRY to avoid any risk of
		 * starvation.
		 */
		if (mm_flags & FAULT_FLAG_ALLOW_RETRY) {
			mm_flags &= ~FAULT_FLAG_ALLOW_RETRY;
			mm_flags |= FAULT_FLAG_TRIED;
			goto retry;
		}
	}
	up_read(&mm->mmap_sem);

	/*
	 * Handle the "normal" (no error) case first.
	 */
	if (likely(!(fault & (VM_FAULT_ERROR | VM_FAULT_BADMAP |
			      VM_FAULT_BADACCESS)))) {
		/*
		 * Major/minor page fault accounting is only done
		 * once. If we go through a retry, it is extremely
		 * likely that the page will be found in page cache at
		 * that point.
		 */
		if (major) {
			tsk->maj_flt++;
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MAJ, 1, regs,
				      addr);
		} else {
			tsk->min_flt++;
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MIN, 1, regs,
				      addr);
		}

		return 0;
	}

	/*
	 * If we are in kernel mode at this point, we have no context to
	 * handle this fault with.
	 */
	if (!user_mode(regs))
		goto no_context;

	if (fault & VM_FAULT_OOM) {
		/*
		 * We ran out of memory, call the OOM killer, and return to
		 * userspace (which will retry the fault, or kill us if we got
		 * oom-killed).
		 */
		pagefault_out_of_memory();
		return 0;
	}

	if (fault & VM_FAULT_SIGBUS) {
		/*
		 * We had some memory, but were unable to successfully fix up
		 * this page fault.
		 */
		sig = SIGBUS;
		code = BUS_ADRERR;
	} else if (fault & (VM_FAULT_HWPOISON | VM_FAULT_HWPOISON_LARGE)) {
		sig = SIGBUS;
		code = BUS_MCEERR_AR;
	} else {
		/*
		 * Something tried to access memory that isn't in our memory
		 * map.
		 */
		sig = SIGSEGV;
		code = fault == VM_FAULT_BADACCESS ?
			SEGV_ACCERR : SEGV_MAPERR;
	}

	__do_user_fault(tsk, addr, esr, sig, code, regs, fault);
	return 0;

no_context:
	__do_kernel_fault(addr, esr, regs);
	return 0;
}


static int __do_page_fault(struct mm_struct *mm, unsigned long addr,
			   unsigned int mm_flags, unsigned long vm_flags,
			   struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	int fault;

	vma = find_vma(mm, addr);
	// 初始化没有找到vma区域，说明addr还没有在进程的地址空间中
	fault = VM_FAULT_BADMAP;
	if (unlikely(!vma))
		goto out;


	if (unlikely(vma->vm_start > addr))
		goto check_stack;

	/*
	 * Ok, we have a good vm_area for this memory access, so we can handle
	 * it.
	 */
good_area:
	/*
	 * Check that the permissions on the VMA allow for the fault which
	 * occurred.
	 */
	// 权限检查
	if (!(vma->vm_flags & vm_flags)) {
		fault = VM_FAULT_BADACCESS;
		goto out;
	}

	//	@	mm/memory.c
	// 重新建立物理页面到VMA的映射关系
	return handle_mm_fault(vma, addr & PAGE_MASK, mm_flags);

check_stack:
	if (vma->vm_flags & VM_GROWSDOWN && !expand_stack(vma, addr))
		goto good_area;
out:
	return fault;
}


/*
 * By the time we get here, we already hold the mm semaphore
 *
 * The mmap_sem may have been released depending on flags and our
 * return value.  See filemap_fault() and __lock_page_or_retry().
 */
int handle_mm_fault(struct vm_area_struct *vma, unsigned long address, unsigned int flags)
{
	int ret;

	// 设置当前线程状态 task_struct->state
	__set_current_state(TASK_RUNNING);

	// 
	count_vm_event(PGFAULT);
	count_memcg_event_mm(vma->vm_mm, PGFAULT);

	/* do counter updates before entering really critical section. */
	check_sync_rss_stat(current);

	if (!arch_vma_access_permitted(vma, flags & FAULT_FLAG_WRITE,
					    flags & FAULT_FLAG_INSTRUCTION,
					    flags & FAULT_FLAG_REMOTE))
		return VM_FAULT_SIGSEGV;

	/*
	 * Enable the memcg OOM handling for faults triggered in user
	 * space.  Kernel faults are handled more gracefully.
	 */
	if (flags & FAULT_FLAG_USER)
		mem_cgroup_oom_enable();

	if (unlikely(is_vm_hugetlb_page(vma))) {
		ret = hugetlb_fault(vma->vm_mm, vma, address, flags);
	} else {
		// 
		ret = __handle_mm_fault(vma, address, flags);
	}

	if (flags & FAULT_FLAG_USER) {
		mem_cgroup_oom_disable();
		/*
		 * The task may have entered a memcg OOM situation but
		 * if the allocation error was handled gracefully (no
		 * VM_FAULT_OOM), there is no need to kill anything.
		 * Just clean up the OOM state peacefully.
		 */
		if (task_in_memcg_oom(current) && !(ret & VM_FAULT_OOM))
			mem_cgroup_oom_synchronize(false);
	}

	return ret;
}


/*
 * Special states are those that do not use the normal wait-loop pattern. See
 * the comment with set_special_state().
 */
#define is_special_task_state(state)                            \
        ((state) & (__TASK_STOPPED | __TASK_TRACED | TASK_DEAD))

#define __set_current_state(state_value)                        \
        do {                                                    \
                WARN_ON_ONCE(is_special_task_state(state_value));\
                current->task_state_change = _THIS_IP_;         \
                current->state = (state_value);                 \
        } while (0)



/*
 * By the time we get here, we already hold the mm semaphore
 *
 * The mmap_sem may have been released depending on flags and our
 * return value.  See filemap_fault() and __lock_page_or_retry().
 */
static int __handle_mm_fault(struct vm_area_struct *vma, unsigned long address, unsigned int flags)
{
	struct vm_fault vmf = {
		.vma = vma,
		.address = address & PAGE_MASK,
		.flags = flags,
		// linear_page_index 
		.pgoff = linear_page_index(vma, address),
		// 
		.gfp_mask = __get_fault_gfp_mask(vma),
	};

	unsigned int dirty = flags & FAULT_FLAG_WRITE;
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgd;
	p4d_t *p4d;
	int ret;

	// @	arch/arm64/include/asm/pgtable.h
	// 查找页全局目录，获取地址对应的表项
	// imx8中返回 mm->pgd + address的48~40位
	pgd = pgd_offset(mm, address);

	// @ include/asm-generic/5level-fixup.h  #define p4d_alloc(mm, pgd, address)	(pgd)
	// 查找页四级目录表项，没有则创建
	// 在imx8中，直接返回pgd
	p4d = p4d_alloc(mm, pgd, address);
	if (!p4d)
		return VM_FAULT_OOM;

	// @	include/asm-generic/5level-fixup.h
	// 查找页上级目录表项，没有则创建
	// 在imx8中返回 p4d + address的39~31位
	vmf.pud = pud_alloc(mm, p4d, address);

	if (!vmf.pud)
		return VM_FAULT_OOM;

	// pud_none 判断vmf.pud是否为空，为空返回true
	// transparent_hugepage_enabled 
	if (pud_none(*vmf.pud) && transparent_hugepage_enabled(vma)) {
		ret = create_huge_pud(&vmf);
		if (!(ret & VM_FAULT_FALLBACK))
			return ret;
	} else {
		pud_t orig_pud = *vmf.pud;

		// barrier（屏障）
		barrier();
		if (pud_trans_huge(orig_pud) || pud_devmap(orig_pud)) {

			/* NUMA case for anonymous PUDs would go here */

			if (dirty && !pud_write(orig_pud)) {
				ret = wp_huge_pud(&vmf, orig_pud);
				if (!(ret & VM_FAULT_FALLBACK))
					return ret;
			} else {
				huge_pud_set_accessed(&vmf, orig_pud);
				return 0;
			}
		}
	}

	// 查找页中级目录表项，没有则创建
	vmf.pmd = pmd_alloc(mm, vmf.pud, address);
	if (!vmf.pmd)
		return VM_FAULT_OOM;

	// pmd_none 判断是否为空，如果为空返回true
	// transparent_hugepage_enabled 
	if (pmd_none(*vmf.pmd) && transparent_hugepage_enabled(vma)) {
		// 
		ret = create_huge_pmd(&vmf);
		if (!(ret & VM_FAULT_FALLBACK))
			return ret;
	} else {
		pmd_t orig_pmd = *vmf.pmd;

		barrier();
		if (unlikely(is_swap_pmd(orig_pmd))) {
			VM_BUG_ON(thp_migration_supported() && !is_pmd_migration_entry(orig_pmd));
			if (is_pmd_migration_entry(orig_pmd))
				pmd_migration_entry_wait(mm, vmf.pmd);
			return 0;
		}
		if (pmd_trans_huge(orig_pmd) || pmd_devmap(orig_pmd)) {
			if (pmd_protnone(orig_pmd) && vma_is_accessible(vma))
				return do_huge_pmd_numa_page(&vmf, orig_pmd);

			if (dirty && !pmd_write(orig_pmd)) {
				ret = wp_huge_pmd(&vmf, orig_pmd);
				if (!(ret & VM_FAULT_FALLBACK))
					return ret;
			} else {
				huge_pmd_set_accessed(&vmf, orig_pmd);
				return 0;
			}
		}
	}

	// 处理pte页表
	return handle_pte_fault(&vmf);
}


/////////////////////////////////////////////////////////////////////////////////////////
// @	arch/arm64/include/asm/pgtable.h
#define pgd_offset(mm, addr)	(pgd_offset_raw((mm)->pgd, (addr)))

#define pgd_offset_raw(pgd, addr)	((pgd) + pgd_index(addr))

// 得到48位中的高9位，作为偏移
#define pgd_index(addr)		(((addr) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1)) // ( (addr >> 39) & 1 1111 1111)

//	@	arch/arm64/include/asm/pgtable-hwdef.h
// CONFIG_PGTABLE_LEVELS=4
#define PGDIR_SHIFT		ARM64_HW_PGTABLE_LEVEL_SHIFT(4 - CONFIG_PGTABLE_LEVELS)  // 39

// arch/arm64/include/asm/pgtable-hwdef.h
#define ARM64_HW_PGTABLE_LEVEL_SHIFT(n)	( (PAGE_SHIFT - 3) * (4 - (n))  + 3)  // (12-3)*(4-0) + 3 = 39

//	@	arch/arm64/include/asm/page-def.h
#define PAGE_SHIFT		CONFIG_ARM64_PAGE_SHIFT   //CONFIG_ARM64_PAGE_SHIFT=12

//	@	arch/arm64/include/asm/pgtable-hwdef.h
#define PTRS_PER_PGD		(1 << (VA_BITS - PGDIR_SHIFT))   // (1<<(48-39))   =  1<<9   10 0000 0000
/////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////

#define pud_alloc(mm, p4d, address) \
	((unlikely(pgd_none(*(p4d))) && __pud_alloc(mm, p4d, address)) ? \
		NULL : pud_offset(p4d, address))

#define p4d_none(p4d)			0

//	@	arch/arm64/include/asm/pgtable.h
#define pud_offset(dir, addr)		((pud_t *)__va( pud_offset_phys((dir), (addr)) ))

//	@	arch/arm64/include/asm/pgtable.h
#define pud_offset_phys(dir, addr)	(pgd_page_paddr(*(dir)) + pud_index(addr) * sizeof(pud_t))

//	@	arch/arm64/include/asm/pgtable.h
#define pud_index(addr)		(((addr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))  // (addr >> 30) & 1 1111 1111

// arch/arm64/include/asm/pgtable-hwdef.h
#define PTRS_PER_PUD		PTRS_PER_PTE

//	arch/arm64/include/asm/pgtable-hwdef.h
#define PTRS_PER_PTE		(1 << (PAGE_SHIFT - 3))  // 1 << 9 // 10 0000 0000

//		arch/arm64/include/asm/pgtable-hwdef.h
#define PUD_SHIFT		ARM64_HW_PGTABLE_LEVEL_SHIFT(1)  // (12-3)*(4-1) + 3 = 30

//	@	arch/arm64/include/asm/memory.h
#define __va(x)			((void *)__phys_to_virt((phys_addr_t)(x)))

//	@	arch/arm64/include/asm/memory.h
#define __phys_to_virt(x)	((unsigned long)((x) - PHYS_OFFSET) | PAGE_OFFSET)
/////////////////////////////////////////////////////////////////////////////////////////


// 取地址pgd->pgd的低12位作为pud基地址
static inline phys_addr_t pgd_page_paddr(pgd_t pgd)
{
	//  arch/arm64/include/asm/pgtable-types.h:50:#define pgd_val(x)	((x).pgd)
	//	arch/arm64/include/asm/pgtable-hwdef.h:200:#define PHYS_MASK		((UL(1) << PHYS_MASK_SHIFT) - 1)
	// 	arch/arm64/include/asm/pgtable-hwdef.h:199:#define PHYS_MASK_SHIFT		(48)
	//	arch/arm64/include/asm/page-def.h:29:#define PAGE_MASK		(~(PAGE_SIZE-1))
	//	arch/arm64/include/asm/page-def.h:28:#define PAGE_SIZE		(_AC(1, UL) << PAGE_SHIFT)

	// PHYS_MASK 表示物理地址掩码
	// PAGE_MASK 表示页大小掩码
	return pgd_val(pgd) & PHYS_MASK & (s32)PAGE_MASK;
}

/////////////////////////////////////////////////////////////////////////////////////////
//	@	arch/arm64/include/asm/pgtable.h
#define pmd_none(pmd)		(!pmd_val(pmd))

//	@	arch/arm64/include/asm/pgtable-types.h
#define pmd_val(x)	((x).pmd)




/////////////////////////////////////////////////////////////////////////////////////////

/*
 * These routines also need to handle stuff like marking pages dirty
 * and/or accessed for architectures that don't do it in hardware (most
 * RISC architectures).  The early dirtying is also good on the i386.
 *
 * There is also a hook called "update_mmu_cache()" that architectures
 * with external mmu caches can use to update those (ie the Sparc or
 * PowerPC hashed page tables that act as extended TLBs).
 *
 * We enter with non-exclusive mmap_sem (to exclude vma changes, but allow
 * concurrent faults).
 *
 * The mmap_sem may have been released depending on flags and our return value.
 * See filemap_fault() and __lock_page_or_retry().
 */
//处理pte页表
static int handle_pte_fault(struct vm_fault *vmf)
{
	pte_t entry;

	if (unlikely(pmd_none(*vmf->pmd))) {
		/*
		 * Leave __pte_alloc() until later: because vm_ops->fault may
		 * want to allocate huge page, and if we expose page table
		 * for an instant, it will be difficult to retract from
		 * concurrent faults and from rmap lookups.
		 */
		vmf->pte = NULL;
	} else {
		/* See comment in pte_alloc_one_map() */
		if (pmd_devmap_trans_unstable(vmf->pmd))
			return 0;
		/*
		 * A regular pmd is established and it can't morph into a huge
		 * pmd from under us anymore at this point because we hold the
		 * mmap_sem read mode and khugepaged takes it in write mode.
		 * So now it's safe to run pte_offset_map().
		 */
		vmf->pte = pte_offset_map(vmf->pmd, vmf->address);
		vmf->orig_pte = *vmf->pte;

		/*
		 * some architectures can have larger ptes than wordsize,
		 * e.g.ppc44x-defconfig has CONFIG_PTE_64BIT=y and
		 * CONFIG_32BIT=y, so READ_ONCE or ACCESS_ONCE cannot guarantee
		 * atomic accesses.  The code below just needs a consistent
		 * view for the ifs and we later double check anyway with the
		 * ptl lock held. So here a barrier will do.
		 */
		barrier();
		if (pte_none(vmf->orig_pte)) {
			pte_unmap(vmf->pte);
			vmf->pte = NULL;
		}
	} 

	// pte是否存在
	if (!vmf->pte) {
		// vma_is_anonymous 返回 !vma->vm_ops（vm_operations_struct） // @	include/linux/mm.h
		if (vma_is_anonymous(vmf->vma)){
			// 匿名缺页处理
			return do_anonymous_page(vmf); // anonymous(匿名的)
		} else {
			// 
			return do_fault(vmf);
		}
	}

	// 页表项是否存在内存中
	if (!pte_present(vmf->orig_pte)) {
		// 不存在内存中
		return do_swap_page(vmf);
	}

	// 如果存在内存中
	if (pte_protnone(vmf->orig_pte) && vma_is_accessible(vmf->vma)) {
		return do_numa_page(vmf);
	}

	// 
	vmf->ptl = pte_lockptr(vmf->vma->vm_mm, vmf->pmd);
	spin_lock(vmf->ptl);
	entry = vmf->orig_pte;
	if (unlikely(!pte_same(*vmf->pte, entry)))
		goto unlock;

	if (vmf->flags & FAULT_FLAG_WRITE) {

		if (!pte_write(entry)) {
			// 写内存导致的缺页异常，但是没有写权位，写时复制
			// 如果代码走到这里，说明pte只有读权限，而又要写内存
			return do_wp_page(vmf);
		}
		entry = pte_mkdirty(entry);
	}
	entry = pte_mkyoung(entry);
	if (ptep_set_access_flags(vmf->vma, vmf->address, vmf->pte, entry, vmf->flags & FAULT_FLAG_WRITE)) {
		update_mmu_cache(vmf->vma, vmf->address, vmf->pte);
	} else {
		/*
		 * This is needed only for protection faults but the arch code
		 * is not yet telling us if this is a protection fault or not.
		 * This still avoids useless tlb flushes for .text page faults
		 * with threads.
		 */
		if (vmf->flags & FAULT_FLAG_WRITE)
			flush_tlb_fix_spurious_fault(vmf->vma, vmf->address);
	}
unlock:
	pte_unmap_unlock(vmf->pte, vmf->ptl);
	return 0;
}