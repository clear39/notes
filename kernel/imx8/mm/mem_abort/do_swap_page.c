//	@	mm/memory.c


/*
 * We enter with non-exclusive mmap_sem (to exclude vma changes,
 * but allow concurrent faults), and pte mapped but not yet locked.
 * We return with pte unmapped and unlocked.
 *
 * We return with the mmap_sem locked or unlocked in the same cases
 * as does filemap_fault().
 */
int do_swap_page(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct page *page = NULL, *swapcache;
	struct mem_cgroup *memcg;
	struct vma_swap_readahead swap_ra;
	swp_entry_t entry;
	pte_t pte;
	int locked;
	int exclusive = 0;
	int ret = 0;
	bool vma_readahead = swap_use_vma_readahead();

	if (vma_readahead)
		page = swap_readahead_detect(vmf, &swap_ra);

	if (!pte_unmap_same(vma->vm_mm, vmf->pmd, vmf->pte, vmf->orig_pte)) {
		if (page)
			put_page(page);
		goto out;
	}

	// 根据pte找到swap_entry，swap_entry和pte有一个对应关系
	entry = pte_to_swp_entry(vmf->orig_pte);
	if (unlikely(non_swap_entry(entry))) {
		if (is_migration_entry(entry)) {
			migration_entry_wait(vma->vm_mm, vmf->pmd, vmf->address);
		} else if (is_device_private_entry(entry)) {
			/*
			 * For un-addressable device memory we call the pgmap
			 * fault handler callback. The callback must migrate
			 * the page back to some CPU accessible page.
			 */
			ret = device_private_entry_fault(vma, vmf->address, entry, vmf->flags, vmf->pmd);
		} else if (is_hwpoison_entry(entry)) {
			ret = VM_FAULT_HWPOISON;
		} else {
			print_bad_pte(vma, vmf->address, vmf->orig_pte, NULL);
			ret = VM_FAULT_SIGBUS;
		}
		goto out;
	}
	delayacct_set_flag(DELAYACCT_PF_SWAPIN);

	if (!page) {
		// 根据 entry 从swap缓冲区中查找页，在swapcache里面寻找entry对应的page
		page = lookup_swap_cache(entry, vma_readahead ? vma : NULL, vmf->address);
	}

	// 页面没有找到
	if (!page) {
		if (vma_readahead) {
			page = do_swap_page_readahead(entry, GFP_HIGHUSER_MOVABLE, vmf, &swap_ra);
		} else {
			// 如果 swapcache 里面找不到就到swap area中找，分配新的内存页并从swap area中读取
			page = swapin_readahead(entry, GFP_HIGHUSER_MOVABLE, vma, vmf->address);
		}
		if (!page) {
			/*
			 * Back out if somebody else faulted in this pte
			 * while we released the pte lock.
			 */
			// 获取一个pte的entry，重新建立映射
			vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, vmf->address, &vmf->ptl);
			if (likely(pte_same(*vmf->pte, vmf->orig_pte)))
				ret = VM_FAULT_OOM;
			delayacct_clear_flag(DELAYACCT_PF_SWAPIN);
			goto unlock;
		}

		/* Had to read the page from swap area: Major fault */
		ret = VM_FAULT_MAJOR;
		count_vm_event(PGMAJFAULT);
		count_memcg_event_mm(vma->vm_mm, PGMAJFAULT);
	} else if (PageHWPoison(page)) {
		/*
		 * hwpoisoned dirty swapcache pages are kept for killing
		 * owner processes (which may be unknown at hwpoison time)
		 */
		ret = VM_FAULT_HWPOISON;
		delayacct_clear_flag(DELAYACCT_PF_SWAPIN);
		swapcache = page;
		goto out_release;
	}

	swapcache = page;
	locked = lock_page_or_retry(page, vma->vm_mm, vmf->flags);

	delayacct_clear_flag(DELAYACCT_PF_SWAPIN);
	if (!locked) {
		ret |= VM_FAULT_RETRY;
		goto out_release;
	}

	/*
	 * Make sure try_to_free_swap or reuse_swap_page or swapoff did not
	 * release the swapcache from under us.  The page pin, and pte_same
	 * test below, are not enough to exclude that.  Even if it is still
	 * swapcache, we need to check that the page's swap has not changed.
	 */
	if (unlikely(!PageSwapCache(page) || page_private(page) != entry.val))
		goto out_page;

	page = ksm_might_need_to_copy(page, vma, vmf->address);
	if (unlikely(!page)) {
		ret = VM_FAULT_OOM;
		page = swapcache;
		goto out_page;
	}

	if (mem_cgroup_try_charge(page, vma->vm_mm, GFP_KERNEL,
				&memcg, false)) {
		ret = VM_FAULT_OOM;
		goto out_page;
	}

	/*
	 * Back out if somebody else already faulted in this pte.
	 */
	vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, vmf->address, &vmf->ptl);
	if (unlikely(!pte_same(*vmf->pte, vmf->orig_pte)))
		goto out_nomap;

	if (unlikely(!PageUptodate(page))) {
		ret = VM_FAULT_SIGBUS;
		goto out_nomap;
	}

	/*
	 * The page isn't present yet, go ahead with the fault.
	 *
	 * Be careful about the sequence of operations here.
	 * To get its accounting right, reuse_swap_page() must be called
	 * while the page is counted on swap but not yet in mapcount i.e.
	 * before page_add_anon_rmap() and swap_free(); try_to_free_swap()
	 * must be called after the swap_free(), or it will never succeed.
	 */

	inc_mm_counter_fast(vma->vm_mm, MM_ANONPAGES);
	dec_mm_counter_fast(vma->vm_mm, MM_SWAPENTS);
	pte = mk_pte(page, vma->vm_page_prot);
	if ((vmf->flags & FAULT_FLAG_WRITE) && reuse_swap_page(page, NULL)) {
		pte = maybe_mkwrite(pte_mkdirty(pte), vma);
		vmf->flags &= ~FAULT_FLAG_WRITE;
		ret |= VM_FAULT_WRITE;
		exclusive = RMAP_EXCLUSIVE;
	}
	flush_icache_page(vma, page);
	if (pte_swp_soft_dirty(vmf->orig_pte))
		pte = pte_mksoft_dirty(pte);
	set_pte_at(vma->vm_mm, vmf->address, vmf->pte, pte);
	vmf->orig_pte = pte;
	if (page == swapcache) {
		do_page_add_anon_rmap(page, vma, vmf->address, exclusive);
		mem_cgroup_commit_charge(page, memcg, true, false);
		activate_page(page);
	} else { /* ksm created a completely new copy */
		page_add_new_anon_rmap(page, vma, vmf->address, false);
		mem_cgroup_commit_charge(page, memcg, false, false);
		lru_cache_add_active_or_unevictable(page, vma);
	}

	swap_free(entry);
	if (mem_cgroup_swap_full(page) ||
	    (vma->vm_flags & VM_LOCKED) || PageMlocked(page))
		try_to_free_swap(page);
	unlock_page(page);
	if (page != swapcache) {
		/*
		 * Hold the lock to avoid the swap entry to be reused
		 * until we take the PT lock for the pte_same() check
		 * (to avoid false positives from pte_same). For
		 * further safety release the lock after the swap_free
		 * so that the swap count won't change under a
		 * parallel locked swapcache.
		 */
		unlock_page(swapcache);
		put_page(swapcache);
	}

	if (vmf->flags & FAULT_FLAG_WRITE) {
		ret |= do_wp_page(vmf);
		if (ret & VM_FAULT_ERROR)
			ret &= VM_FAULT_ERROR;
		goto out;
	}

	/* No need to invalidate - it was non-present before */
	update_mmu_cache(vma, vmf->address, vmf->pte);
unlock:
	pte_unmap_unlock(vmf->pte, vmf->ptl);
out:
	return ret;
out_nomap:
	mem_cgroup_cancel_charge(page, memcg, false);
	pte_unmap_unlock(vmf->pte, vmf->ptl);
out_page:
	unlock_page(page);
out_release:
	put_page(page);
	if (page != swapcache) {
		unlock_page(swapcache);
		put_page(swapcache);
	}
	return ret;
}