void MediaPlayerService::instantiate() {
    defaultServiceManager()->addService(String16("media.player"), new MediaPlayerService());
}


//其中defaultServiceManager()返回值为 gDefaultServiceManager = new BpServiceManager(BpBinder(0));
//这里impl = BpBinder(0)
BpServiceManager(const sp<IBinder>& impl)
: BpInterface<IServiceManager>(impl) //BpInterface<IServiceManager>(BpBinder(0)) 
{
}



//	frameworks/native/include/binder/IInterface.h
template<typename INTERFACE>
class BpInterface : public INTERFACE, public BpRefBase
{
public:
                                BpInterface(const sp<IBinder>& remote);//这里remote = BpBinder(0)

protected:
    virtual IBinder*            onAsBinder();
};


//	frameworks/native/include/binder/IInterface.h
template<typename INTERFACE>
inline BpInterface<INTERFACE>::BpInterface(const sp<IBinder>& remote)
    : BpRefBase(remote)//BpRefBase frameworks/native/include/binder/Binder.h  //这里remote = BpBinder(0)
{
}


//	frameworks/native/libs/binder/Binder.cpp
BpRefBase::BpRefBase(const sp<IBinder>& o)
    : mRemote(o.get()), mRefs(NULL), mState(0)  //这里remote = BpBinder(0)
{
    extendObjectLifetime(OBJECT_LIFETIME_WEAK);

    if (mRemote) {
        mRemote->incStrong(this);           // Removed on first IncStrong().
        mRefs = mRemote->createWeak(this);  // Held for our entire lifetime.
    }
}

inline  IBinder*        BpRefBase::remote()                { return mRemote; }
inline  IBinder*        BpRefBase::remote() const          { return mRemote; }



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	frameworks/native/libs/binder/IServiceManager.cpp
//默认值allowIsolated = false
virtual status_t BpServiceManager::addService(const String16& name, const sp<IBinder>& service,bool allowIsolated)
{
	Parcel data, reply;//创建临时变量
	data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
	data.writeString16(name);
	data.writeStrongBinder(service);
	data.writeInt32(allowIsolated ? 1 : 0);//目前不明白有什么作用
	status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply); //remote()返回 BpBinder(0) => BpBinder->transact(ADD_SERVICE_TRANSACTION, data, &reply);
	return err == NO_ERROR ? reply.readExceptionCode() : err;
}


// Write RPC headers.  (previously just the interface token)
status_t Parcel::writeInterfaceToken(const String16& interface)
{
    //getStrictModePolicy 这个接口有何作用，目前不知？
    writeInt32(IPCThreadState::self()->getStrictModePolicy() | STRICT_MODE_PENALTY_GATHER);
    // currently the interface identification token is just its name as a string
    return writeString16(interface);
}


status_t Parcel::writeString16(const String16& str)
{
    return writeString16(str.string(), str.size());
}

status_t Parcel::writeString16(const char16_t* str, size_t len)
{
    if (str == NULL) return writeInt32(-1);
    
    status_t err = writeInt32(len);
    if (err == NO_ERROR) {
        len *= sizeof(char16_t);
        uint8_t* data = (uint8_t*)writeInplace(len+sizeof(char16_t));
        if (data) {
            memcpy(data, str, len);
            *reinterpret_cast<char16_t*>(data+len) = 0;
            return NO_ERROR;
        }
        err = mError;
    }
    return err;
}




status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}

//	bionic/libc/kernel/common/uapi/binder.h:39
/*
 * This is the flattened representation of a Binder object for transfer
 * between processes.  The 'offsets' supplied as part of a binder transaction
 * contains offsets into the data where these structures occur.  The Binder
 * driver takes care of re-writing the structure type and data as it moves
 * between processes.
 */
struct flat_binder_object {
	/* 8 bytes for large_flat_header. */
	unsigned long		type;
	unsigned long		flags;
 
	/* 8 bytes of data. */
	union {
		void		*binder;	/* local object */
		signed long	handle;		/* remote object */
	};
 
	/* extra data associated with local object */
	void			*cookie;
};


status_t flatten_binder(const sp<ProcessState>& proc,const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;
    
    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    if (binder != NULL) {
        IBinder *local = binder->localBinder();
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            if (proxy == NULL) {
                ALOGE("null proxy");
            }
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.type = BINDER_TYPE_HANDLE;
            obj.handle = handle;
            obj.cookie = NULL;
        } else {
	    //执行这里
            obj.type = BINDER_TYPE_BINDER;
            obj.binder = local->getWeakRefs();
            obj.cookie = local;
        }
    } else {
        obj.type = BINDER_TYPE_BINDER;
        obj.binder = NULL;
        obj.cookie = NULL;
    }
    
    return finish_flatten_binder(binder, obj, out);
}


BBinder* BBinder::localBinder()
{
    return this;
}



inline static status_t finish_flatten_binder(const sp<IBinder>& binder, const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}


//这里为临时变量
status_t Parcel::writeObject(const flat_binder_object& val, bool nullMetaData)
{

    //检测是否够存储flat_binder_object
    const bool enoughData = (mDataPos+sizeof(val)) <= mDataCapacity;
    const bool enoughObjects = mObjectsSize < mObjectsCapacity;


    if (enoughData && enoughObjects) {
restart_write:
        *reinterpret_cast<flat_binder_object*>(mData+mDataPos) = val;
        
        // Need to write meta-data?
        if (nullMetaData || val.binder != NULL) {
            mObjects[mObjectsSize] = mDataPos;
            acquire_object(ProcessState::self(), val, this);
            mObjectsSize++;
        }
        
        // remember if it's a file descriptor
        if (val.type == BINDER_TYPE_FD) {//这里 val.type = BINDER_TYPE_BINDER
            if (!mAllowFds) {
                return FDS_NOT_ALLOWED;
            }
            mHasFds = mFdsKnown = true;
        }

        return finishWrite(sizeof(flat_binder_object));
    }

    if (!enoughData) {
        const status_t err = growData(sizeof(val));
        if (err != NO_ERROR) return err;
    }
    if (!enoughObjects) {
        size_t newSize = ((mObjectsSize+2)*3)/2;
        size_t* objects = (size_t*)realloc(mObjects, newSize*sizeof(size_t));
        if (objects == NULL) return NO_MEMORY;
        mObjects = objects;
        mObjectsCapacity = newSize;
    }
    
    goto restart_write;
}

void acquire_object(const sp<ProcessState>& proc,const flat_binder_object& obj, const void* who)
{
    switch (obj.type) {
        case BINDER_TYPE_BINDER://运行这里
            if (obj.binder) {
                LOG_REFS("Parcel %p acquiring reference on local %p", who, obj.cookie);
                static_cast<IBinder*>(obj.cookie)->incStrong(who);//增加计数
            }
            return;
       。。。。。。
    }

    ALOGD("Invalid object type 0x%08lx", obj.type);
}

status_t Parcel::finishWrite(size_t len)
{
    //printf("Finish write of %d\n", len);
    mDataPos += len;
    ALOGV("finishWrite Setting data pos of %p to %d\n", this, mDataPos);
    if (mDataPos > mDataSize) {
        mDataSize = mDataPos;
        ALOGV("finishWrite Setting data size of %p to %d\n", this, mDataSize);
    }
    //printf("New pos=%d, size=%d\n", mDataPos, mDataSize);
    return NO_ERROR;
}









/////////////////////////////////////////////////////////////////////////////////////
//status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply); //remote()返回 BpBinder(0)

status_t BpBinder::transact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    // Once a binder has died, it will never come back to life.
    if (mAlive) {
        status_t status = IPCThreadState::self()->transact(mHandle, code, data, reply, flags);//这里mHandle = 0
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }

    return DEAD_OBJECT;
}


status_t IPCThreadState::transact(int32_t handle,uint32_t code, const Parcel& data,Parcel* reply, uint32_t flags)
{
    status_t err = data.errorCheck();

    flags |= TF_ACCEPT_FDS;

	。。。。。。
    
    if (err == NO_ERROR) {
        LOG_ONEWAY(">>>> SEND from pid %d uid %d %s", getpid(), getuid(),(flags & TF_ONE_WAY) == 0 ? "READ REPLY" : "ONE WAY");
        err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);//分析  注意这里写入命令BC_TRANSACTION
    }
    
    if (err != NO_ERROR) {
        if (reply) reply->setError(err);
        return (mLastError = err);
    }
    
    if ((flags & TF_ONE_WAY) == 0) {//非TF_ONE_WAY
		。。。。。。
        if (reply) {//reply不为空
           err = waitForResponse(reply);//运行这里
        } else {
           。。。。。。
        }
		。。。。。。
    } else {// TF_ONE_WAY
        err = waitForResponse(NULL, NULL);
    }
    
    return err;
}




//将数据写入binder_transaction_data结构中
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;

    tr.target.handle = handle;
    tr.code = code;
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;
    
    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();
        tr.data.ptr.buffer = data.ipcData();
        tr.offsets_size = data.ipcObjectsCount()*sizeof(size_t);
        tr.data.ptr.offsets = data.ipcObjects();
    } else if (statusBuffer) {
       。。。。。。
    } else {
        return (mLastError = err);
    }
    
    mOut.writeInt32(cmd);//BC_TRANSACTION
    mOut.write(&tr, sizeof(tr));
    
    return NO_ERROR;
}


struct binder_transaction_data {
	/* The first two are only used for bcTRANSACTION and brTRANSACTION,
	 * identifying the target and contents of the transaction.
	 */
	union {
		size_t	handle;	/* target descriptor of command transaction */
		void	*ptr;	/* target descriptor of return transaction */
	} target;
	void		*cookie;	/* target object cookie */
	unsigned int	code;		/* transaction command */
 
	/* General information about the transaction. */
	unsigned int	flags;
	pid_t		sender_pid;
	uid_t		sender_euid;
	size_t		data_size;	/* number of bytes of data */
	size_t		offsets_size;	/* number of bytes of offsets */
 
	/* If this transaction is inline, the data immediately
	 * follows here; otherwise, it ends with a pointer to
	 * the data buffer.
	 */
	union {
		struct {
			/* transaction data */
			const void	*buffer;
			/* offsets from buffer to flat_binder_object structs */
			const void	*offsets;
		} ptr;
		uint8_t	buf[8];
	} data;
};



size_t Parcel::ipcDataSize() const
{
    return (mDataSize > mDataPos ? mDataSize : mDataPos);
}

const uint8_t* Parcel::ipcData() const
{
    return mData;
}


size_t Parcel::ipcObjectsCount() const
{
    return mObjectsSize;
}


const size_t* Parcel::ipcObjects() const
{
    return mObjects;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//acquireResult默认acquireResult=NULL
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    int32_t cmd;
    int32_t err;

    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break;

        。。。。。。
    }

finish:
    if (err != NO_ERROR) {
        if (acquireResult) *acquireResult = err;
        if (reply) reply->setError(err);
        mLastError = err;
    }
    
    return err;
}



//默认值bool doReceive=true
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }
    
    binder_write_read bwr;
    
    // Is the read buffer empty?
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    
    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    
    bwr.write_size = outAvail;
    bwr.write_buffer = (long unsigned int)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {//这里理论上doReceive和needRead均为1
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (long unsigned int)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }
    
    // Return immediately if there is nothing to do.
	//(最后bwr.write_size和bwr.read_size均为0，IPCThreadState::talkWithDriver函数什么也不做)
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
        IF_LOG_COMMANDS() {
            alog << "About to read/write, write size = " << mOut.dataSize() << endl;
        }
#if defined(HAVE_ANDROID_OS)
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
        。。。。。。。
    } while (err == -EINTR);

    。。。。。。
    return err;
}







//////////////////////////////////////////////////////////////////////////////
//数据写入ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr)驱动分析
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	/*pr_info("binder_ioctl: %d:%d %x %lx\n", proc->pid, current->pid, cmd, arg);*/

	trace_binder_ioctl(cmd, arg);

	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);//分析？？？？？
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
		//把用户空间数据拷贝到临时变量bwr
		if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		binder_debug(BINDER_DEBUG_READ_WRITE,"%d:%d write %lld at %016llx, read %lld at %016llx\n",
			     proc->pid, thread->pid,(u64)bwr.write_size, (u64)bwr.write_buffer,(u64)bwr.read_size, (u64)bwr.read_buffer);

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
			ret = binder_thread_read(proc, thread, bwr.read_buffer, bwr.read_size, &bwr.read_consumed, filp->f_flags & O_NONBLOCK);
			trace_binder_read_done(ret);
			if (!list_empty(&proc->todo))
				wake_up_interruptible(&proc->wait);
			if (ret < 0) {
				if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
					ret = -EFAULT;
				goto err;
			}
		}
		binder_debug(BINDER_DEBUG_READ_WRITE,
			     "%d:%d wrote %lld of %lld, read return %lld of %lld\n",
			     proc->pid, thread->pid,
			     (u64)bwr.write_consumed, (u64)bwr.write_size,
			     (u64)bwr.read_consumed, (u64)bwr.read_size);
				 
		//bwr拷贝回用户空间
		if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		break;
	}
		。。。。。。。
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



/** https://blog.csdn.net/djinglan/article/details/8150444
 * wait_event_interruptible - sleep until a condition gets true
 * @wq: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 *
 * The process is put to sleep (TASK_INTERRUPTIBLE) until the
 * @condition evaluates to true or a signal is received.
 * The @condition is checked each time the waitqueue @wq is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * The function will return -ERESTARTSYS if it was interrupted by a
 * signal and 0 if @condition evaluated to true.
 */
#define wait_event_interruptible(wq, condition)				\
({									\
	int __ret = 0;							\
	if (!(condition))						\
		__wait_event_interruptible(wq, condition, __ret);	\
	__ret;								\
})

#define __wait_event_interruptible(wq, condition, ret)			\
do {									\
	DEFINE_WAIT(__wait);						\
									\
	for (;;) {							\
		prepare_to_wait(&wq, &__wait, TASK_INTERRUPTIBLE);	\
		if (condition)						\
			break;						\
		if (!signal_pending(current)) {				\
			schedule();					\
			continue;					\
		}							\
		ret = -ERESTARTSYS;					\
		break;							\
	}								\
	finish_wait(&wq, &__wait);					\
} while (0)

#define DEFINE_WAIT_FUNC(name, function)				\
wait_queue_t name = {						\
	.private	= current,				\
	.func		= function,				\
	.task_list	= LIST_HEAD_INIT((name).task_list),	\
}

#define DEFINE_WAIT(name) DEFINE_WAIT_FUNC(name, autoremove_wake_function)



//这里同样是mediaserver首次调用，所以*p == NULL
static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
	struct binder_thread *thread = NULL;
	struct rb_node *parent = NULL;
	struct rb_node **p = &proc->threads.rb_node;

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


struct binder_stats {
	int br[_IOC_NR(BR_FAILED_REPLY) + 1];
	int bc[_IOC_NR(BC_DEAD_BINDER_DONE) + 1];
	int obj_created[BINDER_STAT_COUNT];
	int obj_deleted[BINDER_STAT_COUNT];
};


int binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,binder_uintptr_t binder_buffer, size_t size,binder_size_t *consumed)
{
	uint32_t cmd;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	while (ptr < end && thread->return_error == BR_OK) {
		//读取cmd值,这里cmd==BC_TRANSACTION
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
		case BC_TRANSACTION:
		case BC_REPLY: {
			struct binder_transaction_data tr;

			if (copy_from_user(&tr, ptr, sizeof(tr)))
				return -EFAULT;
			ptr += sizeof(tr);
			binder_transaction(proc, thread, &tr, cmd == BC_REPLY);//这里cmd == BC_TRANSACTION 
			break;
		}
		。。。。。。
		}
		*consumed = ptr - buffer;
	}
	return 0;
}


//reply = false;
static void binder_transaction(struct binder_proc *proc,struct binder_thread *thread,struct binder_transaction_data *tr, int reply)
{
	struct binder_transaction *t;//
	struct binder_work *tcomplete;
	binder_size_t *offp, *off_end;
	binder_size_t off_min;
	struct binder_proc *target_proc;//保存数据流向的目标进程（binder是以单实例来操作的，每个进程一个binder_proc）
	struct binder_thread *target_thread = NULL;
	struct binder_node *target_node = NULL;//保存目标binder_node，这里直接是binder_context_mgr_node
	struct list_head *target_list;//保存目标事务链表
	wait_queue_head_t *target_wait;//保存等待队列为ServiceManager循环读取数据，一直阻塞的等待队列
	struct binder_transaction *in_reply_to = NULL;
	struct binder_transaction_log_entry *e;
	uint32_t return_error;

	e = binder_transaction_log_add(&binder_transaction_log);
	e->call_type = reply ? 2 : !!(tr->flags & TF_ONE_WAY);
	e->from_proc = proc->pid;
	e->from_thread = thread->pid;
	e->target_handle = tr->target.handle;
	e->data_size = tr->data_size;
	e->offsets_size = tr->offsets_size;

	if (reply) {
		。。。。。。
	} else {
		//tr->target.handle==0这里为ServiceManager 
		if (tr->target.handle) {
			。。。。。。
		} else {
			//得到ServiceManager的binder_node
			target_node = binder_context_mgr_node;
			if (target_node == NULL) {
				return_error = BR_DEAD_REPLY;
				goto err_no_context_mgr_node;
			}
		}
		e->to_node = target_node->debug_id;
		target_proc = target_node->proc;
		if (target_proc == NULL) {
			return_error = BR_DEAD_REPLY;
			goto err_dead_binder;
		}
		if (security_binder_transaction(proc->tsk, target_proc->tsk) < 0) {//
			return_error = BR_FAILED_REPLY;
			goto err_invalid_target_handle;
		}

		//mediaserver首次调用thread->transaction_stack 为 NULL，这里下面代码不执行
		if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {
			。。。。。。
		}
	}
	if (target_thread) {
		。。。。。。
	} else {
		target_list = &target_proc->todo;//目标事务链表
		target_wait = &target_proc->wait;//等待队列，这里这个等待队列为ServiceManager循环读取数据，一直阻塞的等待队列
	}
	e->to_proc = target_proc->pid;

	/* TODO: reuse incoming transaction for reply */
	//分配了一个待处理事务t
	t = kzalloc(sizeof(*t), GFP_KERNEL);//binder_transaction
	if (t == NULL) {
		return_error = BR_FAILED_REPLY;
		goto err_alloc_t_failed;
	}
	binder_stats_created(BINDER_STAT_TRANSACTION);

	//一个待完成工作项tcomplete
	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);//binder_work
	if (tcomplete == NULL) {
		return_error = BR_FAILED_REPLY;
		goto err_alloc_tcomplete_failed;
	}
	binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);

	。。。。。。

	if (!reply && !(tr->flags & TF_ONE_WAY))
		t->from = thread;
	else
		t->from = NULL;
	t->sender_euid = proc->tsk->cred->euid;//发送进程UID
	t->to_proc = target_proc;
	t->to_thread = target_thread;//注意这里target_thread为NULL
	t->code = tr->code;
	t->flags = tr->flags;
	t->priority = task_nice(current);

	trace_binder_transaction(reply, t, target_node);

	t->buffer = binder_alloc_buf(target_proc, tr->data_size,tr->offsets_size, !reply && (t->flags & TF_ONE_WAY));//
	if (t->buffer == NULL) {
		return_error = BR_FAILED_REPLY;
		goto err_binder_alloc_buf_failed;
	}
	t->buffer->allow_user_free = 0;
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t;
	t->buffer->target_node = target_node;
	trace_binder_transaction_alloc_buf(t->buffer);
	
	//由于现在target_node要被使用了，增加它的引用计数
	if (target_node)
		binder_inc_node(target_node, 1, 0, NULL);

	offp = (binder_size_t *)(t->buffer->data + ALIGN(tr->data_size, sizeof(void *)));

	if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t) tr->data.ptr.buffer, tr->data_size)) {
		binder_user_error("%d:%d got transaction with invalid data ptr\n", proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		goto err_copy_data_failed;
	}
	if (copy_from_user(offp, (const void __user *)(uintptr_t)tr->data.ptr.offsets, tr->offsets_size)) {
		binder_user_error("%d:%d got transaction with invalid offsets ptr\n",proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		goto err_copy_data_failed;
	}
	if (!IS_ALIGNED(tr->offsets_size, sizeof(binder_size_t))) {
		binder_user_error("%d:%d got transaction with invalid offsets size, %lld\n",proc->pid, thread->pid, (u64)tr->offsets_size);
		return_error = BR_FAILED_REPLY;
		goto err_bad_offset;
	}
	off_end = (void *)offp + tr->offsets_size;
	off_min = 0;
	for (; offp < off_end; offp++) {
		struct flat_binder_object *fp;
		if (*offp > t->buffer->data_size - sizeof(*fp) ||
		    *offp < off_min ||
		    t->buffer->data_size < sizeof(*fp) ||
		    !IS_ALIGNED(*offp, sizeof(u32))) {
			。。。。。。
			return_error = BR_FAILED_REPLY;
			goto err_bad_offset;
		}
		fp = (struct flat_binder_object *)(t->buffer->data + *offp);
		off_min = *offp + sizeof(struct flat_binder_object);
		switch (fp->type) {
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct binder_ref *ref;
			//  由于是第一次在Binder驱动程序中传输这个MediaPlayerService，调用binder_get_node函数查询这个Binder实体时，会返回空，
			//  于是binder_new_node在proc中新建一个，下次就可以直接使用了
			struct binder_node *node = binder_get_node(proc, fp->binder);
			if (node == NULL) {
				node = binder_new_node(proc, fp->binder, fp->cookie);
				if (node == NULL) {
					return_error = BR_FAILED_REPLY;
					goto err_binder_new_node_failed;
				}
				node->min_priority = fp->flags & FLAT_BINDER_FLAG_PRIORITY_MASK;
				node->accept_fds = !!(fp->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS);
			}
			if (fp->cookie != node->cookie) {
				binder_user_error("%d:%d sending u%016llx node %d, cookie mismatch %016llx != %016llx\n",
					proc->pid, thread->pid,
					(u64)fp->binder, node->debug_id,
					(u64)fp->cookie, (u64)node->cookie);
				goto err_binder_get_ref_for_node_failed;
			}
			if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
				return_error = BR_FAILED_REPLY;
				goto err_binder_get_ref_for_node_failed;
			}
			//注意，到了这里的时候，t->buffer中的flat_binder_obj的type已经改为BINDER_TYPE_HANDLE，handle已经改为ref->desc，
			//跟原来不一样了，因为这个flat_binder_obj是最终是要传给Service Manager的，而Service Manager只能够通过句柄值来引用这个Binder实体。
			ref = binder_get_ref_for_node(target_proc, node);
			if (ref == NULL) {
				return_error = BR_FAILED_REPLY;
				goto err_binder_get_ref_for_node_failed;
			}
			if (fp->type == BINDER_TYPE_BINDER)
				fp->type = BINDER_TYPE_HANDLE;
			else
				fp->type = BINDER_TYPE_WEAK_HANDLE;
			fp->handle = ref->desc;
			//通过binder_inc_ref来增加这个引用计数，防止这个引用还在使用过程当中就被销毁
			binder_inc_ref(ref, fp->type == BINDER_TYPE_HANDLE,&thread->todo);

			trace_binder_transaction_node_to_ref(t, node, ref);
			binder_debug(BINDER_DEBUG_TRANSACTION,"        node %d u%016llx -> ref %d desc %d\n",node->debug_id, (u64)node->ptr,ref->debug_id, ref->desc);
		} break;
		
	}
	if (reply) {
		。。。。。。
	} else if (!(t->flags & TF_ONE_WAY)) {
		BUG_ON(t->buffer->async_transaction != 0);
		t->need_reply = 1;//标志需要回应
		t->from_parent = thread->transaction_stack;
		thread->transaction_stack = t;  //////////////////注意这里保存了插入目标binder中的binder_transaction
	} else {
		。。。。。。
	}
	
	
	t->work.type = BINDER_WORK_TRANSACTION;
	list_add_tail(&t->work.entry, target_list);//把待处理事务加入到target_list列表中 （target_list = &target_proc->todo;）
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	//待完成工作项加入到本线程的todo等待执行列表中
	list_add_tail(&tcomplete->entry, &thread->todo);
	
	//唤醒目标等待队列
	if (target_wait)
		wake_up_interruptible(target_wait);
	
	return;

	。。。。。。
}





//这里一共往用户传进来的缓冲区buffer写入了两个整数，分别是BR_NOOP和BR_TRANSACTION_COMPLETE
static int binder_thread_read(struct binder_proc *proc,struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,binder_size_t *consumed, 
			      int non_block)
{
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	int ret = 0;
	int wait_for_proc_work;

	if (*consumed == 0) {
		//写入BR_NOOP
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}

retry:
	//这里，thread->transaction_stack和thread->todo均不为空，于是wait_for_proc_work为false
	//thread->transaction_stack ???
	//thread->todo均不为空 是在 binder_thread_write中插入类型为BINDER_WORK_TRANSACTION_COMPLETE
	wait_for_proc_work = thread->transaction_stack == NULL && list_empty(&thread->todo);

	。。。。。。

	thread->looper |= BINDER_LOOPER_STATE_WAITING;
	if (wait_for_proc_work)
		proc->ready_threads++;

	binder_unlock(__func__);

	trace_binder_wait_for_work(wait_for_proc_work, !!thread->transaction_stack, !list_empty(&thread->todo));

	//wait_for_proc_work为false
	if (wait_for_proc_work) {
		。。。。。。
	} else {
		if (non_block) {
			。。。。。。
		} else{
			//wait_event_freezable根据 binder_has_thread_work的条件，如果为true立即返回，如果为false 则等待
			ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
		}
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
			w = list_first_entry(&thread->todo, struct binder_work, entry);//第一次进来执行这里
		else if (!list_empty(&proc->todo) && wait_for_proc_work)
			w = list_first_entry(&proc->todo, struct binder_work, entry);
		else {
			if (ptr - buffer == 4 && !(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN)) /* no data added */
				goto retry;
			break;//这里跳出死循环
		}

		if (end - ptr < sizeof(tr) + 4)
			break;

		switch (w->type) {
		。。。。。。
		case BINDER_WORK_TRANSACTION_COMPLETE: {
			//写入	BR_TRANSACTION_COMPLETE
			cmd = BR_TRANSACTION_COMPLETE;
			if (put_user(cmd, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);

			binder_stat_br(proc, thread, cmd);
			binder_debug(BINDER_DEBUG_TRANSACTION_COMPLETE,"%d:%d BR_TRANSACTION_COMPLETE\n",proc->pid, thread->pid);

			//移除队列
			list_del(&w->entry);
			kfree(w);
			binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
		} break;
		。。。。。。
		}

		//这里t一直为NULL所以直接返回
		if (!t)
			continue;

		。。。。。。
	}

done:

	*consumed = ptr - buffer;
	if (proc->requested_threads + proc->ready_threads == 0 &&
	    proc->requested_threads_started < proc->max_threads &&
	    (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
	     BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
	     /*spawn a new thread if we leave this out */) {
		proc->requested_threads++;
		binder_debug(BINDER_DEBUG_THREADS,"%d:%d BR_SPAWN_LOOPER\n",proc->pid, thread->pid);
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	}
	return 0;
}


static int binder_has_thread_work(struct binder_thread *thread)
{
	//!list_empty(&thread->todo) 为 true
	return !list_empty(&thread->todo) || thread->return_error != BR_OK ||
		(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN);
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//这里分析完binder_ioctl，addService传输完成，但是服务层ServiceManager未能返回；
//这里回到talkWithDriver函数，返回的buffer中写入BR_NOOP和BR_TRANSACTION_COMPLETE

//默认值bool doReceive=true
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    。。。。。。

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
        IF_LOG_COMMANDS() {
            alog << "About to read/write, write size = " << mOut.dataSize() << endl;
        }
#if defined(HAVE_ANDROID_OS)
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
        。。。。。。
    } while (err == -EINTR);

	。。。。。。

    if (err >= NO_ERROR) {

		//发送数据处理
        if (bwr.write_consumed > 0) {//把mOut的数据清空
            if (bwr.write_consumed < (ssize_t)mOut.dataSize())
                mOut.remove(0, bwr.write_consumed);
            else
                mOut.setDataSize(0);
        }

		//接收数据处理
        if (bwr.read_consumed > 0) {
            mIn.setDataSize(bwr.read_consumed);
            mIn.setDataPosition(0);
        }
        。。。。。。
        return NO_ERROR;
    }
    
    return err;
}

//返回上层调用函数waitForResponse
//acquireResult默认acquireResult=NULL
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    int32_t cmd;
    int32_t err;

    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break;////

        err = mIn.errorCheck();
        if (err < NO_ERROR) break;
        if (mIn.dataAvail() == 0) continue;
        
        cmd = mIn.readInt32();
		。。。。。。
        switch (cmd) {
		。。。。。。。
        default:
	    //从mIn读出一个整数，这个便是BR_NOOP了,空操作，什么不做，继续进入talkWithDriver()
            err = executeCommand(cmd);
            if (err != NO_ERROR) goto finish;
            break;
        }
    }

finish:
    if (err != NO_ERROR) {
        if (acquireResult) *acquireResult = err;
        if (reply) reply->setError(err);
        mLastError = err;
    }
    
    return err;
}


status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;
    
    switch (cmd) {
    case BR_NOOP://什么不做，返回waitForResponse，在次进入talkWithDriver
        break;
    }

    if (result != NO_ERROR) {
        mLastError = result;
    }
    
    return result;
}


//默认值bool doReceive=true
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }
    
    binder_write_read bwr;
    
    // Is the read buffer empty?
	//mIn中还有BR_TRANSACTION_COMPLETE所以needRead为false
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    
    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
	//outAvail 为 0
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    
    bwr.write_size = outAvail;
    bwr.write_buffer = (long unsigned int)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (long unsigned int)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }

    。。。。。。。
    
    // Return immediately if there is nothing to do.
	//由于(bwr.write_size == 0) && (bwr.read_size == 0) 满足 直接返回waitForResponse
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;
	
    。。。。。。
    
    return err;
}


//返回上层调用函数waitForResponse，解析命令BR_TRANSACTION_COMPLETE
//acquireResult默认acquireResult=NULL
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    int32_t cmd;
    int32_t err;

    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break;////

        err = mIn.errorCheck();
        if (err < NO_ERROR) break;
        if (mIn.dataAvail() == 0) continue;
        
        cmd = mIn.readInt32();
        
        IF_LOG_COMMANDS() {
            alog << "Processing waitForResponse Command: " << getReturnString(cmd) << endl;
        }

        switch (cmd) {
        case BR_TRANSACTION_COMPLETE:
	    //由于reply不为空，acquireResult默认为NULL，所以继续循环，返回进入talkWithDriver
            if (!reply && !acquireResult) goto finish;
            break;
		。。。。。。
        }
    }

finish:
    if (err != NO_ERROR) {
        if (acquireResult) *acquireResult = err;
        if (reply) reply->setError(err);
        mLastError = err;
    }
    
    return err;
}


status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }
    
    binder_write_read bwr;
    
    // Is the read buffer empty?
	// needRead 为 true
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    
    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
	//mOut 这里当传输完成时，就被mOut.setDataSize(0),outAvail为0
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    
    bwr.write_size = outAvail;//outAvail为0
    bwr.write_buffer = (long unsigned int)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (long unsigned int)mIn.data();
    } else {
        。。。。。。
    }

	。。。。。。

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
		。。。。。。
#if defined(HAVE_ANDROID_OS)
		//这里写入缓存大小为0，读取数据缓存大小不为0，再次进入驱动调用
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
		。。。。。。
    } while (err == -EINTR);
	
	。。。。。。
    
    return err;
}



static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	int ret = 0;
	int wait_for_proc_work;

	if (*consumed == 0) {
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}

retry:
	//thread->transaction_stack不为NULL(在上次发送时thread->transaction_stack赋值)，thread->todo为空，即wait_for_proc_work为false
	wait_for_proc_work = thread->transaction_stack == NULL && list_empty(&thread->todo);

	if (thread->return_error != BR_OK && ptr < end) {
		。。。。。。
	}


	thread->looper |= BINDER_LOOPER_STATE_WAITING;
	if (wait_for_proc_work)
		proc->ready_threads++;

	binder_unlock(__func__);

	trace_binder_wait_for_work(wait_for_proc_work, !!thread->transaction_stack, !list_empty(&thread->todo));
	if (wait_for_proc_work) {
		。。。。。。
	} else {
		if (non_block) {
			。。。。。。
		} else{
			//一直等待数据，等待ServiceManager回传数据，往等待队列中插入binder_work
			ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
		}
	}

	。。。。。。
	
	return 0;
}


////////////////////////////////////////////////////////////////////////////
//这里我们回到ServiceManager中 service_manager_note.c 继续分析























