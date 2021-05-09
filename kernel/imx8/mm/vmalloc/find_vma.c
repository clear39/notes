

//	 @	mm/mmap.c

//VMA(struct vm_area_struct) 查找有三个接口find_vma、find_vma_prev、find_vma_intersection


// 用于查找start_addr、end_addr和现存的VMA有重叠的一个VMA，基于find_vma来实现
static inline struct vm_area_struct * find_vma_intersection(struct mm_struct * mm, unsigned long start_addr, unsigned long end_addr)
{
	struct vm_area_struct * vma = find_vma(mm,start_addr);

	if (vma && end_addr <= vma->vm_start)
		vma = NULL;

	return vma;
}


/*
 * Same as find_vma, but also return a pointer to the previous VMA in *pprev.
 */
// find_vma_prev函数的逻辑和find_vma一样，但是返回VMA的钱继成员vma->vm_prev
struct vm_area_struct * find_vma_prev(struct mm_struct *mm, unsigned long addr, struct vm_area_struct **pprev)
{
	struct vm_area_struct *vma;

	vma = find_vma(mm, addr);
	if (vma) {
		*pprev = vma->vm_prev;
	} else {
		struct rb_node *rb_node = mm->mm_rb.rb_node;
		*pprev = NULL;
		while (rb_node) {
			*pprev = rb_entry(rb_node, struct vm_area_struct, vm_rb);
			rb_node = rb_node->rb_right;
		}
	}
	return vma;
}



// find_vma find_vma_prev

// addr 为虚拟地址，

/* Look up the first VMA which satisfies  addr < vm_end,  NULL if none. */
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
	struct rb_node *rb_node;
	struct vm_area_struct *vma;

	/* Check the cache first. */
	// 是内核中最近出现的一个查找VMA的优化方法，在task_struct结构中，
	// 有一个存放最近访问过的VMA的数组 vmacache[VMACACHE_SIZE],
	// 其中可以存放4个最近访问过的VMA,充分利用了局部性原理,
	// 如果vmacache中没有找到VMA，那么遍历这个用户进程的mm_rb红黑树，
	// 这个红黑树存放着该用户进程所有的VMA
	vma = vmacache_find(mm, addr);
	if (likely(vma))
		return vma;

	rb_node = mm->mm_rb.rb_node;

	while (rb_node) {
		struct vm_area_struct *tmp;

		tmp = rb_entry(rb_node, struct vm_area_struct, vm_rb);

		if (tmp->vm_end > addr) {
			vma = tmp;
			if (tmp->vm_start <= addr)
				break;
			rb_node = rb_node->rb_left;
		} else
			rb_node = rb_node->rb_right;
	}

	if (vma)
		vmacache_update(addr, vma);

	return vma;
}

//  @	mm/vmacache.c
struct vm_area_struct *vmacache_find(struct mm_struct *mm, unsigned long addr)
{
	int i;

	count_vm_vmacache_event(VMACACHE_FIND_CALLS);

	if (!vmacache_valid(mm))
		return NULL;

	for (i = 0; i < VMACACHE_SIZE; i++) {
		struct vm_area_struct *vma = current->vmacache.vmas[i];

		if (!vma)
			continue;

		if (WARN_ON_ONCE(vma->vm_mm != mm))
			break;

		if (vma->vm_start <= addr && vma->vm_end > addr) {
			count_vm_vmacache_event(VMACACHE_FIND_HITS);
			return vma;
		}
	}

	return NULL;
}



void vmacache_update(unsigned long addr, struct vm_area_struct *newvma)
{
	if (vmacache_valid_mm(newvma->vm_mm))
		current->vmacache.vmas[VMACACHE_HASH(addr)] = newvma;
}