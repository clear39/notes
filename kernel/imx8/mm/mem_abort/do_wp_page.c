

//	@	mm/memory.c
/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * Note that this routine assumes that the protection checks have been
 * done by the caller (the low-level page fault routine in most cases).
 * Thus we can safely just mark it writable once we've done any necessary
 * COW.
 *
 * We also mark the page dirty at this point even though the page will
 * change only once the write actually happens. This avoids a few races,
 * and potentially makes it more efficient.
 *
 * We enter with non-exclusive mmap_sem (to exclude vma changes,
 * but allow concurrent faults), with pte both mapped and locked.
 * We return with mmap_sem still held, but pte unmapped and unlocked.
 */
static int do_wp_page(struct vm_fault *vmf)
	__releases(vmf->ptl)
{
	struct vm_area_struct *vma = vmf->vma;

	vmf->page = vm_normal_page(vma, vmf->address, vmf->orig_pte);
	if (!vmf->page) {
		/*
		 * VM_MIXEDMAP !pfn_valid() case, or VM_SOFTDIRTY clear on a
		 * VM_PFNMAP VMA.
		 *
		 * We should not cow pages in a shared writeable mapping.
		 * Just mark the pages writable and/or call ops->pfn_mkwrite.
		 */
		if ((vma->vm_flags & (VM_WRITE|VM_SHARED)) == (VM_WRITE|VM_SHARED))
			return wp_pfn_shared(vmf);

		pte_unmap_unlock(vmf->pte, vmf->ptl);
		return wp_page_copy(vmf);
	}

	/*
	 * Take out anonymous pages first, anonymous shared vmas are
	 * not dirty accountable.
	 */
	if (PageAnon(vmf->page) && !PageKsm(vmf->page)) {
		int total_map_swapcount;
		if (!trylock_page(vmf->page)) {
			get_page(vmf->page);
			pte_unmap_unlock(vmf->pte, vmf->ptl);
			lock_page(vmf->page);
			vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, vmf->address, &vmf->ptl);
			if (!pte_same(*vmf->pte, vmf->orig_pte)) {
				unlock_page(vmf->page);
				pte_unmap_unlock(vmf->pte, vmf->ptl);
				put_page(vmf->page);
				return 0;
			}
			put_page(vmf->page);
		}
		if (reuse_swap_page(vmf->page, &total_map_swapcount)) {
			if (total_map_swapcount == 1) {
				/*
				 * The page is all ours. Move it to
				 * our anon_vma so the rmap code will
				 * not search our parent or siblings.
				 * Protected against the rmap code by
				 * the page lock.
				 */
				page_move_anon_rmap(vmf->page, vma);
			}
			unlock_page(vmf->page);
			wp_page_reuse(vmf);
			return VM_FAULT_WRITE;
		}
		unlock_page(vmf->page);
	} else if (unlikely((vma->vm_flags & (VM_WRITE|VM_SHARED)) == (VM_WRITE|VM_SHARED))) {
		return wp_page_shared(vmf);
	}

	/*
	 * Ok, we need to copy. Oh, well..
	 */
	get_page(vmf->page);

	pte_unmap_unlock(vmf->pte, vmf->ptl);
	// 私有可写，复制物理页，将虚拟页映射到物理页
	return wp_page_copy(vmf);
}














/*
 * Handle the case of a page which we actually need to copy to a new page.
 *
 * Called with mmap_sem locked and the old page referenced, but
 * without the ptl held.
 *
 * High level logic flow:
 *
 * - Allocate a page, copy the content of the old page to the new one.
 * - Handle book keeping and accounting - cgroups, mmu-notifiers, etc.
 * - Take the PTL. If the pte changed, bail out and release the allocated page
 * - If the pte is still the way we remember it, update the page table and all
 *   relevant references. This includes dropping the reference the page-table
 *   held to the old page, as well as updating the rmap.
 * - In any case, unlock the PTL and drop the reference we took to the old page.
 */
static int wp_page_copy(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *mm = vma->vm_mm;
	struct page *old_page = vmf->page;
	struct page *new_page = NULL;
	pte_t entry;
	int page_copied = 0;
	const unsigned long mmun_start = vmf->address & PAGE_MASK;
	const unsigned long mmun_end = mmun_start + PAGE_SIZE;
	struct mem_cgroup *memcg;

	if (unlikely(anon_vma_prepare(vma)))
		goto oom;

	if (is_zero_pfn(pte_pfn(vmf->orig_pte))) {
		new_page = alloc_zeroed_user_highpage_movable(vma, vmf->address);
		if (!new_page)
			goto oom;
	} else {
		new_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, vmf->address);
		if (!new_page)
			goto oom;
		cow_user_page(new_page, old_page, vmf->address, vma);
	}

	if (mem_cgroup_try_charge(new_page, mm, GFP_KERNEL, &memcg, false))
		goto oom_free_new;

	__SetPageUptodate(new_page);

	mmu_notifier_invalidate_range_start(mm, mmun_start, mmun_end);

	/*
	 * Re-check the pte - we dropped the lock
	 */
	vmf->pte = pte_offset_map_lock(mm, vmf->pmd, vmf->address, &vmf->ptl);
	if (likely(pte_same(*vmf->pte, vmf->orig_pte))) {
		if (old_page) {
			if (!PageAnon(old_page)) {
				dec_mm_counter_fast(mm, mm_counter_file(old_page));
				inc_mm_counter_fast(mm, MM_ANONPAGES);
			}
		} else {
			inc_mm_counter_fast(mm, MM_ANONPAGES);
		}
		flush_cache_page(vma, vmf->address, pte_pfn(vmf->orig_pte));
		entry = mk_pte(new_page, vma->vm_page_prot);
		entry = maybe_mkwrite(pte_mkdirty(entry), vma);
		/*
		 * Clear the pte entry and flush it first, before updating the
		 * pte with the new entry. This will avoid a race condition
		 * seen in the presence of one thread doing SMC and another
		 * thread doing COW.
		 */
		ptep_clear_flush_notify(vma, vmf->address, vmf->pte);
		page_add_new_anon_rmap(new_page, vma, vmf->address, false);
		mem_cgroup_commit_charge(new_page, memcg, false, false);
		lru_cache_add_active_or_unevictable(new_page, vma);
		/*
		 * We call the notify macro here because, when using secondary
		 * mmu page tables (such as kvm shadow page tables), we want the
		 * new page to be mapped directly into the secondary page table.
		 */
		set_pte_at_notify(mm, vmf->address, vmf->pte, entry);
		update_mmu_cache(vma, vmf->address, vmf->pte);
		if (old_page) {
			/*
			 * Only after switching the pte to the new page may
			 * we remove the mapcount here. Otherwise another
			 * process may come and find the rmap count decremented
			 * before the pte is switched to the new page, and
			 * "reuse" the old page writing into it while our pte
			 * here still points into it and can be read by other
			 * threads.
			 *
			 * The critical issue is to order this
			 * page_remove_rmap with the ptp_clear_flush above.
			 * Those stores are ordered by (if nothing else,)
			 * the barrier present in the atomic_add_negative
			 * in page_remove_rmap.
			 *
			 * Then the TLB flush in ptep_clear_flush ensures that
			 * no process can access the old page before the
			 * decremented mapcount is visible. And the old page
			 * cannot be reused until after the decremented
			 * mapcount is visible. So transitively, TLBs to
			 * old page will be flushed before it can be reused.
			 */
			page_remove_rmap(old_page, false);
		}

		/* Free the old page.. */
		new_page = old_page;
		page_copied = 1;
	} else {
		mem_cgroup_cancel_charge(new_page, memcg, false);
	}

	if (new_page)
		put_page(new_page);

	pte_unmap_unlock(vmf->pte, vmf->ptl);
	mmu_notifier_invalidate_range_end(mm, mmun_start, mmun_end);
	if (old_page) {
		/*
		 * Don't let another task, with possibly unlocked vma,
		 * keep the mlocked page.
		 */
		if (page_copied && (vma->vm_flags & VM_LOCKED)) {
			lock_page(old_page);	/* LRU manipulation */
			if (PageMlocked(old_page))
				munlock_vma_page(old_page);
			unlock_page(old_page);
		}
		put_page(old_page);
	}
	return page_copied ? VM_FAULT_WRITE : 0;
oom_free_new:
	put_page(new_page);
oom:
	if (old_page)
		put_page(old_page);
	return VM_FAULT_OOM;
}