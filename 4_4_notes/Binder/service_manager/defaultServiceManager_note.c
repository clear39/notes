//以mediaserver为例
int main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);
    char value[PROPERTY_VALUE_MAX];
    bool doLog = (property_get("ro.test_harness", value, "0") > 0) && (atoi(value) == 1);
    pid_t childPid;
    // FIXME The advantage of making the process containing media.log service the parent process of
    // the process that contains all the other real services, is that it allows us to collect more
    // detailed information such as signal numbers, stop and continue, resource usage, etc.
    // But it is also more complex.  Consider replacing this by independent processes, and using
    // binder on death notification instead.
    if (doLog && (childPid = fork()) != 0) {
		//启动一个media.log服务，为父进程
        。。。。。。
    } else {
        // all other services
        if (doLog) {
			//设置父进程media.log服务退出，子进程退出
            prctl(PR_SET_PDEATHSIG, SIGKILL);   // if parent media.log dies before me, kill me also
            setpgid(0, 0);                      // but if I die first, don't kill my parent
        }
        sp<ProcessState> proc(ProcessState::self());
        sp<IServiceManager> sm = defaultServiceManager();
        ALOGI("ServiceManager: %p", sm.get());
        AudioFlinger::instantiate();
        MediaPlayerService::instantiate();
        CameraService::instantiate();
        AudioPolicyService::instantiate();
        registerExtensions();
        ProcessState::self()->startThreadPool();
        IPCThreadState::self()->joinThreadPool();
    }
}



ProcessState::ProcessState()
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // XXX Ideally, there should be a specific define for whether we
        // have mmap (or whether we could possibly have the kernel module
        // availabla).
#if !defined(HAVE_WIN32_IPC)
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/binder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
#else
        mDriverFD = -1;
#endif
    }

    LOG_ALWAYS_FATAL_IF(mDriverFD < 0, "Binder driver could not be opened.  Terminating.");
}


static int open_driver()
{
    int fd = open("/dev/binder", O_RDWR);
    if (fd >= 0) {
	//close on exec, not on-fork, 意为如果对描述符设置了FD_CLOEXEC，使用execl执行的程序里，此描述符被关闭，
	//不能再使用它，但是在使用fork调用的子进程中，此描述符并不关闭，仍可使用。
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        int vers;
        status_t result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            ALOGE("Binder driver protocol does not match user space protocol!");
            close(fd);
            fd = -1;
        }
        size_t maxThreads = 15;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '/dev/binder' failed: %s\n", strerror(errno));
    }
    return fd;
}



sp<IServiceManager> defaultServiceManager()
{
    if (gDefaultServiceManager != NULL) return gDefaultServiceManager;
    
    {
        AutoMutex _l(gDefaultServiceManagerLock);
        while (gDefaultServiceManager == NULL) {
            gDefaultServiceManager = interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));
            if (gDefaultServiceManager == NULL)
                sleep(1);
        }
    }
    
    return gDefaultServiceManager;
}

//	frameworks/base/libs/binder/Static.cpp
Mutex gDefaultServiceManagerLock;
sp<IServiceManager> gDefaultServiceManager;



///////////////////////////////////////////////////////////////////////////////////////////

gDefaultServiceManager = interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));

//	ProcessState::self()->getContextObject(NULL)
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& caller)
{
    return getStrongProxyForHandle(0);
}



//最终返回BpBinder(0)
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        // We need to create a new BpBinder if there isn't currently one, OR we
        // are unable to acquire a weak reference on this current one.  See comment
        // in getWeakProxyForHandle() for more info about this.
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {
                // Special case for context manager...
                // The context manager is the only object for which we create
                // a BpBinder proxy without already holding a reference.
                // Perform a dummy transaction to ensure the context manager
                // is registered before we create the first local reference
                // to it (which will occur when creating the BpBinder).
                // If a local reference is created for the BpBinder when the
                // context manager is not present, the driver will fail to
                // provide a reference to the context manager, but the
                // driver API does not return status.
                //
                // Note that this is not race-free if the context manager
                // dies while this code runs.
                //
                // TODO: add a driver API to wait for context manager, or
                // stop special casing handle 0 for context manager and add
                // a driver API to get a handle to the context manager with
                // proper reference counting.

                Parcel data;
                status_t status = IPCThreadState::self()->transact(0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT)
                   return NULL;
            }

            b = new BpBinder(handle); 
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            // This little bit of nastyness is to allow us to add a primary
            // reference to the remote proxy when this team doesn't have one
            // but another team is sending the handle to us.
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}



gDefaultServiceManager = interface_cast<IServiceManager>(BpBinder(0));


//framework/base/include/binder/IInterface.h
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
{
    return INTERFACE::asInterface(obj);
}
//转化为
inline sp<IServiceManager> interface_cast(const sp<IBinder>& obj)
{
    return IServiceManager::asInterface(obj);
}


//又可以得到：
gDefaultServiceManager = IServiceManager::asInterface(BpBinder(0));


//	//framework/base/include/binder/IInterface.h
DECLARE_META_INTERFACE(ServiceManager);

#define DECLARE_META_INTERFACE(INTERFACE)                               \
    static const android::String16 descriptor;                          \
    static android::sp<I##INTERFACE> asInterface(                       \
            const android::sp<android::IBinder>& obj);                  \
    virtual const android::String16& getInterfaceDescriptor() const;    \
    I##INTERFACE();                                                     \
    virtual ~I##INTERFACE();                                            \



IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");

#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \
    const android::String16 I##INTERFACE::descriptor(NAME);             \
    const android::String16&                                            \
            I##INTERFACE::getInterfaceDescriptor() const {              \
        return I##INTERFACE::descriptor;                                \
    }                                                                   \
    android::sp<I##INTERFACE> I##INTERFACE::asInterface(                \
            const android::sp<android::IBinder>& obj)                   \
    {                                                                   \
        android::sp<I##INTERFACE> intr;                                 \
        if (obj != NULL) {                                              \
            intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new Bp##INTERFACE(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }                                                                   \
    I##INTERFACE::I##INTERFACE() { }                                    \
    I##INTERFACE::~I##INTERFACE() { }                                   \


// 转换为
//其中传入参数obj = BpBinder(0)
android::sp<IServiceManager> IServiceManager::asInterface(const android::sp<android::IBinder>& obj)                   
{                                                                  
	android::sp<IServiceManager> intr;                                
	if (obj != NULL) {              
	    //IServiceManager::descriptor = "android.os.IServiceManager"                         
	    intr = static_cast<IServiceManager*>(obj->queryLocalInterface(IServiceManager::descriptor).get());        
	    if (intr == NULL) {                                         
		intr = new BpServiceManager(obj);                          
	    }                                                           
	}                                                               
	return intr;                                                    
}     


//	framework/base/libs/binder/Binder.cpp                                                              
sp<IInterface>  IBinder::queryLocalInterface(const String16& descriptor)
{
    return NULL;
}


//	intr = new BpServiceManager(obj); => intr = new BpServiceManager(BpBinder(0));


//最终得到
gDefaultServiceManager = new BpServiceManager(BpBinder(0));




