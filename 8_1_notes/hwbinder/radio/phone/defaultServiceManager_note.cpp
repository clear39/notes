//	@system/libhidl/transport/ServiceManagement.cpp
/*
using IServiceManager1_0 = android::hidl::manager::V1_0::IServiceManager;
using IServiceManager1_1 = android::hidl::manager::V1_1::IServiceManager;
*/
sp<IServiceManager1_0> defaultServiceManager() {
    return defaultServiceManager1_1();
}

sp<IServiceManager1_1> defaultServiceManager1_1() {
    {
        AutoMutex _l(details::gDefaultServiceManagerLock);
        if (details::gDefaultServiceManager != NULL) {
            return details::gDefaultServiceManager;
        }

        if (access("/dev/hwbinder", F_OK|R_OK|W_OK) != 0) {
            // HwBinder not available on this device or not accessible to
            // this process.
            return nullptr;
        }

        waitForHwServiceManager();

        while (details::gDefaultServiceManager == NULL) {

        	//	fromBinder	@system/libhidl/transport/include/hidl/HidlBinderSupport.h
        	//	ProcessState	@system/libhwbinder/ProcessState.cpp

        	//	details::gDefaultServiceManager  = new BpHwServiceManager(BpHwBinder(0)));
            details::gDefaultServiceManager = fromBinder<IServiceManager1_1, BpHwServiceManager, BnHwServiceManager>(ProcessState::self()->getContextObject(NULL));
            if (details::gDefaultServiceManager == NULL) {
                LOG(ERROR) << "Waited for hwservicemanager, but got nullptr.";
                sleep(1);
            }
        }
    }

    return details::gDefaultServiceManager;
}


static const char* kHwServicemanagerReadyProperty = "hwservicemanager.ready";
void waitForHwServiceManager() {
    using std::literals::chrono_literals::operator""s;

    while (!WaitForProperty(kHwServicemanagerReadyProperty, "true", 1s)) {
        LOG(WARNING) << "Waited for hwservicemanager.ready for a second, waiting another...";
    }
}



//	fromBinder<IServiceManager1_1, BpHwServiceManager, BnHwServiceManager>(BpHwBinder(0));
template <typename IType, typename ProxyType, typename StubType>
sp<IType> fromBinder(const sp<IBinder>& binderIface) {
    using ::android::hidl::base::V1_0::IBase;
    using ::android::hidl::base::V1_0::BnHwBase;

    if (binderIface.get() == nullptr) {
        return nullptr;
    }
    if (binderIface->localBinder() == nullptr) {	//localBinder()直接返回NULL;		@system/libhwbinder/Binder.cpp
        return new ProxyType(binderIface);    // new BpHwServiceManager(BpHwBinder(0)));
    }
    sp<IBase> base = static_cast<BnHwBase*>(binderIface.get())->getImpl();
    if (details::canCastInterface(base.get(), IType::descriptor)) {
        StubType* stub = static_cast<StubType*>(binderIface.get());
        return stub->getImpl();
    } else {
        return nullptr;
    }
}




sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);
}

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

	//到本地临时缓冲区中查找,是否由已经获取到对应的binder
	//没有创建一个新的并且压入集合
    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        // We need to create a new BpHwBinder if there isn't currently one, OR we
        // are unable to acquire a weak reference on this current one.  See comment
        // in getWeakProxyForHandle() for more info about this.
        IBinder* b = e->binder;//判断binder是否为null
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            b = new BpHwBinder(handle);	//@system/libhwbinder/BpHwBinder.cpp
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();//这里是将binder赋值给refs
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

struct handle_entry {
    IBinder* binder;
    RefBase::weakref_type* refs;
};

ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
	//	 Vector<handle_entry>mHandleToObject;
    const size_t N=mHandleToObject.size();
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = NULL;
        e.refs = NULL;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}