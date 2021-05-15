

// mm/mmap.c
SYSCALL_DEFINE1(brk, unsigned long, brk)
{
	unsigned long retval;
	unsigned long newbrk, oldbrk;
	// 
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *next;
	unsigned long min_brk;
	bool populate;
	//
	LIST_HEAD(uf);

	// 
	if (down_write_killable(&mm->mmap_sem))
		return -EINTR;

#ifdef CONFIG_COMPAT_BRK   // 225:# CONFIG_COMPAT_BRK is not set 
	/*
	 * CONFIG_COMPAT_BRK can still be overridden by setting
	 * randomize_va_space to 2, which will still cause mm->start_brk
	 * to be arbitrarily shifted
	 */
	if (current->brk_randomized)
		min_brk = mm->start_brk;
	else
		min_brk = mm->end_data;
#else
	// 执行这里，brk分配的空间是从数据段的顶部end_data到用户栈的底部
	// 动态分配空间是从进程的end_data开始，每次分配一块空间，就把这个边界往上推进一段；
	// 
	min_brk = mm->start_brk;
#endif
	if (brk < min_brk)
		goto out;

	/*
	 * Check against rlimit here. If this check is done later after the test
	 * of oldbrk with newbrk then it can escape the test and let the data
	 * segment grow beyond its set limit the in case where the limit is
	 * not page aligned -Ram Gupta
	 */
	if (check_data_rlimit(rlimit(RLIMIT_DATA), brk, mm->start_brk, mm->end_data, mm->start_data))
		goto out;

	// 都需要页对齐，
	newbrk = PAGE_ALIGN(brk);
	oldbrk = PAGE_ALIGN(mm->brk);
	if (oldbrk == newbrk)
		goto set_brk;

	/* Always allow shrinking brk. */
	if (brk <= mm->brk) {
		// do_munmap为用户空间收缩对应接口
		if (!do_munmap(mm, newbrk, oldbrk-newbrk, &uf))
			goto set_brk;
		goto out;
	}

	/* Check against existing mmap mappings. */
	next = find_vma(mm, oldbrk);
	// 
	if (next && newbrk + PAGE_SIZE > vm_start_gap(next))
		goto out;

	/* Ok, looks good - let it rip. */
	if (do_brk_flags(oldbrk, newbrk-oldbrk, 0, &uf) < 0)
		goto out;

set_brk:
	mm->brk = brk;
	populate = newbrk > oldbrk && (mm->def_flags & VM_LOCKED) != 0;
	up_write(&mm->mmap_sem);
	userfaultfd_unmap_complete(mm, &uf);
	if (populate)
		mm_populate(oldbrk, newbrk - oldbrk);
	return brk;

out:
	retval = mm->brk;
	up_write(&mm->mmap_sem);
	return retval;
}


//	@	arch/arm64/include/asm/current.h
struct task_struct;


// 对于arm64位平台，记录当前进程的task_struct地址是利用sp_el0寄存器，当内核执行进程切换时会吧当前要运行的进程task_struct地址记录到该寄存器中。
// 因此我们current查找task_struct时也很简单了，不再用通过sp和thread_info去定位了
/*
 * We don't use read_sysreg() as we want the compiler to cache the value where
 * possible.
 */
static __always_inline struct task_struct *get_current(void)
{
	unsigned long sp_el0;

	asm ("mrs %0, sp_el0" : "=r" (sp_el0));

	return (struct task_struct *)sp_el0;
}

#define current get_current()