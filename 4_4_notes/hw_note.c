//	hardware/interfaces/audio/2.0/default/service.cpp

int main(int /* argc */, char* /* argv */ []) {
    configureRpcThreadpool(16, true /*callerWillJoin*/);  //第一步
    android::status_t status;
    status = registerPassthroughServiceImplementation<IDevicesFactory>();
    LOG_ALWAYS_FATAL_IF(status != OK, "Error while registering audio service: %d", status);
    status = registerPassthroughServiceImplementation<IEffectsFactory>();
    LOG_ALWAYS_FATAL_IF(status != OK, "Error while registering audio effects service: %d", status);
    // Soundtrigger and FM radio might be not present.
    status = registerPassthroughServiceImplementation<ISoundTriggerHw>();
    ALOGE_IF(status != OK, "Error while registering soundtrigger service: %d", status);
    if (useBroadcastRadioFutureFeatures) {
        status = registerPassthroughServiceImplementation<broadcastradio::V1_1::IBroadcastRadioFactory>();
    } else {
        status = registerPassthroughServiceImplementation<broadcastradio::V1_0::IBroadcastRadioFactory>();
    }
    ALOGE_IF(status != OK, "Error while registering fm radio service: %d", status);
    joinRpcThreadpool();
    return status;
}



第一步调用

//	system/libhidl/transport/HidlTransportSupport.cpp	
void configureRpcThreadpool(size_t maxThreads, bool callerWillJoin) {
    // TODO(b/32756130) this should be transport-dependent
    configureBinderRpcThreadpool(maxThreads, callerWillJoin);
}

//	system/libhidl/transport/HidlBinderSupport.cpp
void configureBinderRpcThreadpool(size_t maxThreads, bool callerWillJoin) {
    ProcessState::self()->setThreadPoolConfiguration(maxThreads, callerWillJoin /*callerJoinsPool*/);
}

//ProcessState
//	system/libhwbinder/ProcessState.cpp
sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    gProcess = new ProcessState;
    return gProcess;
}


//	system/libhwbinder/ProcessState.cpp
ProcessState::ProcessState()
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)
    , mExecutingThreadsCount(0)
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
    , mStarvationStartTimeMs(0)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mSpawnThreadOnStart(true)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        //  #define BINDER_VM_SIZE ((1 * 1024 * 1024) - sysconf(_SC_PAGE_SIZE) * 2)
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/hwbinder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
    }
    else {
        ALOGE("Binder driver could not be opened.  Terminating.");
    }
}



static int open_driver()
{
    int fd = open("/dev/hwbinder", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        int vers = 0;
	//获取版版本
        status_t result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            ALOGE("Binder driver protocol(%d) does not match user space protocol(%d)!", vers, BINDER_CURRENT_PROTOCOL_VERSION);
            close(fd);
            fd = -1;
        }

        //设置最大binder线程数
        size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '/dev/hwbinder' failed: %s\n", strerror(errno));
    }
    return fd;
}



////////////////////////////////////////////////////////////////////////////////////////
注册
/**
 * Registers passthrough service implementation.
 */
template<class Interface>
__attribute__((warn_unused_result))
status_t registerPassthroughServiceImplementation(std::string name = "default") {
    sp<Interface> service = Interface::getService(name, true /* getStub */);

    if (service == nullptr) {
        ALOGE("Could not get passthrough implementation for %s/%s.",Interface::descriptor, name.c_str());
        return EXIT_FAILURE;
    }

    LOG_FATAL_IF(service->isRemote(), "Implementation of %s/%s is remote!",Interface::descriptor, name.c_str());

    status_t status = service->registerAsService(name);

    if (status == OK) {
        ALOGI("Registration complete for %s/%s.",Interface::descriptor, name.c_str());
    } else {
        ALOGE("Could not register service %s/%s (%d).",Interface::descriptor, name.c_str(), status);
    }

    return status;
}


以IDevicesFactory为例：
__attribute__((warn_unused_result))
status_t registerPassthroughServiceImplementation(std::string name = "default") {
    sp<IDevicesFactory> service = IDevicesFactory::getService(name, true /* getStub */);

    if (service == nullptr) {
        ALOGE("Could not get passthrough implementation for %s/%s.",IDevicesFactory::descriptor, name.c_str());
        return EXIT_FAILURE;
    }

    LOG_FATAL_IF(service->isRemote(), "Implementation of %s/%s is remote!",IDevicesFactory::descriptor, name.c_str());

    status_t status = service->registerAsService(name);

    if (status == OK) {
        ALOGI("Registration complete for %s/%s.",IDevicesFactory::descriptor, name.c_str());
    } else {
        ALOGE("Could not register service %s/%s (%d).",IDevicesFactory::descriptor, name.c_str(), status);
    }

    return status;
}





// system/libhidl/transport/HidlTransportSupport.cpp
void joinRpcThreadpool() {
    // TODO(b/32756130) this should be transport-dependent
    joinBinderRpcThreadpool();
}

//	system/libhidl/transport/HidlBinderSupport.cpp
void joinBinderRpcThreadpool() {
    IPCThreadState::self()->joinThreadPool();
}



IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) {
restart:
        const pthread_key_t k = gTLS;
	//第一次获取处理值为0
        IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
        if (st) return st;

	
        return new IPCThreadState;
    }

    if (gShutdown) {
        ALOGW("Calling IPCThreadState::self() during shutdown is dangerous, expect a crash.\n");
        return NULL;
    }

    pthread_mutex_lock(&gTLSMutex);
    if (!gHaveTLS) {
	//注意，这里gTLS初始值为0
        int key_create_value = pthread_key_create(&gTLS, threadDestructor);
        if (key_create_value != 0) {
            pthread_mutex_unlock(&gTLSMutex);
            ALOGW("IPCThreadState::self() unable to create TLS key, expect a crash: %s\n",strerror(key_create_value));
            return NULL;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}


IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::self()),
      mMyThreadId(gettid()),		//获取当前线程id
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0)
{
    //在这里把真的IPCThreadState指针设置进去
    pthread_setspecific(gTLS, this);
    clearCaller();

    //设置容量大小
    mIn.setDataCapacity(256);
    mOut.setDataCapacity(256);
}


void IPCThreadState::clearCaller()
{
    //获取当前进程id
    mCallingPid = getpid();
    //获取当前用户id
    mCallingUid = getuid();
}



//isMain默认值为true
void IPCThreadState::joinThreadPool(bool isMain)
{
    LOG_THREADPOOL("**** THREAD %p (PID %d) IS JOINING THE THREAD POOL\n", (void*)pthread_self(), getpid());

    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);

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

    LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%d\n",(void*)pthread_self(), getpid(), result);

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
                BHwBinder* obj = mPendingStrongDerefs[i];
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
            alog << "Processing top-level Command: " << getReturnString(cmd) << endl;
        }

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount++;
        if (mProcess->mExecutingThreadsCount >= mProcess->mMaxThreads &&  mProcess->mStarvationStartTimeMs == 0) {
            mProcess->mStarvationStartTimeMs = uptimeMillis();
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        result = executeCommand(cmd);

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount--;
        if (mProcess->mExecutingThreadsCount < mProcess->mMaxThreads && mProcess->mStarvationStartTimeMs != 0) {
            int64_t starvationTimeMs = uptimeMillis() - mProcess->mStarvationStartTimeMs;
            if (starvationTimeMs > 100) {
                ALOGE("binder thread pool (%zu threads) starved for %" PRId64 " ms", mProcess->mMaxThreads, starvationTimeMs);
            }
            mProcess->mStarvationStartTimeMs = 0;
        }
        pthread_cond_broadcast(&mProcess->mThreadCountDecrement);
        pthread_mutex_unlock(&mProcess->mThreadCountLock);
    }

    return result;
}




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
    bwr.write_buffer = (uintptr_t)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }

    IF_LOG_COMMANDS() {
        if (outAvail != 0) {
            alog << "Sending commands to driver: " << indent;
            const void* cmds = (const void*)bwr.write_buffer;
            const void* end = ((const uint8_t*)cmds)+bwr.write_size;
            alog << HexDump(cmds, bwr.write_size) << endl;
            while (cmds < end) cmds = printCommand(alog, cmds);
            alog << dedent;
        }
        alog << "Size of receive buffer: " << bwr.read_size << ", needRead: " << needRead << ", doReceive: " << doReceive << endl;
    }

    // Return immediately if there is nothing to do.
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
        IF_LOG_COMMANDS() {
            alog << "About to read/write, write size = " << mOut.dataSize() << endl;
        }
#if defined(__ANDROID__)
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
        if (mProcess->mDriverFD <= 0) {
            err = -EBADF;
        }
        IF_LOG_COMMANDS() {
            alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
        }
    } while (err == -EINTR);

    IF_LOG_COMMANDS() {
        alog << "Our err: " << (void*)(intptr_t)err << ", write consumed: " << bwr.write_consumed << " (of " << mOut.dataSize() << "), read consumed: " << bwr.read_consumed << endl;
    }

    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) {
            if (bwr.write_consumed < mOut.dataSize())
                mOut.remove(0, bwr.write_consumed);
            else
                mOut.setDataSize(0);
        }
        if (bwr.read_consumed > 0) {
            mIn.setDataSize(bwr.read_consumed);
            mIn.setDataPosition(0);
        }
        IF_LOG_COMMANDS() {
            alog << "Remaining data size: " << mOut.dataSize() << endl;
            alog << "Received commands from driver: " << indent;
            const void* cmds = mIn.data();
            const void* end = mIn.data() + mIn.dataSize();
            alog << HexDump(cmds, mIn.dataSize()) << endl;
            while (cmds < end) cmds = printReturnCommand(alog, cmds);
            alog << dedent;
        }
        return NO_ERROR;
    }

    return err;
}





/*
 * On 64-bit platforms where user code may run in 32-bits the driver must
 * translate the buffer (and local binder) addresses appropriately.
 */
//	kernel_imx/include/uapi/linux/android/binder.h
struct binder_write_read {
	binder_size_t		write_size;	/* bytes to write */
	binder_size_t		write_consumed;	/* bytes consumed by driver */
	binder_uintptr_t	write_buffer;
	binder_size_t		read_size;	/* bytes to read */
	binder_size_t		read_consumed;	/* bytes consumed by driver */
	binder_uintptr_t	read_buffer;
};

#ifdef BINDER_IPC_32BIT
typedef __u32 binder_size_t;
typedef __u32 binder_uintptr_t;
#else
typedef __u64 binder_size_t;
typedef __u64 binder_uintptr_t;
#endif






status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BHwBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;
    switch ((uint32_t)cmd) {
    case BR_ERROR:
        result = mIn.readInt32();
        break;

    case BR_OK:
        break;

    case BR_ACQUIRE:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BHwBinder*)mIn.readPointer();
        ALOG_ASSERT(refs->refBase() == obj,"BR_ACQUIRE: object %p does not match cookie %p (expected %p)",refs, obj, refs->refBase());
        obj->incStrong(mProcess.get());
        IF_LOG_REMOTEREFS() {
            LOG_REMOTEREFS("BR_ACQUIRE from driver on %p", obj);
            obj->printRefs();
        }
        mOut.writeInt32(BC_ACQUIRE_DONE);
        mOut.writePointer((uintptr_t)refs);
        mOut.writePointer((uintptr_t)obj);
        break;

    case BR_RELEASE:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BHwBinder*)mIn.readPointer();
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
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BHwBinder*)mIn.readPointer();
        refs->incWeak(mProcess.get());
        mOut.writeInt32(BC_INCREFS_DONE);
        mOut.writePointer((uintptr_t)refs);
        mOut.writePointer((uintptr_t)obj);
        break;

    case BR_DECREFS:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BHwBinder*)mIn.readPointer();
        // NOTE: This assertion is not valid, because the object may no
        // longer exist (thus the (BHwBinder*)cast above resulting in a different
        // memory address).
        //ALOG_ASSERT(refs->refBase() == obj,
        //           "BR_DECREFS: object %p does not match cookie %p (expected %p)",
        //           refs, obj, refs->refBase());
        mPendingWeakDerefs.push(refs);
        break;

    case BR_ATTEMPT_ACQUIRE:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BHwBinder*)mIn.readPointer();

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
            ALOG_ASSERT(result == NO_ERROR,
                "Not enough command data for brTRANSACTION");
            if (result != NO_ERROR) break;

            Parcel buffer;
            buffer.ipcSetDataReference(
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(binder_size_t), freeBuffer, this);

            const pid_t origPid = mCallingPid;
            const uid_t origUid = mCallingUid;
            const int32_t origStrictModePolicy = mStrictModePolicy;
            const int32_t origTransactionBinderFlags = mLastTransactionBinderFlags;

            mCallingPid = tr.sender_pid;
            mCallingUid = tr.sender_euid;
            mLastTransactionBinderFlags = tr.flags;

            //ALOGI(">>>> TRANSACT from pid %d uid %d\n", mCallingPid, mCallingUid);

            Parcel reply;
            status_t error;
            bool reply_sent = false;
            IF_LOG_TRANSACTIONS() {
                alog << "BR_TRANSACTION thr " << (void*)pthread_self()
                    << " / obj " << tr.target.ptr << " / code "
                    << TypeCode(tr.code) << ": " << indent << buffer
                    << dedent << endl
                    << "Data addr = "
                    << reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer)
                    << ", offsets addr="
                    << reinterpret_cast<const size_t*>(tr.data.ptr.offsets) << endl;
            }

            auto reply_callback = [&] (auto &replyParcel) {
                if (reply_sent) {
                    // Reply was sent earlier, ignore it.
                    ALOGE("Dropping binder reply, it was sent already.");
                    return;
                }
                reply_sent = true;
                if ((tr.flags & TF_ONE_WAY) == 0) {
                    replyParcel.setError(NO_ERROR);
                    sendReply(replyParcel, 0);
                } else {
                    ALOGE("Not sending reply in one-way transaction");
                }
            };

            if (tr.target.ptr) {
                // We only have a weak reference on the target object, so we must first try to
                // safely acquire a strong reference before doing anything else with it.
                if (reinterpret_cast<RefBase::weakref_type*>(
                        tr.target.ptr)->attemptIncStrong(this)) {
                    error = reinterpret_cast<BHwBinder*>(tr.cookie)->transact(tr.code, buffer,
                            &reply, tr.flags, reply_callback);
                    reinterpret_cast<BHwBinder*>(tr.cookie)->decStrong(this);
                } else {
                    error = UNKNOWN_TRANSACTION;
                }

            } else {
                error = mContextObject->transact(tr.code, buffer, &reply, tr.flags, reply_callback);
            }

            if ((tr.flags & TF_ONE_WAY) == 0) {
                if (!reply_sent) {
                    // Should have been a reply but there wasn't, so there
                    // must have been an error instead.
                    reply.setError(error);
                    sendReply(reply, 0);
                } else {
                    if (error != NO_ERROR) {
                        ALOGE("transact() returned error after sending reply.");
                    } else {
                        // Ok, reply sent and transact didn't return an error.
                    }
                }
            } else {
                // One-way transaction, don't care about return value or reply.
            }

            //ALOGI("<<<< TRANSACT from pid %d restore pid %d uid %d\n",
            //     mCallingPid, origPid, origUid);


            mCallingPid = origPid;
            mCallingUid = origUid;
            mStrictModePolicy = origStrictModePolicy;
            mLastTransactionBinderFlags = origTransactionBinderFlags;

            IF_LOG_TRANSACTIONS() {
                alog << "BC_REPLY thr " << (void*)pthread_self() << " / obj "
                    << tr.target.ptr << ": " << indent << reply << dedent << endl;
            }

        }
        break;

    case BR_DEAD_BINDER:
        {
            BpHwBinder *proxy = (BpHwBinder*)mIn.readPointer();
            proxy->sendObituary();
            mOut.writeInt32(BC_DEAD_BINDER_DONE);
            mOut.writePointer((uintptr_t)proxy);
        } break;

    case BR_CLEAR_DEATH_NOTIFICATION_DONE:
        {
            BpHwBinder *proxy = (BpHwBinder*)mIn.readPointer();
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




