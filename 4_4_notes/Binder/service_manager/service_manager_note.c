//	frameworks/native/cmds/servicemanager/service_manager.c
int main(int argc, char **argv)
{
    struct binder_state *bs;
    // BINDER_SERVICE_MANAGER定义frameworks/base/cmds/servicemanager/binder.h文件中
   //frameworks/native/cmds/servicemanager/binder.h:55:#define BINDER_SERVICE_MANAGER ((void*) 0)
    void *svcmgr = BINDER_SERVICE_MANAGER;  //BINDER_SERVICE_MANAGER  == ((void*) 0)

    bs = binder_open(128*1024);//128k

   //参考binder_become_context_manager_note.c 文档分析
    if (binder_become_context_manager(bs)) {
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

    svcmgr_handle = svcmgr;

    //这里特别注意：svcmgr_handler 和上一行的svcmgr_handle多一个字母r，最开始看错了，郁闷
    binder_loop(bs, svcmgr_handler);
    return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
int binder_become_context_manager(struct binder_state *bs)
{
    return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    unsigned readbuf[32];

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;
    
    readbuf[0] = BC_ENTER_LOOPER;


    
    binder_write(bs, readbuf, sizeof(unsigned));

    for (;;) {

	//这里是请求返回数据
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (unsigned) readbuf;


	/*
	bwr.write_size = 0;
    	bwr.write_consumed = 0;
    	bwr.write_buffer = 0;
	readbuf[0] = BC_ENTER_LOOPER;
 	bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (unsigned) readbuf;
	*/
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

        if (res < 0) {
            ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }

        res = binder_parse(bs, 0, readbuf, bwr.read_consumed, func);
        if (res == 0) {
            ALOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}


/*
data == BC_ENTER_LOOPER
*/
int binder_write(struct binder_state *bs, void *data, unsigned len)
{
    //注意这里是临时变量
    struct binder_write_read bwr;
    int res;
    bwr.write_size = len;
    bwr.write_consumed = 0;
    bwr.write_buffer = (unsigned) data;

    //没有返回值
    bwr.read_size = 0;
    bwr.read_consumed = 0;
    bwr.read_buffer = 0;


/*
	bwr.write_size = len;
    	bwr.write_consumed = 0;
    	bwr.write_buffer = BC_ENTER_LOOPER;
 	bwr.read_size = 0;
        bwr.read_consumed = 0;
        bwr.read_buffer = 0;
*/
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
    if (res < 0) {
        fprintf(stderr,"binder_write: ioctl failed (%s)\n",strerror(errno));
    }
    return res;
}


int binder_parse(struct binder_state *bs, struct binder_io *bio,uint32_t *ptr, uint32_t size, binder_handler func)
{
    int r = 1;
    uint32_t *end = ptr + (size / 4);

    while (ptr < end) {
        uint32_t cmd = *ptr++;
#if TRACE
        fprintf(stderr,"%s:\n", cmd_name(cmd));
#endif
        switch(cmd) {
        case BR_NOOP:
            break;
        case BR_TRANSACTION_COMPLETE:
            break;
        case BR_INCREFS:
        case BR_ACQUIRE:
        case BR_RELEASE:
        case BR_DECREFS:
#if TRACE
            fprintf(stderr,"  %08x %08x\n", ptr[0], ptr[1]);
#endif
            ptr += 2;
            break;
        case BR_TRANSACTION: {
            struct binder_txn *txn = (void *) ptr;
            if ((end - ptr) * sizeof(uint32_t) < sizeof(struct binder_txn)) {
                ALOGE("parse: txn too small!\n");
                return -1;
            }
            binder_dump_txn(txn);//用于数据调试打印，release没有作用
            if (func) {//func = svcmgr_handler	
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, txn);
                res = func(bs, txn, &msg, &reply);//svcmgr_handler(bs, txn, &msg, &reply);
                binder_send_reply(bs, &reply, txn->data, res);

            }
            ptr += sizeof(*txn) / sizeof(uint32_t);
            break;
        }
        case BR_REPLY: {
            struct binder_txn *txn = (void*) ptr;
            if ((end - ptr) * sizeof(uint32_t) < sizeof(struct binder_txn)) {
                ALOGE("parse: reply too small!\n");
                return -1;
            }
            binder_dump_txn(txn);//用于数据调试打印，release没有作用
            if (bio) { // bio
                bio_init_from_txn(bio, txn);
                bio = 0;
            } else {
                    /* todo FREE BUFFER */
            }
            ptr += (sizeof(*txn) / sizeof(uint32_t));
            r = 0;
            break;
        }
        case BR_DEAD_BINDER: {
            struct binder_death *death = (void*) *ptr++;
            death->func(bs, death->ptr);
            break;
        }
        case BR_FAILED_REPLY:
            r = -1;
            break;
        case BR_DEAD_REPLY:
            r = -1;
            break;
        default:
            ALOGE("parse: OOPS %d\n", cmd);
            return -1;
        }
    }

    return r;
}


//	bio_init(&reply, rdata, sizeof(rdata), 4);
void bio_init(struct binder_io *bio, void *data,uint32_t maxdata, uint32_t maxoffs)
{
    uint32_t n = maxoffs * sizeof(uint32_t);

    if (n > maxdata) {
        bio->flags = BIO_F_OVERFLOW;
        bio->data_avail = 0;
        bio->offs_avail = 0;
        return;
    }

    bio->data = bio->data0 = (char *) data + n;
    bio->offs = bio->offs0 = data;
    bio->data_avail = maxdata - n;
    bio->offs_avail = maxoffs;
    bio->flags = 0;
}


//	bio_init_from_txn(&msg, txn);
void bio_init_from_txn(struct binder_io *bio, struct binder_txn *txn)
{
    bio->data = bio->data0 = txn->data;
    bio->offs = bio->offs0 = txn->offs;
    bio->data_avail = txn->data_size;
    bio->offs_avail = txn->offs_size / 4;
    bio->flags = BIO_F_SHARED;
}




//		 res = func(bs, txn, &msg, &reply);//svcmgr_handler(bs, txn, &msg, &reply);
int svcmgr_handler(struct binder_state *bs,
                   struct binder_txn *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    unsigned len;
    void *ptr;
    uint32_t strict_policy;
    int allow_isolated;

//    ALOGI("target=%p code=%d pid=%d uid=%d\n",txn->target, txn->code, txn->sender_pid, txn->sender_euid);

    if (txn->target != svcmgr_handle)
        return -1;

    // Equivalent to Parcel::enforceInterface(), reading the RPC
    // header with the strict mode policy mask and the interface name.
    // Note that we ignore the strict_policy and don't propagate it
    // further (since we do no outbound RPCs anyway).
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr,"invalid id %s\n", str8(s));
        return -1;
    }

    switch(txn->code) {
    case SVC_MGR_GET_SERVICE:
    case SVC_MGR_CHECK_SERVICE:
        s = bio_get_string16(msg, &len);
        ptr = do_find_service(bs, s, len, txn->sender_euid);
        if (!ptr)
            break;
        bio_put_ref(reply, ptr);
        return 0;

    case SVC_MGR_ADD_SERVICE:
        s = bio_get_string16(msg, &len);
        ptr = bio_get_ref(msg);
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        if (do_add_service(bs, s, len, ptr, txn->sender_euid, allow_isolated))//分析
            return -1;
        break;

    case SVC_MGR_LIST_SERVICES: {
        unsigned n = bio_get_uint32(msg);

        si = svclist;
        while ((n-- > 0) && si)
            si = si->next;
        if (si) {
            bio_put_string16(reply, si->name);
            return 0;
        }
        return -1;
    }
    default:
        ALOGE("unknown code %d\n", txn->code);
        return -1;
    }

    bio_put_uint32(reply, 0);
    return 0;
}


int do_add_service(struct binder_state *bs,uint16_t *s, unsigned len,void *ptr, unsigned uid, int allow_isolated)
{
    struct svcinfo *si;
    //ALOGI("add_service('%s',%p,%s) uid=%d\n", str8(s), ptr,allow_isolated ? "allow_isolated" : "!allow_isolated", uid);

    if (!ptr || (len == 0) || (len > 127))
        return -1;

    if (!svc_can_register(uid, s)) { //分析
        ALOGE("add_service('%s',%p) uid=%d - PERMISSION DENIED\n",str8(s), ptr, uid);
        return -1;
    }

    si = find_svc(s, len);//分析
    if (si) {
        if (si->ptr) {
            ALOGE("add_service('%s',%p) uid=%d - ALREADY REGISTERED, OVERRIDE\n",str8(s), ptr, uid);
            svcinfo_death(bs, si);
        }
        si->ptr = ptr;
    } else {
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        if (!si) {
            ALOGE("add_service('%s',%p) uid=%d - OUT OF MEMORY\n",str8(s), ptr, uid);
            return -1;
        }
        si->ptr = ptr;
        si->len = len;
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        si->next = svclist;
        svclist = si;
    }

    binder_acquire(bs, ptr);//分析
    binder_link_to_death(bs, ptr, &si->death);
    return 0;
}


//通过用户id判断权限，是否可以添加服务
int svc_can_register(unsigned uid, uint16_t *name)
{
    unsigned n;
    
    if ((uid == 0) || (uid == AID_SYSTEM))
        return 1;

    for (n = 0; n < sizeof(allowed) / sizeof(allowed[0]); n++)
        if ((uid == allowed[n].uid) && str16eq(name, allowed[n].name))
            return 1;

    return 0;
}

//查找service是否已经注册
struct svcinfo *find_svc(uint16_t *s16, unsigned len)
{
    struct svcinfo *si;

    for (si = svclist; si; si = si->next) {
        if ((len == si->len) && !memcmp(s16, si->name, len * sizeof(uint16_t))) {
            return si;
        }
    }
    return 0;
}

//	binder_acquire(bs, ptr);
void binder_acquire(struct binder_state *bs, void *ptr)
{
    uint32_t cmd[2];
    cmd[0] = BC_ACQUIRE;
    cmd[1] = (uint32_t) ptr;
    binder_write(bs, cmd, sizeof(cmd));
}

void binder_link_to_death(struct binder_state *bs, void *ptr, struct binder_death *death)
{
    uint32_t cmd[3];
    cmd[0] = BC_REQUEST_DEATH_NOTIFICATION;
    cmd[1] = (uint32_t) ptr;
    cmd[2] = (uint32_t) death;
    binder_write(bs, cmd, sizeof(cmd));
}

int binder_write(struct binder_state *bs, void *data, unsigned len)
{
    struct binder_write_read bwr;
    int res;
    bwr.write_size = len;
    bwr.write_consumed = 0;
    bwr.write_buffer = (unsigned) data;
    bwr.read_size = 0;
    bwr.read_consumed = 0;
    bwr.read_buffer = 0;
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
    if (res < 0) {
        fprintf(stderr,"binder_write: ioctl failed (%s)\n",strerror(errno));
    }
    return res;
}




#define NAME(n) case n: return #n
const char *cmd_name(uint32_t cmd)
{
    switch(cmd) {
        NAME(BR_NOOP);
        NAME(BR_TRANSACTION_COMPLETE);
        NAME(BR_INCREFS);
        NAME(BR_ACQUIRE);
        NAME(BR_RELEASE);
        NAME(BR_DECREFS);
        NAME(BR_TRANSACTION);
        NAME(BR_REPLY);
        NAME(BR_FAILED_REPLY);
        NAME(BR_DEAD_REPLY);
        NAME(BR_DEAD_BINDER);
    default: return "???";
    }
}


void hexdump(void *_data, unsigned len)
{
    unsigned char *data = _data;
    unsigned count;

    for (count = 0; count < len; count++) {
        if ((count & 15) == 0)
            fprintf(stderr,"%04x:", count);
        fprintf(stderr," %02x %c", *data,
                (*data < 32) || (*data > 126) ? '.' : *data);
        data++;
        if ((count & 15) == 15)
            fprintf(stderr,"\n");
    }
    if ((count & 15) != 0)
        fprintf(stderr,"\n");
}

void binder_dump_txn(struct binder_txn *txn)
{
    struct binder_object *obj;
    unsigned *offs = txn->offs;
    unsigned count = txn->offs_size / 4;

    fprintf(stderr,"  target %p  cookie %p  code %08x  flags %08x\n",txn->target, txn->cookie, txn->code, txn->flags);
    fprintf(stderr,"  pid %8d  uid %8d  data %8d  offs %8d\n",txn->sender_pid, txn->sender_euid, txn->data_size, txn->offs_size);
    hexdump(txn->data, txn->data_size);
    while (count--) {
        obj = (void*) (((char*) txn->data) + *offs++);
        fprintf(stderr,"  - type %08x  flags %08x  ptr %p  cookie %p\n",obj->type, obj->flags, obj->pointer, obj->cookie);
    }
}







/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//驱动分析
static const struct file_operations binder_fops = {
	.owner = THIS_MODULE,
	.poll = binder_poll,
	.unlocked_ioctl = binder_ioctl,
	.compat_ioctl = binder_ioctl,
	.mmap = binder_mmap,
	.open = binder_open,
	.flush = binder_flush,
	.release = binder_release,
};

static struct miscdevice binder_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "binder",
	.fops = &binder_fops
};

BINDER_DEBUG_ENTRY(state);
BINDER_DEBUG_ENTRY(stats);
BINDER_DEBUG_ENTRY(transactions);
BINDER_DEBUG_ENTRY(transaction_log);

static int __init binder_init(void)
{
	int ret;

	binder_deferred_workqueue = create_singlethread_workqueue("binder");
	if (!binder_deferred_workqueue)
		return -ENOMEM;

	binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
	if (binder_debugfs_dir_entry_root)
		binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",binder_debugfs_dir_entry_root);

	ret = misc_register(&binder_miscdev);

	if (binder_debugfs_dir_entry_root) {
		debugfs_create_file("state",S_IRUGO,binder_debugfs_dir_entry_root,NULL,&binder_state_fops);
		debugfs_create_file("stats",S_IRUGO,binder_debugfs_dir_entry_root,NULL,&binder_stats_fops);
		debugfs_create_file("transactions",S_IRUGO,binder_debugfs_dir_entry_root,NULL,&binder_transactions_fops);
		debugfs_create_file("transaction_log",S_IRUGO,binder_debugfs_dir_entry_root,&binder_transaction_log,&binder_transaction_log_fops);
		debugfs_create_file("failed_transaction_log",S_IRUGO,binder_debugfs_dir_entry_root,&binder_transaction_log_failed,&binder_transaction_log_fops);
	}
	return ret;
}
 
device_initcall(binder_init);





static int binder_open(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",current->group_leader->pid, current->pid);

	//给每个打开该设备的进程（打开binder设备是单实例）创建一个binder_proc
	proc = kzalloc(sizeof(*proc), GFP_KERNEL);
	if (proc == NULL)
		return -ENOMEM;

	//	#define get_task_struct(tsk) do { atomic_inc(&(tsk)->usage); } while(0)
	get_task_struct(current);//增加计数
	proc->tsk = current;/**每个进程在内核中都有一个进程控制块(PCB)来维护进程相关的信息,Linux内核的进程控制块是task_struct结构体.*/
	INIT_LIST_HEAD(&proc->todo);
	init_waitqueue_head(&proc->wait);//初始化等待队列头
	proc->default_priority = task_nice(current);

	binder_lock(__func__);

	binder_stats_created(BINDER_STAT_PROC);//增加binder_stats对应的BINDER_STAT_PROC的计数
	hlist_add_head(&proc->proc_node, &binder_procs);//用来加入到全局binder_procs链表上
	proc->pid = current->group_leader->pid;//赋值当前进程ID
	INIT_LIST_HEAD(&proc->delivered_death);
	filp->private_data = proc;

	binder_unlock(__func__);

	if (binder_debugfs_dir_entry_proc) {
		char strbuf[11];
		snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
		proc->debugfs_entry = debugfs_create_file(strbuf, S_IRUGO,binder_debugfs_dir_entry_proc, proc, &binder_proc_fops);
	}

	return 0;
}


struct binder_proc {
	struct hlist_node proc_node;//用来加入到全局binder_procs链表上
	struct rb_root threads;//threads树用来保存binder_proc进程内用于处理用户请求的线程，它的最大数量由max_threads来决定
	struct rb_root nodes;//node树成用来保存binder_proc进程内的Binder实体(结构为binder_node)
	/***
	*refs_by_desc树和refs_by_node树用来保存binder_proc进程内的Binder引用，即引用的其它进程的Binder实体，
	*它分别用两种方式来组织红黑树，一种是以句柄作来key值来组织，一种是以引用的实体节点的地址值作来key值来组织，
	*它们都是表示同一样东西，只不过是为了内部查找方便而用两个红黑树来表示
	*/
	struct rb_root refs_by_desc;
	struct rb_root refs_by_node;


	int pid;
	struct vm_area_struct *vma;
	struct mm_struct *vma_vm_mm;
	struct task_struct *tsk;/**每个进程在内核中都有一个进程控制块(PCB)来维护进程相关的信息,Linux内核的进程控制块是task_struct结构体.*/
	struct files_struct *files;
	struct hlist_node deferred_work_node;
	int deferred_work;
	void *buffer; //成员buffer变量是一个void*指针，它表示要映射的物理内存在内核空间中的起始位置
	/**
	*user_buffer_offset成员变量是一个ptrdiff_t类型的变量，它表示的是内核使用的虚拟地址与进程使用的虚拟地址之间的差值，
	*即如果某个物理页面在内核空间中对应的虚拟地址是addr的话，那么这个物理页面在进程空间对应的虚拟地址就为addr + user_buffer_offset
	*/
	ptrdiff_t user_buffer_offset;

	struct list_head buffers;
	struct rb_root free_buffers;//空闲的binder_buffer通过成员变量rb_node连入到struct binder_proc中的free_buffers表示的红黑树中去
	struct rb_root allocated_buffers;//正在使用的binder_buffer通过成员变量rb_node连入到struct binder_proc中的allocated_buffers表示的红黑树中去
	size_t free_async_space;

	struct page **pages;//pages成员变量是一个struct page*类型的数组，struct page是用来描述物理页面的数据结构
	size_t buffer_size;//buffer_size成员变量是一个size_t类型的变量，表示要映射的内存的大小
	uint32_t buffer_free;
	struct list_head todo;//事务链表
	wait_queue_head_t wait;//等待队列
	struct binder_stats stats;
	struct list_head delivered_death;
	int max_threads;//最大数量由max_threads来决定
	int requested_threads;
	int requested_threads_started;
	int ready_threads;
	long default_priority;
	struct dentry *debugfs_entry;
};


//	kernel_imx/include/linux/rbtree.h
struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root {
	struct rb_node *rb_node;
};





//vm_area_struct 进程虚拟内存
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct vm_struct *area;//内核虚拟内存
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

	/*pr_info("binder_mmap: %d %lx-%lx maps %p\n",
		 proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/
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
	pr_err("binder_mmap: %d %lx-%lx %s failed %d\n",
	       proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
	return ret;
}

/**
*Binder驱动程序管理这个内存映射地址空间的方法，即是如何管理buffer ~ (buffer + buffer_size)
*这段地址空间的，这个地址空间被划分为一段一段来管理，
*每一段是结构体struct binder_buffer来描述                                          
*/
struct binder_buffer {
	struct list_head entry; /* free and allocated entries by address */
	struct rb_node rb_node; /* free entry by size or allocated entry */
				/* by address */
	unsigned free:1;
	unsigned allow_user_free:1;
	unsigned async_transaction:1;
	unsigned debug_id:29;

	struct binder_transaction *transaction;

	struct binder_node *target_node;
	size_t data_size;
	size_t offsets_size;
	uint8_t data[0];
};


/**
 *	get_vm_area  -  reserve（储备） a contiguous（临近的） kernel virtual area
 *	@size:		size of the area
 *	@flags:		%VM_IOREMAP for I/O mappings or VM_ALLOC
 *
 *	Search an area of @size in the kernel virtual mapping area,
 *	and reserved it for out purposes.  Returns the area descriptor
 *	on success or %NULL on failure.
 */
struct vm_struct *get_vm_area(unsigned long size, unsigned long flags)
{
	return __get_vm_area_node(size, 1, flags, VMALLOC_START, VMALLOC_END,NUMA_NO_NODE, GFP_KERNEL,__builtin_return_address(0));
}


static struct vm_struct *__get_vm_area_node(unsigned long size,
		unsigned long align, unsigned long flags, unsigned long start,
		unsigned long end, int node, gfp_t gfp_mask, const void *caller)
{
	struct vmap_area *va;
	struct vm_struct *area;//内核虚拟内存结构

	BUG_ON(in_interrupt());//??????????? 不能在中断函数中调用这个函数
	if (flags & VM_IOREMAP) {
		int bit = fls(size);

		if (bit > IOREMAP_MAX_ORDER)
			bit = IOREMAP_MAX_ORDER;
		else if (bit < PAGE_SHIFT)
			bit = PAGE_SHIFT;

		align = 1ul << bit;
	}

	size = PAGE_ALIGN(size);
	if (unlikely(!size))
		return NULL;

	//
	area = kzalloc_node(sizeof(*area), gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!area))
		return NULL;

	/*
	 * We always allocate a guard page.
	 */
	size += PAGE_SIZE;

	va = alloc_vmap_area(size, align, start, end, node, gfp_mask);
	if (IS_ERR(va)) {
		kfree(area);
		return NULL;
	}

	/*
	 * When this function is called from __vmalloc_node_range,
	 * we add VM_UNLIST flag to avoid accessing uninitialized
	 * members of vm_struct such as pages and nr_pages fields.
	 * They will be set later.
	 */
	if (flags & VM_UNLIST)
		setup_vmalloc_vm(area, va, flags, caller);
	else
		insert_vmalloc_vm(area, va, flags, caller);//根据注释执行这里

	return area;
}


/*
 * Allocate a region of KVA of the specified size and alignment, within the
 * vstart and vend.
 */
static struct vmap_area *alloc_vmap_area(unsigned long size,unsigned long align,unsigned long vstart, unsigned long vend,int node, gfp_t gfp_mask)
{
	struct vmap_area *va;
	struct rb_node *n;
	unsigned long addr;
	int purged = 0;
	struct vmap_area *first;

	BUG_ON(!size);
	BUG_ON(size & ~PAGE_MASK);
	BUG_ON(!is_power_of_2(align));

	va = kmalloc_node(sizeof(struct vmap_area),gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!va))
		return ERR_PTR(-ENOMEM);

retry:
	spin_lock(&vmap_area_lock);
	/*
	 * Invalidate cache if we have more permissive parameters.
	 * cached_hole_size notes the largest hole noticed _below_
	 * the vmap_area cached in free_vmap_cache: if size fits
	 * into that hole, we want to scan from vstart to reuse
	 * the hole instead of allocating above free_vmap_cache.
	 * Note that __free_vmap_area may update free_vmap_cache
	 * without updating cached_hole_size or cached_align.
	 */
	if (!free_vmap_cache ||
			size < cached_hole_size ||
			vstart < cached_vstart ||
			align < cached_align) {
nocache:
		cached_hole_size = 0;
		free_vmap_cache = NULL;
	}
	/* record if we encounter less permissive parameters */
	cached_vstart = vstart;
	cached_align = align;

	/* find starting point for our search */
	if (free_vmap_cache) {
		first = rb_entry(free_vmap_cache, struct vmap_area, rb_node);
		addr = ALIGN(first->va_end, align);
		if (addr < vstart)
			goto nocache;
		if (addr + size < addr)
			goto overflow;

	} else {
		addr = ALIGN(vstart, align);
		if (addr + size < addr)
			goto overflow;

		n = vmap_area_root.rb_node;
		first = NULL;

		while (n) {
			struct vmap_area *tmp;
			tmp = rb_entry(n, struct vmap_area, rb_node);
			if (tmp->va_end >= addr) {
				first = tmp;
				if (tmp->va_start <= addr)
					break;
				n = n->rb_left;
			} else
				n = n->rb_right;
		}

		if (!first)
			goto found;
	}

	/* from the starting point, walk areas until a suitable hole is found */
	while (addr + size > first->va_start && addr + size <= vend) {
		if (addr + cached_hole_size < first->va_start)
			cached_hole_size = first->va_start - addr;
		addr = ALIGN(first->va_end, align);
		if (addr + size < addr)
			goto overflow;

		if (list_is_last(&first->list, &vmap_area_list))
			goto found;

		first = list_entry(first->list.next,
				struct vmap_area, list);
	}

found:
	if (addr + size > vend)
		goto overflow;

	va->va_start = addr;
	va->va_end = addr + size;
	va->flags = 0;
	__insert_vmap_area(va);
	free_vmap_cache = &va->rb_node;
	spin_unlock(&vmap_area_lock);

	BUG_ON(va->va_start & (align-1));
	BUG_ON(va->va_start < vstart);
	BUG_ON(va->va_end > vend);

	return va;

overflow:
	spin_unlock(&vmap_area_lock);
	if (!purged) {
		purge_vmap_area_lazy();
		purged = 1;
		goto retry;
	}
	if (printk_ratelimit())
		printk(KERN_WARNING
			"vmap allocation for size %lu failed: "
			"use vmalloc=<size> to increase size.\n", size);
	kfree(va);
	return ERR_PTR(-EBUSY);
}

static void __insert_vmap_area(struct vmap_area *va)
{
	struct rb_node **p = &vmap_area_root.rb_node;
	struct rb_node *parent = NULL;
	struct rb_node *tmp;

	while (*p) {
		struct vmap_area *tmp_va;

		parent = *p;
		tmp_va = rb_entry(parent, struct vmap_area, rb_node);
		if (va->va_start < tmp_va->va_end)
			p = &(*p)->rb_left;
		else if (va->va_end > tmp_va->va_start)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&va->rb_node, parent, p);
	rb_insert_color(&va->rb_node, &vmap_area_root);

	/* address-sort this list */
	tmp = rb_prev(&va->rb_node);
	if (tmp) {
		struct vmap_area *prev;
		prev = rb_entry(tmp, struct vmap_area, rb_node);
		list_add_rcu(&va->list, &prev->list);
	} else
		list_add_rcu(&va->list, &vmap_area_list);
}


static void insert_vmalloc_vm(struct vm_struct *vm, struct vmap_area *va,unsigned long flags, const void *caller)
{
	setup_vmalloc_vm(vm, va, flags, caller);
	clear_vm_unlist(vm);
}


static void setup_vmalloc_vm(struct vm_struct *vm, struct vmap_area *va,
			      unsigned long flags, const void *caller)
{
	spin_lock(&vmap_area_lock);
	vm->flags = flags;
	vm->addr = (void *)va->va_start;
	vm->size = va->va_end - va->va_start;
	vm->caller = caller;
	
	va->vm = vm;
	va->flags |= VM_VM_AREA;
	spin_unlock(&vmap_area_lock);
}

static void clear_vm_unlist(struct vm_struct *vm)
{
	/*
	 * Before removing VM_UNLIST,
	 * we should make sure that vm has proper values.
	 * Pair with smp_rmb() in show_numa_info().
	 */
	smp_wmb();
	vm->flags &= ~VM_UNLIST;
}




static int binder_update_page_range(struct binder_proc *proc, int allocate,void *start, void *end,struct vm_area_struct *vma)
{
	void *page_addr;
	unsigned long user_page_addr;
	struct vm_struct tmp_area;
	struct page **page;
	struct mm_struct *mm;

	binder_debug(BINDER_DEBUG_BUFFER_ALLOC, "%d: %s pages %p-%p\n", proc->pid, allocate ? "allocate" : "free", start, end);

	if (end <= start)
		return 0;

	trace_binder_update_page_range(proc, allocate, start, end);

	if (vma)
		mm = NULL;
	else
		mm = get_task_mm(proc->tsk);

	if (mm) {
		down_write(&mm->mmap_sem);
		vma = proc->vma;
		if (vma && mm != proc->vma_vm_mm) {
			pr_err("%d: vma mm and task mm mismatch\n", proc->pid);
			vma = NULL;
		}
	}

	if (allocate == 0)
		goto free_range;

	if (vma == NULL) {
		pr_err("%d: binder_alloc_buf failed to map pages in userspace, no vma\n",proc->pid);
		goto err_no_vma;
	}

	for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
		int ret;
		struct page **page_array_ptr;
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];

		BUG_ON(*page);
		//alloc_page来分配一个物理页面，这个函数返回一个struct page物理页面描述符
		*page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
		if (*page == NULL) {
			pr_err("%d: binder_alloc_buf failed for page at %p\n",proc->pid, page_addr);
			goto err_alloc_page_failed;
		}
		
		//根据这个page描述的内容初始化好struct vm_struct tmp_area结构体
		tmp_area.addr = page_addr;
		tmp_area.size = PAGE_SIZE + PAGE_SIZE /* guard page? */;
		page_array_ptr = page;
		
		//通过map_vm_area将这个物理页面插入到tmp_area描述的内核空间去
		ret = map_vm_area(&tmp_area, PAGE_KERNEL, &page_array_ptr);
		if (ret) {
			pr_err("%d: binder_alloc_buf failed to map page at %p in kernel\n",proc->pid, page_addr);
			goto err_map_kernel_failed;
		}
		//通过page_addr + proc->user_buffer_offset获得进程虚拟空间地
		user_page_addr = (uintptr_t)page_addr + proc->user_buffer_offset;
		
		//通过vm_insert_page函数将这个物理页面插入到进程地址空间去，参数vma代表了要插入的进程的地址空间
		ret = vm_insert_page(vma, user_page_addr, page[0]);
		if (ret) {
			pr_err("%d: binder_alloc_buf failed to map page at %lx in userspace\n", proc->pid, user_page_addr);
			goto err_vm_insert_page_failed;
		}
		/* vm_insert_page does not seem to increment the refcount */
	}
	if (mm) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
	return 0;

free_range:
	for (page_addr = end - PAGE_SIZE; page_addr >= start; page_addr -= PAGE_SIZE) {
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
		if (vma)
			zap_page_range(vma, (uintptr_t)page_addr + proc->user_buffer_offset, PAGE_SIZE, NULL);
err_vm_insert_page_failed:
		unmap_kernel_range((unsigned long)page_addr, PAGE_SIZE);
err_map_kernel_failed:
		__free_page(*page);
		*page = NULL;
err_alloc_page_failed:
		;
	}
err_no_vma:
	if (mm) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
	return -ENOMEM;
}

//////////////////////////////////////////////////////////////////////////////////////////
//binder_become_context_manager(bs)分析
//	frameworks/native/cmds/servicemanager/binder.c
int binder_become_context_manager(struct binder_state *bs)
{
    return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
}


static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	/*pr_info("binder_ioctl: %d:%d %x %lx\n", proc->pid, current->pid, cmd, arg);*/

	trace_binder_ioctl(cmd, arg);

	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret)
		goto err_unlocked;

	binder_lock(__func__);
	thread = binder_get_thread(proc);
	if (thread == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	switch (cmd) {
	......
	case BINDER_SET_MAX_THREADS:
		if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
			ret = -EINVAL;
			goto err;
		}
		break;
	case BINDER_SET_CONTEXT_MGR:
		//	有两个全局变量binder_context_mgr_node和binder_context_mgr_uid
		//	static struct binder_node *binder_context_mgr_node;
		//	static kuid_t binder_context_mgr_uid = INVALID_UID;
		if (binder_context_mgr_node != NULL) {
			pr_err("BINDER_SET_CONTEXT_MGR already set\n");
			ret = -EBUSY;
			goto err;
		}
		ret = security_binder_set_context_mgr(proc->tsk);//?????????????????
		if (ret < 0)
			goto err;
		if (uid_valid(binder_context_mgr_uid)) {
			if (!uid_eq(binder_context_mgr_uid, current->cred->euid)) {
				pr_err("BINDER_SET_CONTEXT_MGR bad uid %d != %d\n",
				       from_kuid(&init_user_ns, current->cred->euid), from_kuid(&init_user_ns, binder_context_mgr_uid));
				ret = -EPERM;
				goto err;
			}
		} else
			binder_context_mgr_uid = current->cred->euid;
		
		//把新建的binder_node指针保存在binder_context_mgr_node中
		binder_context_mgr_node = binder_new_node(proc, 0, 0);
		if (binder_context_mgr_node == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		//初始化了binder_context_mgr_node的引用计数值
		binder_context_mgr_node->local_weak_refs++;
		binder_context_mgr_node->local_strong_refs++;
		binder_context_mgr_node->has_strong_ref = 1;
		binder_context_mgr_node->has_weak_ref = 1;
		break;
	......
	}
	ret = 0;
err:
	if (thread)
		thread->looper &= ~BINDER_LOOPER_STATE_NEED_RETURN;
	binder_unlock(__func__);
	wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret && ret != -ERESTARTSYS)
		pr_info("%d:%d ioctl %x %lx returned %d\n", proc->pid, current->pid, cmd, arg, ret);
err_unlocked:
	trace_binder_ioctl_done(ret);
	return ret;
}


struct binder_thread {
	struct binder_proc *proc;//所属的进程
	struct rb_node rb_node;
	int pid;
	int looper;//looper成员变量表示线程的状态
	struct binder_transaction *transaction_stack;//transaction_stack表示线程正在处理的事务
	struct list_head todo;//todo表示发往该线程的数据列表
	//return_error和return_error2表示操作结果返回码
	uint32_t return_error; /* Write failed, return error code in read buf */
	uint32_t return_error2; /* Write failed, return error code in read */
	/* buffer. Used when sending a reply to a dead process that */
	/* we are also waiting on */
	wait_queue_head_t wait;//wait用来阻塞线程等待某个事件的发生
	struct binder_stats stats;//stats用来保存一些统计信息
};

//looper成员变量取值
enum {
	BINDER_LOOPER_STATE_REGISTERED  = 0x01,
	BINDER_LOOPER_STATE_ENTERED     = 0x02,
	BINDER_LOOPER_STATE_EXITED      = 0x04,
	BINDER_LOOPER_STATE_INVALID     = 0x08,
	BINDER_LOOPER_STATE_WAITING     = 0x10,
	BINDER_LOOPER_STATE_NEED_RETURN = 0x20
};


struct binder_transaction {
	int debug_id;
	struct binder_work work;
	struct binder_thread *from;
	struct binder_transaction *from_parent;
	struct binder_proc *to_proc;
	struct binder_thread *to_thread;
	struct binder_transaction *to_parent;
	unsigned need_reply:1;
	/* unsigned is_dead:1; */	/* not used at the moment */

	struct binder_buffer *buffer;
	unsigned int	code;
	unsigned int	flags;
	long	priority;
	long	saved_priority;
	kuid_t	sender_euid;
};

struct binder_work {
	struct list_head entry;
	enum {
		BINDER_WORK_TRANSACTION = 1,
		BINDER_WORK_TRANSACTION_COMPLETE,
		BINDER_WORK_NODE,
		BINDER_WORK_DEAD_BINDER,
		BINDER_WORK_DEAD_BINDER_AND_CLEAR,
		BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
	} type;
};



static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
	struct binder_thread *thread = NULL;
	struct rb_node *parent = NULL;
	struct rb_node **p = &proc->threads.rb_node;

	//  这里把当前线程current的pid作为键值，在进程proc->threads表示的红黑树中进行查找，
	// 看是否已经为当前线程创建过了binder_thread信息
	//在这个场景下，由于当前线程是第一次进到这里，所以肯定找不到，即*p == NULL成立，
	//于是，就为当前线程创建一个线程上下文信息结构体binder_thread，
	//并初始化相应成员变量，并插入到proc->threads所表示的红黑树中去，下次要使用时就可以从proc中找到了。
	//注意，这里的thread->looper = BINDER_LOOPER_STATE_NEED_RETURN
	while (*p) {
		parent = *p;
		thread = rb_entry(parent, struct binder_thread, rb_node);

		if (current->pid < thread->pid)
			p = &(*p)->rb_left;
		else if (current->pid > thread->pid)
			p = &(*p)->rb_right;
		else
			break;
	}
	if (*p == NULL) {
		thread = kzalloc(sizeof(*thread), GFP_KERNEL);
		if (thread == NULL)
			return NULL;
		binder_stats_created(BINDER_STAT_THREAD);
		thread->proc = proc;
		thread->pid = current->pid;
		init_waitqueue_head(&thread->wait);
		INIT_LIST_HEAD(&thread->todo);
		rb_link_node(&thread->rb_node, parent, p);
		rb_insert_color(&thread->rb_node, &proc->threads);
		thread->looper |= BINDER_LOOPER_STATE_NEED_RETURN;
		thread->return_error = BR_OK;
		thread->return_error2 = BR_OK;
	}
	return thread;
}


//binder_context_mgr_node = binder_new_node(proc, 0, 0);
static struct binder_node *binder_new_node(struct binder_proc *proc,binder_uintptr_t ptr,binder_uintptr_t cookie)
{
	//这里传进来的ptr和cookie均为NULL
	struct rb_node **p = &proc->nodes.rb_node;
	struct rb_node *parent = NULL;
	struct binder_node *node;

	while (*p) {
		parent = *p;
		node = rb_entry(parent, struct binder_node, rb_node);

		if (ptr < node->ptr)
			p = &(*p)->rb_left;
		else if (ptr > node->ptr)
			p = &(*p)->rb_right;
		else
			return NULL;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		return NULL;
	binder_stats_created(BINDER_STAT_NODE);
	rb_link_node(&node->rb_node, parent, p);
	rb_insert_color(&node->rb_node, &proc->nodes);
	node->debug_id = ++binder_last_id;
	node->proc = proc;
	node->ptr = ptr;
	node->cookie = cookie;
	node->work.type = BINDER_WORK_NODE;
	INIT_LIST_HEAD(&node->work.entry);
	INIT_LIST_HEAD(&node->async_todo);
	binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "%d:%d node %d u%016llx c%016llx created\n",
		     proc->pid, current->pid, node->debug_id,
		     (u64)node->ptr, (u64)node->cookie);
	return node;
}


//它表示一个binder实体
struct binder_node {
	int debug_id;
	struct binder_work work;
	union {
		struct rb_node rb_node;
		struct hlist_node dead_node;
	};
	struct binder_proc *proc;//proc成员变量就是表示这个Binder实例所属于进程了
	//refs成员变量把所有引用了该Binder实体的Binder引用连接起来构成一个链表
	struct hlist_head refs;
	//internal_strong_refs、local_weak_refs和local_strong_refs表示这个Binder实体的引用计数
	int internal_strong_refs;
	int local_weak_refs;
	int local_strong_refs;
	//ptr和cookie成员变量分别表示这个Binder实体在用户空间的地址以及附加数据
	binder_uintptr_t ptr;
	binder_uintptr_t cookie;
	unsigned has_strong_ref:1;
	unsigned pending_strong_ref:1;
	unsigned has_weak_ref:1;
	unsigned pending_weak_ref:1;
	unsigned has_async_transaction:1;
	unsigned accept_fds:1;
	unsigned min_priority:8;
	struct list_head async_todo;
};




///////////////////////////////////////////////////////////////////////////////////////////////////////////
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    unsigned readbuf[32];

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;
    
    readbuf[0] = BC_ENTER_LOOPER;
    binder_write(bs, readbuf, sizeof(unsigned));

    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (unsigned) readbuf;

        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

        if (res < 0) {
            ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }

        res = binder_parse(bs, 0, readbuf, bwr.read_consumed, func);
        if (res == 0) {
            ALOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}


int binder_write(struct binder_state *bs, void *data, unsigned len)
{
    struct binder_write_read bwr;
    int res;
    bwr.write_size = len;
    bwr.write_consumed = 0;
    bwr.write_buffer = (unsigned) data;
    bwr.read_size = 0;
    bwr.read_consumed = 0;
    bwr.read_buffer = 0;
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
    if (res < 0) {
        fprintf(stderr,"binder_write: ioctl failed (%s)\n",strerror(errno));
    }
    return res;
}


//	ionic/libc/kernel/common/uapi/binder.h
//write_bufffer和read_buffer所指向的结构体是struct binder_transaction_data
struct binder_write_read {
	signed long	write_size;	/* bytes to write */
	signed long	write_consumed;	/* bytes consumed by driver */
	unsigned long	write_buffer;
	signed long	read_size;	/* bytes to read */
	signed long	read_consumed;	/* bytes consumed by driver */
	unsigned long	read_buffer;
};

enum transaction_flags {
	 TF_ONE_WAY = 0x01,
	 TF_ROOT_OBJECT = 0x04,
	 TF_STATUS_CODE = 0x08,
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 TF_ACCEPT_FDS = 0x10,
};

struct binder_transaction_data {
	 union {
		/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
		 size_t handle;
		 void 	*ptr;
	 } target;
	 
	 void *cookie;
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 unsigned int code;
	 unsigned int flags;
	 pid_t sender_pid;
	 uid_t sender_euid;
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 size_t data_size;
	 size_t offsets_size;
	 union {
		 struct {
		/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
		 const void __user *buffer;
		 const void __user *offsets;
		 } ptr;
		 
		 uint8_t buf[8];
		/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 } data;
};






static int binder_thread_read(struct binder_proc *proc,struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,binder_size_t *consumed,
      			  int non_block)
{
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	int ret = 0;
	int wait_for_proc_work;

	if (*consumed == 0) {//该环境*consumed == 0 为真
		//往buffer中写入BR_NOOP
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}

retry:
	//hread->transaction_stack == NULL，并且thread->todo列表也是空的，
	//这表示当前线程没有事务需要处理，于是wait_for_proc_work为true，表示要去查看proc是否有未处理的事务
	wait_for_proc_work = thread->transaction_stack == NULL && list_empty(&thread->todo);

	//当前thread->return_error == BR_OK，这是前面创建binder_thread时初始化设置的
	if (thread->return_error != BR_OK && ptr < end) {
		if (thread->return_error2 != BR_OK) {
			if (put_user(thread->return_error2, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			binder_stat_br(proc, thread, thread->return_error2);
			if (ptr == end)
				goto done;
			thread->return_error2 = BR_OK;
		}
		if (put_user(thread->return_error, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		binder_stat_br(proc, thread, thread->return_error);
		thread->return_error = BR_OK;
		goto done;
	}


	//设置thread的状态为BINDER_LOOPER_STATE_WAITING，表示线程处于等待状态
	thread->looper |= BINDER_LOOPER_STATE_WAITING;
	if (wait_for_proc_work)
		proc->ready_threads++;

	binder_unlock(__func__);

	trace_binder_wait_for_work(wait_for_proc_work, !!thread->transaction_stack, !list_empty(&thread->todo));
	
	if (wait_for_proc_work) {//wait_for_proc_work为true
		if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED))) {
			binder_user_error("%d:%d ERROR: Thread waiting for process work before calling BC_REGISTER_LOOPER or BC_ENTER_LOOPER (state %x)\n",
				proc->pid, thread->pid, thread->looper);
			wait_event_interruptible(binder_user_error_wait,binder_stop_on_user_error < 2);
		}
		//调用binder_set_nice函数设置当前线程的优先级别为proc->default_priority，
		//这是因为thread要去处理属于proc的事务，因此要将此thread的优先级别设置和proc一样
		binder_set_nice(proc->default_priority);
		
		//如果文件打开模式为非阻塞模式，即non_block为true，那么函数就直接返回-EAGAIN，要求用户重新执行ioctl；
		//否则的话，就通过当前线程就通过wait_event_interruptible_exclusive函数进入休眠状态，等待请求到来再唤醒了。
		if (non_block) {
			if (!binder_has_proc_work(proc, thread))
				ret = -EAGAIN;
		} else
			//在这个场景中，proc也没有事务处理，即binder_has_proc_work(proc, thread)为false
			ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
	} else {
		。。。。。。
	}

	binder_lock(__func__);

	if (wait_for_proc_work)
		proc->ready_threads--;
	thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

	if (ret)
		return ret;

	while (1) {
		uint32_t cmd;
		struct binder_transaction_data tr;
		struct binder_work *w;
		struct binder_transaction *t = NULL;

		if (!list_empty(&thread->todo))
			。。。。。。
		else if (!list_empty(&proc->todo) && wait_for_proc_work)
			w = list_first_entry(&proc->todo, struct binder_work, entry);
		else {
			。。。。。。。
		}

		if (end - ptr < sizeof(tr) + 4)
			break;

		switch (w->type) {
		case BINDER_WORK_TRANSACTION: {
			t = container_of(w, struct binder_transaction, work);
		} break;
		。。。。。。
		}

		if (!t)
			continue;

		BUG_ON(t->buffer == NULL);
		if (t->buffer->target_node) {
			//接着就是把事务项t中的数据拷贝到本地局部变量struct binder_transaction_data tr中去了
			struct binder_node *target_node = t->buffer->target_node;
			tr.target.ptr = target_node->ptr;
			tr.cookie =  target_node->cookie;
			t->saved_priority = task_nice(current);
			if (t->priority < target_node->min_priority && !(t->flags & TF_ONE_WAY))
				binder_set_nice(t->priority);
			else if (!(t->flags & TF_ONE_WAY) || t->saved_priority > target_node->min_priority)
				binder_set_nice(target_node->min_priority);
			cmd = BR_TRANSACTION;
		} else {
			。。。。。。
		}
		tr.code = t->code;
		tr.flags = t->flags;
		tr.sender_euid = from_kuid(current_user_ns(), t->sender_euid);

		if (t->from) {
			struct task_struct *sender = t->from->proc->tsk;
			tr.sender_pid = task_tgid_nr_ns(sender, task_active_pid_ns(current));
		} else {
			tr.sender_pid = 0;
		}
		
		//t->buffer->data所指向的地址是内核空间的，现在要把数据返回给Service Manager进程的用户空间，
		//而Service Manager进程的用户空间是不能访问内核空间的数据的，所以这里要作一下处理。
		//怎么处理呢？我们在学面向对象语言的时候，对象的拷贝有深拷贝和浅拷贝之分，深拷贝是把另外分配一块新内存，
		//然后把原始对象的内容搬过去，浅拷贝是并没有为新对象分配一块新空间，而只是分配一个引用，
		//而个引用指向原始对象。Binder机制用的是类似浅拷贝的方法，通过在用户空间分配一个虚拟地址，
		//然后让这个用户空间虚拟地址与 t->buffer->data这个内核空间虚拟地址指向同一个物理地址，这样就可以实现浅拷贝了
		//这里只要将t->buffer->data加上一个偏移值proc->user_buffer_offset就可以得到t->buffer->data对应的用户空间虚拟地址了。
		//调整了tr.data.ptr.buffer的值之后，不要忘记也要一起调整tr.data.ptr.offsets的值。
		tr.data_size = t->buffer->data_size;
		tr.offsets_size = t->buffer->offsets_size;
		tr.data.ptr.buffer = (binder_uintptr_t)((uintptr_t)t->buffer->data + proc->user_buffer_offset);
		tr.data.ptr.offsets = tr.data.ptr.buffer + ALIGN(t->buffer->data_size, sizeof(void *));

		if (put_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		if (copy_to_user(ptr, &tr, sizeof(tr)))
			return -EFAULT;
		ptr += sizeof(tr);

		trace_binder_transaction_received(t);
		binder_stat_br(proc, thread, cmd);
		。。。。。。
		list_del(&t->work.entry);
		t->buffer->allow_user_free = 1;
		if (cmd == BR_TRANSACTION && !(t->flags & TF_ONE_WAY)) {
			t->to_parent = thread->transaction_stack;
			t->to_thread = thread;
			thread->transaction_stack = t;
		} else {
			。。。。。。
		}
		break;
	}

done:

	*consumed = ptr - buffer;
	if (proc->requested_threads + proc->ready_threads == 0 &&
	    proc->requested_threads_started < proc->max_threads &&
	    (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
	     BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
	     /*spawn a new thread if we leave this out */) {
		proc->requested_threads++;
		。。。。。。
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	}
	return 0;
}


//返回到用户空间，调用函数binder_loop，进入到binder_parse
int binder_parse(struct binder_state *bs, struct binder_io *bio,uint32_t *ptr, uint32_t size, binder_handler func)
{
    int r = 1;
    uint32_t *end = ptr + (size / 4);

    while (ptr < end) {
        uint32_t cmd = *ptr++;
		。。。。。。
        switch(cmd) {
		。。。。。。
		case BR_TRANSACTION: {
            struct binder_txn *txn = (void *) ptr;
            if ((end - ptr) * sizeof(uint32_t) < sizeof(struct binder_txn)) {
                ALOGE("parse: txn too small!\n");
                return -1;
            }
            binder_dump_txn(txn);
            if (func) {
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, txn);
                res = func(bs, txn, &msg, &reply);
                binder_send_reply(bs, &reply, txn->data, res);
            }
            ptr += sizeof(*txn) / sizeof(uint32_t);
            break;
        }
		。。。。。。
		}
    }

    return r;
}


struct binder_txn
{
    void *target;
    void *cookie;
    uint32_t code;
    uint32_t flags;

    uint32_t sender_pid;
    uint32_t sender_euid;

    uint32_t data_size;
    uint32_t offs_size;
    void *data;
    void *offs;
};


struct binder_io
{
    char *data;            /* pointer to read/write from */
    uint32_t *offs;        /* array of offsets */
    uint32_t data_avail;   /* bytes available in data buffer */
    uint32_t offs_avail;   /* entries available in offsets array */

    char *data0;           /* start of data buffer */
    uint32_t *offs0;       /* start of offsets buffer */
    uint32_t flags;
    uint32_t unused;
};


void bio_init(struct binder_io *bio, void *data,uint32_t maxdata, uint32_t maxoffs)
{
    uint32_t n = maxoffs * sizeof(uint32_t);

    if (n > maxdata) {
        bio->flags = BIO_F_OVERFLOW;
        bio->data_avail = 0;
        bio->offs_avail = 0;
        return;
    }

    bio->data = bio->data0 = (char *) data + n;
    bio->offs = bio->offs0 = data;
    bio->data_avail = maxdata - n;
    bio->offs_avail = maxoffs;
    bio->flags = 0;
}

void bio_init_from_txn(struct binder_io *bio, struct binder_txn *txn)
{
    bio->data = bio->data0 = txn->data;
    bio->offs = bio->offs0 = txn->offs;
    bio->data_avail = txn->data_size;
    bio->offs_avail = txn->offs_size / 4;
    bio->flags = BIO_F_SHARED;
}

int svcmgr_handler(struct binder_state *bs,
                   struct binder_txn *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    unsigned len;
    void *ptr;
    uint32_t strict_policy;
    int allow_isolated;

//    ALOGI("target=%p code=%d pid=%d uid=%d\n",
//         txn->target, txn->code, txn->sender_pid, txn->sender_euid);

    if (txn->target != svcmgr_handle)
        return -1;

    // Equivalent to Parcel::enforceInterface(), reading the RPC
    // header with the strict mode policy mask and the interface name.
    // Note that we ignore the strict_policy and don't propagate it
    // further (since we do no outbound RPCs anyway).
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr,"invalid id %s\n", str8(s));
        return -1;
    }

    switch(txn->code) {
	。。。。。
    case SVC_MGR_ADD_SERVICE:
        s = bio_get_string16(msg, &len);
        ptr = bio_get_ref(msg);
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        if (do_add_service(bs, s, len, ptr, txn->sender_euid, allow_isolated))
            return -1;
        break;
	。。。。。。
    }

    bio_put_uint32(reply, 0);
    return 0;
}


uint16_t *bio_get_string16(struct binder_io *bio, unsigned *sz)
{
    unsigned len;
    len = bio_get_uint32(bio);
    if (sz)
        *sz = len;
    return bio_get(bio, (len + 1) * sizeof(uint16_t));
}

uint32_t bio_get_uint32(struct binder_io *bio)
{
    uint32_t *ptr = bio_get(bio, sizeof(*ptr));
    return ptr ? *ptr : 0;
}

static void *bio_get(struct binder_io *bio, uint32_t size)
{
    size = (size + 3) & (~3);

    if (bio->data_avail < size){
        bio->data_avail = 0;
        bio->flags |= BIO_F_OVERFLOW;
        return 0;
    }  else {
        void *ptr = bio->data;
        bio->data += size;
        bio->data_avail -= size;
        return ptr;
    }
}


void *bio_get_ref(struct binder_io *bio)
{
    struct binder_object *obj;

    obj = _bio_get_obj(bio);
    if (!obj)
        return 0;

    if (obj->type == BINDER_TYPE_HANDLE)
        return obj->pointer;

    return 0;
}

int do_add_service(struct binder_state *bs,uint16_t *s, unsigned len,void *ptr, unsigned uid, int allow_isolated)
{
    struct svcinfo *si;

    if (!ptr || (len == 0) || (len > 127))
        return -1;

	//确认用户ID是否可以注册
    if (!svc_can_register(uid, s)) {
        ALOGE("add_service('%s',%p) uid=%d - PERMISSION DENIED\n",str8(s), ptr, uid);
        return -1;
    }

	//查找对应服务
    si = find_svc(s, len);
    if (si) {
        if (si->ptr) {
            ALOGE("add_service('%s',%p) uid=%d - ALREADY REGISTERED, OVERRIDE\n",str8(s), ptr, uid);
            svcinfo_death(bs, si);
        }
        si->ptr = ptr;
    } else {
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        if (!si) {
            ALOGE("add_service('%s',%p) uid=%d - OUT OF MEMORY\n",
                 str8(s), ptr, uid);
            return -1;
        }
        si->ptr = ptr;//服务对应类指针
        si->len = len;
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        si->next = svclist;
        svclist = si;
    }

    binder_acquire(bs, ptr);
    binder_link_to_death(bs, ptr, &si->death);
    return 0;
}


void binder_acquire(struct binder_state *bs, void *ptr)
{
    uint32_t cmd[2];
    cmd[0] = BC_ACQUIRE;
    cmd[1] = (uint32_t) ptr;
    binder_write(bs, cmd, sizeof(cmd));
}


static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	trace_binder_ioctl(cmd, arg);

	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret)
		goto err_unlocked;

	binder_lock(__func__);
	thread = binder_get_thread(proc);
	if (thread == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	switch (cmd) {
	case BINDER_WRITE_READ: {
		struct binder_write_read bwr;
		if (size != sizeof(struct binder_write_read)) {
			ret = -EINVAL;
			goto err;
		}
		if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		。。。。。。
		if (bwr.write_size > 0) {
			ret = binder_thread_write(proc, thread, bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
			trace_binder_write_done(ret);
			if (ret < 0) {
				bwr.read_consumed = 0;
				if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
					ret = -EFAULT;
				goto err;
			}
		}
		if (bwr.read_size > 0) {
			。。。。。。。
		}
		。。。。。。
		if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		break;
	}
	。。。。。。
	}
	ret = 0;
err:
	if (thread)
		thread->looper &= ~BINDER_LOOPER_STATE_NEED_RETURN;
	binder_unlock(__func__);
	wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret && ret != -ERESTARTSYS)
		pr_info("%d:%d ioctl %x %lx returned %d\n", proc->pid, current->pid, cmd, arg, ret);
err_unlocked:
	trace_binder_ioctl_done(ret);
	return ret;
}

int binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,
			binder_uintptr_t binder_buffer, size_t size,
			binder_size_t *consumed)
{
	uint32_t cmd;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	while (ptr < end && thread->return_error == BR_OK) {
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		trace_binder_command(cmd);
		if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
			binder_stats.bc[_IOC_NR(cmd)]++;
			proc->stats.bc[_IOC_NR(cmd)]++;
			thread->stats.bc[_IOC_NR(cmd)]++;
		}
		switch (cmd) {
		case BC_ACQUIRE: {
			uint32_t target;
			struct binder_ref *ref;
			const char *debug_string;

			if (get_user(target, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (target == 0 && binder_context_mgr_node && (cmd == BC_INCREFS || cmd == BC_ACQUIRE)) {
				ref = binder_get_ref_for_node(proc,binder_context_mgr_node);
				if (ref->desc != target) {
					binder_user_error("%d:%d tried to acquire reference to desc 0, got %d instead\n",proc->pid, thread->pid,ref->desc);
				}
			} else
				ref = binder_get_ref(proc, target);
			if (ref == NULL) {
				binder_user_error("%d:%d refcount change on invalid ref %d\n",proc->pid, thread->pid, target);
				break;
			}
			switch (cmd) {
			。。。。。。
			case BC_ACQUIRE://增加引用计数
				debug_string = "Acquire";
				binder_inc_ref(ref, 1, NULL);
				break;
			。。。。。。
			}
			。。。。。。
			break;
		}
		
		}
		*consumed = ptr - buffer;
	}
	return 0;
}

static struct binder_ref *binder_get_ref(struct binder_proc *proc,uint32_t desc)
{
	struct rb_node *n = proc->refs_by_desc.rb_node;
	struct binder_ref *ref;

	while (n) {
		ref = rb_entry(n, struct binder_ref, rb_node_desc);

		if (desc < ref->desc)
			n = n->rb_left;
		else if (desc > ref->desc)
			n = n->rb_right;
		else
			return ref;
	}
	return NULL;
}

static int binder_inc_ref(struct binder_ref *ref, int strong,struct list_head *target_list)
{
	int ret;
	if (strong) {
		if (ref->strong == 0) {
			ret = binder_inc_node(ref->node, 1, 1, target_list);
			if (ret)
				return ret;
		}
		ref->strong++;
	} else {
		if (ref->weak == 0) {
			ret = binder_inc_node(ref->node, 0, 1, target_list);
			if (ret)
				return ret;
		}
		ref->weak++;
	}
	return 0;
}

static int binder_inc_node(struct binder_node *node, int strong, int internal,struct list_head *target_list)
{
	if (strong) {
		if (internal) {
			if (target_list == NULL && node->internal_strong_refs == 0 && !(node == binder_context_mgr_node && node->has_strong_ref)) {
				pr_err("invalid inc strong node for %d\n",node->debug_id);
				return -EINVAL;
			}
			node->internal_strong_refs++;
		} else
			node->local_strong_refs++;
		if (!node->has_strong_ref && target_list) {
			list_del_init(&node->work.entry);
			list_add_tail(&node->work.entry, target_list);
		}
	} else {
		if (!internal)
			node->local_weak_refs++;
		if (!node->has_weak_ref && list_empty(&node->work.entry)) {
			if (target_list == NULL) {
				pr_err("invalid inc weak node for %d\n", node->debug_id);
				return -EINVAL;
			}
			list_add_tail(&node->work.entry, target_list);
		}
	}
	return 0;
}


void binder_link_to_death(struct binder_state *bs, void *ptr, struct binder_death *death)
{
    uint32_t cmd[3];
    cmd[0] = BC_REQUEST_DEATH_NOTIFICATION;
    cmd[1] = (uint32_t) ptr;
    cmd[2] = (uint32_t) death;
    binder_write(bs, cmd, sizeof(cmd));
}

int binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,
			binder_uintptr_t binder_buffer, size_t size,
			binder_size_t *consumed)
{
	uint32_t cmd;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	while (ptr < end && thread->return_error == BR_OK) {
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		trace_binder_command(cmd);
		if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
			binder_stats.bc[_IOC_NR(cmd)]++;
			proc->stats.bc[_IOC_NR(cmd)]++;
			thread->stats.bc[_IOC_NR(cmd)]++;
		}
		switch (cmd) {
		case BC_REQUEST_DEATH_NOTIFICATION:
		case BC_CLEAR_DEATH_NOTIFICATION: {
			uint32_t target;
			binder_uintptr_t cookie;
			struct binder_ref *ref;
			struct binder_ref_death *death;

			if (get_user(target, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(binder_uintptr_t);
			ref = binder_get_ref(proc, target);
			if (ref == NULL) {
				。。。。。。
				break;
			}

			。。。。。。

			if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
				if (ref->death) {
					binder_user_error("%d:%d BC_REQUEST_DEATH_NOTIFICATION death notification already set\n",proc->pid, thread->pid);
					break;
				}
				death = kzalloc(sizeof(*death), GFP_KERNEL);
				if (death == NULL) {
					thread->return_error = BR_ERROR;
					binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,"%d:%d BC_REQUEST_DEATH_NOTIFICATION failed\n",proc->pid, thread->pid);
					break;
				}
				binder_stats_created(BINDER_STAT_DEATH);
				INIT_LIST_HEAD(&death->work.entry);
				death->cookie = cookie;
				ref->death = death;
				if (ref->node->proc == NULL) {
					ref->death->work.type = BINDER_WORK_DEAD_BINDER;
					if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
						list_add_tail(&ref->death->work.entry, &thread->todo);
					} else {
						list_add_tail(&ref->death->work.entry, &proc->todo);
						wake_up_interruptible(&proc->wait);
					}
				}
			} else {
				。。。。。。
			}
		} break;
		。。。。。。
		}
		*consumed = ptr - buffer;
	}
	return 0;
}



void binder_send_reply(struct binder_state *bs,struct binder_io *reply,void *buffer_to_free,int status)
{
    struct {
        uint32_t cmd_free;
        void *buffer;
        uint32_t cmd_reply;
        struct binder_txn txn;
    } __attribute__((packed)) data;

    data.cmd_free = BC_FREE_BUFFER;
    data.buffer = buffer_to_free;
    data.cmd_reply = BC_REPLY;
    data.txn.target = 0;
    data.txn.cookie = 0;
    data.txn.code = 0;
    if (status) {
        data.txn.flags = TF_STATUS_CODE;
        data.txn.data_size = sizeof(int);
        data.txn.offs_size = 0;
        data.txn.data = &status;
        data.txn.offs = 0;
    } else {
        data.txn.flags = 0;
        data.txn.data_size = reply->data - reply->data0;
        data.txn.offs_size = ((char*) reply->offs) - ((char*) reply->offs0);
        data.txn.data = reply->data0;
        data.txn.offs = reply->offs0;
    }
    binder_write(bs, &data, sizeof(data));
}

// servicemanager写入BC_FREE_BUFFER和BC_REPLY命令，其中BC_FREE_BUFFER用于释放内存
// 
int binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,binder_uintptr_t binder_buffer, size_t size,binder_size_t *consumed)
{
	uint32_t cmd;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	while (ptr < end && thread->return_error == BR_OK) {
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		trace_binder_command(cmd);
		if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
			binder_stats.bc[_IOC_NR(cmd)]++;
			proc->stats.bc[_IOC_NR(cmd)]++;
			thread->stats.bc[_IOC_NR(cmd)]++;
		}
		switch (cmd) {
		。。。。。。
		case BC_FREE_BUFFER: {//第一个命令
			binder_uintptr_t data_ptr;
			struct binder_buffer *buffer;

			if (get_user(data_ptr, (binder_uintptr_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(binder_uintptr_t);

			buffer = binder_buffer_lookup(proc, data_ptr);
			if (buffer == NULL) {
				binder_user_error("%d:%d BC_FREE_BUFFER u%016llx no match\n",proc->pid, thread->pid, (u64)data_ptr);
				break;
			}
			if (!buffer->allow_user_free) {
				binder_user_error("%d:%d BC_FREE_BUFFER u%016llx matched unreturned buffer\n",proc->pid, thread->pid, (u64)data_ptr);
				break;
			}
			。。。。。。

			if (buffer->transaction) {
				buffer->transaction->buffer = NULL;
				buffer->transaction = NULL;
			}
			if (buffer->async_transaction && buffer->target_node) {
				BUG_ON(!buffer->target_node->has_async_transaction);
				if (list_empty(&buffer->target_node->async_todo))
					buffer->target_node->has_async_transaction = 0;
				else
					list_move_tail(buffer->target_node->async_todo.next, &thread->todo);
			}
			trace_binder_transaction_buffer_release(buffer);
			binder_transaction_buffer_release(proc, buffer, NULL);
			binder_free_buf(proc, buffer);
			break;
		}

		case BC_TRANSACTION:
		case BC_REPLY: {//BC_REPLY为第二个命令
			struct binder_transaction_data tr;

			if (copy_from_user(&tr, ptr, sizeof(tr)))
				return -EFAULT;
			ptr += sizeof(tr);
			binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
			break;
		}
		
		。。。。。。
		}
		*consumed = ptr - buffer;
	}
	return 0;
}


static void binder_transaction(struct binder_proc *proc, struct binder_thread *thread,struct binder_transaction_data *tr, int reply)
{
	struct binder_transaction *t;
	struct binder_work *tcomplete;
	size_t *offp, *off_end;
	struct binder_proc *target_proc;
	struct binder_thread *target_thread = NULL;
	struct binder_node *target_node = NULL;
	struct list_head *target_list;
	wait_queue_head_t *target_wait;
	struct binder_transaction *in_reply_to = NULL;
	struct binder_transaction_log_entry *e;
	uint32_t return_error;
 
	......
 
	if (reply) {
		in_reply_to = thread->transaction_stack;
		if (in_reply_to == NULL) {
			......
			return_error = BR_FAILED_REPLY;
			goto err_empty_call_stack;
		}
		binder_set_nice(in_reply_to->saved_priority);
		if (in_reply_to->to_thread != thread) {
			.......
			goto err_bad_call_stack;
		}
		thread->transaction_stack = in_reply_to->to_parent;
		target_thread = in_reply_to->from;
		if (target_thread == NULL) {
			return_error = BR_DEAD_REPLY;
			goto err_dead_binder;
		}
		if (target_thread->transaction_stack != in_reply_to) {
			......
			return_error = BR_FAILED_REPLY;
			in_reply_to = NULL;
			target_thread = NULL;
			goto err_dead_binder;
		}
		target_proc = target_thread->proc;
	} else {
		......
	}
	if (target_thread) {
		e->to_thread = target_thread->pid;
		target_list = &target_thread->todo;
		target_wait = &target_thread->wait;
	} else {
		......
	}
 
 
	/* TODO: reuse incoming transaction for reply */
	t = kzalloc(sizeof(*t), GFP_KERNEL);  //binder_transaction 用于插入到目标等待队列
	if (t == NULL) {
		return_error = BR_FAILED_REPLY;
		goto err_alloc_t_failed;
	}
	
 
	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);//用于返回给应用的队列中
	if (tcomplete == NULL) {
		return_error = BR_FAILED_REPLY;
		goto err_alloc_tcomplete_failed;
	}
 
	if (!reply && !(tr->flags & TF_ONE_WAY))
		t->from = thread;
	else
		t->from = NULL;
	t->sender_euid = proc->tsk->cred->euid;
	t->to_proc = target_proc;
	t->to_thread = target_thread;
	t->code = tr->code;
	t->flags = tr->flags;
	t->priority = task_nice(current);
	t->buffer = binder_alloc_buf(target_proc, tr->data_size,tr->offsets_size, !reply && (t->flags & TF_ONE_WAY));
	if (t->buffer == NULL) {
		return_error = BR_FAILED_REPLY;
		goto err_binder_alloc_buf_failed;
	}
	t->buffer->allow_user_free = 0;
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t;
	t->buffer->target_node = target_node;
	if (target_node)
		binder_inc_node(target_node, 1, 0, NULL);
 
	offp = (size_t *)(t->buffer->data + ALIGN(tr->data_size, sizeof(void *)));
 
	if (copy_from_user(t->buffer->data, tr->data.ptr.buffer, tr->data_size)) {
		binder_user_error("binder: %d:%d got transaction with invalid " "data ptr\n", proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		goto err_copy_data_failed;
	}
	if (copy_from_user(offp, tr->data.ptr.offsets, tr->offsets_size)) {
		binder_user_error("binder: %d:%d got transaction with invalid " "offsets ptr\n", proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		goto err_copy_data_failed;
	}
	
    	......
 
	if (reply) {
		BUG_ON(t->buffer->async_transaction != 0);
		binder_pop_transaction(target_thread, in_reply_to);
	} else if (!(t->flags & TF_ONE_WAY)) {
		......
	} else {
		......
	}

	t->work.type = BINDER_WORK_TRANSACTION;
	list_add_tail(&t->work.entry, target_list);
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	list_add_tail(&tcomplete->entry, &thread->todo);

	if (target_wait)
		wake_up_interruptible(target_wait);

	return;
    	......
}












































void ProcessState::startThreadPool()
{
    AutoMutex _l(mLock);
    if (!mThreadPoolStarted) {
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}

//这里isMain == true
void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.string());
        sp<Thread> t = new PoolThread(isMain);
        t->run(name.string());
    }
}

String8 ProcessState::makeBinderThreadName() {
    int32_t s = android_atomic_add(1, &mThreadPoolSeq);
    String8 name;
    name.appendFormat("Binder_%X", s);
    return name;
}


void IPCThreadState::joinThreadPool(bool isMain)
{
    LOG_THREADPOOL("**** THREAD %p (PID %d) IS JOINING THE THREAD POOL\n", (void*)pthread_self(), getpid());

    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);
    
    // This thread may have been spawned by a thread that was in the background
    // scheduling group, so first we will make sure it is in the foreground
    // one to avoid performing an initial transaction in the background.
    set_sched_policy(mMyThreadId, SP_FOREGROUND);
        
    status_t result;
    do {
        processPendingDerefs();
        // now get the next command to be processed, waiting if necessary
        result = getAndExecuteCommand();

        if (result < NO_ERROR && result != TIMED_OUT && result != -ECONNREFUSED && result != -EBADF) {
            ALOGE("getAndExecuteCommand(fd=%d) returned unexpected error %d, aborting",mProcess->mDriverFD, result);
            abort();
        }
        
        // Let this thread exit the thread pool if it is no longer
        // needed and it is not the main process thread.
        if(result == TIMED_OUT && !isMain) {
            break;
        }
    } while (result != -ECONNREFUSED && result != -EBADF);

    LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%p\n",(void*)pthread_self(), getpid(), (void*)result);
    
    mOut.writeInt32(BC_EXIT_LOOPER);
    talkWithDriver(false);
}  

// When we've cleared the incoming command queue, process any pending derefs
void IPCThreadState::processPendingDerefs()
{
    if (mIn.dataPosition() >= mIn.dataSize()) {
        size_t numPending = mPendingWeakDerefs.size();
        if (numPending > 0) {
            for (size_t i = 0; i < numPending; i++) {
                RefBase::weakref_type* refs = mPendingWeakDerefs[i];
                refs->decWeak(mProcess.get());
            }
            mPendingWeakDerefs.clear();
        }

        numPending = mPendingStrongDerefs.size();
        if (numPending > 0) {
            for (size_t i = 0; i < numPending; i++) {
                BBinder* obj = mPendingStrongDerefs[i];
                obj->decStrong(mProcess.get());
            }
            mPendingStrongDerefs.clear();
        }
    }
}


status_t IPCThreadState::getAndExecuteCommand()
{
    status_t result;
    int32_t cmd;

    result = talkWithDriver();
    if (result >= NO_ERROR) {
        size_t IN = mIn.dataAvail();
        if (IN < sizeof(int32_t)) return result;
        cmd = mIn.readInt32();
        IF_LOG_COMMANDS() {
            alog << "Processing top-level Command: "  << getReturnString(cmd) << endl;
        }

        result = executeCommand(cmd);

        // After executing the command, ensure that the thread is returned to the
        // foreground cgroup before rejoining the pool.  The driver takes care of
        // restoring the priority, but doesn't do anything with cgroups so we
        // need to take care of that here in userspace.  Note that we do make
        // sure to go in the foreground after executing a transaction, but
        // there are other callbacks into user code that could have changed
        // our group so we want to make absolutely sure it is put back.
        set_sched_policy(mMyThreadId, SP_FOREGROUND);
    }

    return result;
}


status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;
    
    switch (cmd) {
    case BR_ERROR:
        result = mIn.readInt32();
        break;
        
    case BR_OK:
        break;
        
    case BR_ACQUIRE:
        refs = (RefBase::weakref_type*)mIn.readInt32();
        obj = (BBinder*)mIn.readInt32();
        ALOG_ASSERT(refs->refBase() == obj, "BR_ACQUIRE: object %p does not match cookie %p (expected %p)", refs, obj, refs->refBase());
        obj->incStrong(mProcess.get());
        IF_LOG_REMOTEREFS() {
            LOG_REMOTEREFS("BR_ACQUIRE from driver on %p", obj);
            obj->printRefs();
        }
        mOut.writeInt32(BC_ACQUIRE_DONE);
        mOut.writeInt32((int32_t)refs);
        mOut.writeInt32((int32_t)obj);
        break;
        
    case BR_RELEASE:
        refs = (RefBase::weakref_type*)mIn.readInt32();
        obj = (BBinder*)mIn.readInt32();
        ALOG_ASSERT(refs->refBase() == obj,
                   "BR_RELEASE: object %p does not match cookie %p (expected %p)",
                   refs, obj, refs->refBase());
        IF_LOG_REMOTEREFS() {
            LOG_REMOTEREFS("BR_RELEASE from driver on %p", obj);
            obj->printRefs();
        }
        mPendingStrongDerefs.push(obj);
        break;
        
    case BR_INCREFS:
        refs = (RefBase::weakref_type*)mIn.readInt32();
        obj = (BBinder*)mIn.readInt32();
        refs->incWeak(mProcess.get());
        mOut.writeInt32(BC_INCREFS_DONE);
        mOut.writeInt32((int32_t)refs);
        mOut.writeInt32((int32_t)obj);
        break;
        
    case BR_DECREFS:
        refs = (RefBase::weakref_type*)mIn.readInt32();
        obj = (BBinder*)mIn.readInt32();
        // NOTE: This assertion is not valid, because the object may no
        // longer exist (thus the (BBinder*)cast above resulting in a different
        // memory address).
        //ALOG_ASSERT(refs->refBase() == obj,
        //           "BR_DECREFS: object %p does not match cookie %p (expected %p)",
        //           refs, obj, refs->refBase());
        mPendingWeakDerefs.push(refs);
        break;
        
    case BR_ATTEMPT_ACQUIRE:
        refs = (RefBase::weakref_type*)mIn.readInt32();
        obj = (BBinder*)mIn.readInt32();
         
        {
            const bool success = refs->attemptIncStrong(mProcess.get());
            ALOG_ASSERT(success && refs->refBase() == obj,
                       "BR_ATTEMPT_ACQUIRE: object %p does not match cookie %p (expected %p)",
                       refs, obj, refs->refBase());
            
            mOut.writeInt32(BC_ACQUIRE_RESULT);
            mOut.writeInt32((int32_t)success);
        }
        break;
    
    case BR_TRANSACTION:
        {
            binder_transaction_data tr;
            result = mIn.read(&tr, sizeof(tr));
            ALOG_ASSERT(result == NO_ERROR, "Not enough command data for brTRANSACTION");
            if (result != NO_ERROR) break;
            
            Parcel buffer;
            buffer.ipcSetDataReference(
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(size_t), freeBuffer, this);
            
            const pid_t origPid = mCallingPid;
            const uid_t origUid = mCallingUid;
            
            mCallingPid = tr.sender_pid;
            mCallingUid = tr.sender_euid;
            
            int curPrio = getpriority(PRIO_PROCESS, mMyThreadId);
            if (gDisableBackgroundScheduling) {
                if (curPrio > ANDROID_PRIORITY_NORMAL) {
                    // We have inherited a reduced priority from the caller, but do not
                    // want to run in that state in this process.  The driver set our
                    // priority already (though not our scheduling class), so bounce
                    // it back to the default before invoking the transaction.
                    setpriority(PRIO_PROCESS, mMyThreadId, ANDROID_PRIORITY_NORMAL);
                }
            } else {
                if (curPrio >= ANDROID_PRIORITY_BACKGROUND) {
                    // We want to use the inherited priority from the caller.
                    // Ensure this thread is in the background scheduling class,
                    // since the driver won't modify scheduling classes for us.
                    // The scheduling group is reset to default by the caller
                    // once this method returns after the transaction is complete.
                    set_sched_policy(mMyThreadId, SP_BACKGROUND);
                }
            }

            //ALOGI(">>>> TRANSACT from pid %d uid %d\n", mCallingPid, mCallingUid);
            
            Parcel reply;
            。。。。。。
            if (tr.target.ptr) {
                sp<BBinder> b((BBinder*)tr.cookie);
                const status_t error = b->transact(tr.code, buffer, &reply, tr.flags);
                if (error < NO_ERROR) 
					reply.setError(error);

            } else {
                const status_t error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
                if (error < NO_ERROR) reply.setError(error);
            }
            
            if ((tr.flags & TF_ONE_WAY) == 0) {
                LOG_ONEWAY("Sending reply to %d!", mCallingPid);
                sendReply(reply, 0);
            } else {
                LOG_ONEWAY("NOT sending reply to %d!", mCallingPid);
            }
            
            mCallingPid = origPid;
            mCallingUid = origUid;

            。。。。。。
            
        }
        break;
    
    case BR_DEAD_BINDER:
        {
            BpBinder *proxy = (BpBinder*)mIn.readInt32();
            proxy->sendObituary();
            mOut.writeInt32(BC_DEAD_BINDER_DONE);
            mOut.writeInt32((int32_t)proxy);
        } break;
        
    case BR_CLEAR_DEATH_NOTIFICATION_DONE:
        {
            BpBinder *proxy = (BpBinder*)mIn.readInt32();
            proxy->getWeakRefs()->decWeak(proxy);
        } break;
        
    case BR_FINISHED:
        result = TIMED_OUT;
        break;
        
    case BR_NOOP:
        break;
        
    case BR_SPAWN_LOOPER:
        mProcess->spawnPooledThread(false);
        break;
        
    default:
        printf("*** BAD COMMAND %d received from Binder driver\n", cmd);
        result = UNKNOWN_ERROR;
        break;
    }

    if (result != NO_ERROR) {
        mLastError = result;
    }
    
    return result;
}














