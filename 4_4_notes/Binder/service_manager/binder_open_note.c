// 分析 bs = binder_open(128*1024);//128k
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	frameworks/native/cmds/servicemanager/binder.c
struct binder_state *binder_open(unsigned mapsize)
{
    struct binder_state *bs;

    bs = malloc(sizeof(*bs));
    if (!bs) {
        errno = ENOMEM;
        return 0;
    }

    bs->fd = open("/dev/binder", O_RDWR);
    if (bs->fd < 0) {
        fprintf(stderr,"binder: cannot open device (%s)\n",strerror(errno));
        goto fail_open;
    }

    bs->mapsize = mapsize;//128k
    bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
    if (bs->mapped == MAP_FAILED) {
        fprintf(stderr,"binder: cannot map device (%s)\n",strerror(errno));
        goto fail_map;
    }

  	/* TODO: check version */
    return bs;

fail_map:
    close(bs->fd);
fail_open:
    free(bs);
    return 0;
}


struct binder_state
{
    int fd;
    void *mapped;
    unsigned mapsize;
};


//分析	bs->fd = open("/dev/binder", O_RDWR);



//	kernel_imx/drivers/staging/android/binder.c
//	创建一个struct binder_proc数据结构来保存打开设备文件/dev/binder的进程的上下文信息

static int binder_open(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",current->group_leader->pid, current->pid);

	proc = kzalloc(sizeof(*proc), GFP_KERNEL); //每个进程创建一个binder_proc上下文信息
	if (proc == NULL)
		return -ENOMEM;

	get_task_struct(current);
	proc->tsk = current;
	INIT_LIST_HEAD(&proc->todo);
	init_waitqueue_head(&proc->wait);
	proc->default_priority = task_nice(current);

	binder_lock(__func__);

	binder_stats_created(BINDER_STAT_PROC);
	hlist_add_head(&proc->proc_node, &binder_procs);// 将创建的binder_proc添加进binder_procs链表
	proc->pid = current->group_leader->pid;
	INIT_LIST_HEAD(&proc->delivered_death);
	filp->private_data = proc; //这个进程上下文信息保存在打开文件结构struct file的私有数据成员变量private_data中

	binder_unlock(__func__);

	if (binder_debugfs_dir_entry_proc) {
		char strbuf[11];
		snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
		proc->debugfs_entry = debugfs_create_file(strbuf, S_IRUGO,binder_debugfs_dir_entry_proc, proc, &binder_proc_fops);
	}

	return 0;
}


// kernel_imx/drivers/staging/android/uapi/binder.h
struct binder_proc {
	struct hlist_node proc_node;
	struct rb_root threads; //threads树用来保存binder_proc进程内用于处理用户请求的线程，它的最大数量由max_threads来决定
	struct rb_root nodes;	//node树成用来保存binder_proc进程内的Binder实体
	//refs_by_desc树和refs_by_node树用来保存binder_proc进程内的Binder引用，即引用的其它进程的Binder实体，它分别用两种方式来组织红黑树，
        //一种是以句柄作来key值来组织，一种是以引用的实体节点的地址值作来key值来组织，它们都是表示同一样东西，只不过是为了内部查找方便而用两个红黑树来表示
	struct rb_root refs_by_desc;//以句柄作来key值来组织
	struct rb_root refs_by_node;//以引用的实体节点的地址值作来key值来组织
	int pid;
	struct vm_area_struct *vma;
	struct mm_struct *vma_vm_mm;
	struct task_struct *tsk;
	struct files_struct *files;
	struct hlist_node deferred_work_node;
	int deferred_work;
	void *buffer;
	ptrdiff_t user_buffer_offset;

	struct list_head buffers;
	struct rb_root free_buffers;
	struct rb_root allocated_buffers;
	size_t free_async_space;

	struct page **pages;
	size_t buffer_size;
	uint32_t buffer_free;
	struct list_head todo;
	wait_queue_head_t wait;
	struct binder_stats stats;
	struct list_head delivered_death;
	int max_threads;			//用户设置孵化线程数最大值
	int requested_threads;			//用户标识孵化线程数，其实内核通知用户创建孵化线程时被制1，线程孵化成功之后被设置为0
	int requested_threads_started;		//用于标记目前已经创建的孵化线程个数
	int ready_threads;			//用于标记忙碌的线程数
	long default_priority;
	struct dentry *debugfs_entry;
};


//bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);

static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct vm_struct *area;
	struct binder_proc *proc = filp->private_data;
	const char *failure_string;
	struct binder_buffer *buffer;

	if (proc->tsk != current)
		return -EINVAL;

	if ((vma->vm_end - vma->vm_start) > SZ_4M)
		vma->vm_end = vma->vm_start + SZ_4M;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));

	if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
		ret = -EPERM;
		failure_string = "bad vm_flags";
		goto err_bad_arg;
	}
	vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;

	mutex_lock(&binder_mmap_lock);
	if (proc->buffer) {
		ret = -EBUSY;
		failure_string = "already mapped";
		goto err_already_mapped;
	}

	area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
	if (area == NULL) {
		ret = -ENOMEM;
		failure_string = "get_vm_area";
		goto err_get_vm_area_failed;
	}
	proc->buffer = area->addr;
	proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
	mutex_unlock(&binder_mmap_lock);

#ifdef CONFIG_CPU_CACHE_VIPT
	if (cache_is_vipt_aliasing()) {
		while (CACHE_COLOUR((vma->vm_start ^ (uint32_t)proc->buffer))) {
			pr_info("binder_mmap: %d %lx-%lx maps %p bad alignment\n", proc->pid, vma->vm_start, vma->vm_end, proc->buffer);
			vma->vm_start += PAGE_SIZE;
		}
	}
#endif
	proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
	if (proc->pages == NULL) {
		ret = -ENOMEM;
		failure_string = "alloc page array";
		goto err_alloc_pages_failed;
	}
	proc->buffer_size = vma->vm_end - vma->vm_start;

	vma->vm_ops = &binder_vm_ops;
	vma->vm_private_data = proc;

	if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
		ret = -ENOMEM;
		failure_string = "alloc small buf";
		goto err_alloc_small_buf_failed;
	}
	buffer = proc->buffer;
	INIT_LIST_HEAD(&proc->buffers);
	list_add(&buffer->entry, &proc->buffers);
	buffer->free = 1;
	binder_insert_free_buffer(proc, buffer);
	proc->free_async_space = proc->buffer_size / 2;
	barrier();
	proc->files = get_files_struct(current);
	proc->vma = vma;
	proc->vma_vm_mm = vma->vm_mm;

	/*pr_info("binder_mmap: %d %lx-%lx maps %p\n",proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/
	return 0;

err_alloc_small_buf_failed:
	kfree(proc->pages);
	proc->pages = NULL;
err_alloc_pages_failed:
	mutex_lock(&binder_mmap_lock);
	vfree(proc->buffer);
	proc->buffer = NULL;
err_get_vm_area_failed:
err_already_mapped:
	mutex_unlock(&binder_mmap_lock);
err_bad_arg:
	pr_err("binder_mmap: %d %lx-%lx %s failed %d\n",proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
	return ret;
}

