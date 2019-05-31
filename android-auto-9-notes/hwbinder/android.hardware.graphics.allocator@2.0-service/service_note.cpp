service gralloc-2-0 /vendor/bin/hw/android.hardware.graphics.allocator@2.0-service
    class hal animation
    user system
    group graphics drmrpc
    capabilities SYS_NICE
    onrestart restart surfaceflinger





//@hardware/interfaces/graphics/allocator/2.0/default/service.cpp
int main() {
    return defaultPassthroughServiceImplementation<IAllocator>(4);
}


//	system/libhidl/transport/include/hidl/LegacySupport.h:81
template<class Interface>
__attribute__((warn_unused_result))
status_t defaultPassthroughServiceImplementation(size_t maxThreads = 1) {
    return defaultPassthroughServiceImplementation<Interface>("default", maxThreads);
}


template<class Interface>
__attribute__((warn_unused_result))
status_t defaultPassthroughServiceImplementation(std::string name,size_t maxThreads = 1) {
    configureRpcThreadpool(maxThreads, true);
    status_t result = registerPassthroughServiceImplementation<Interface>(name);

    if (result != OK) {
        return result;
    }

    joinRpcThreadpool();
    return 0;
}



template<class Interface>
__attribute__((warn_unused_result))
status_t registerPassthroughServiceImplementation(std::string name = "default") {
    sp<Interface> service = Interface::getService(name, true /* getStub */);

    if (service == nullptr) {
        ALOGE("Could not get passthrough implementation for %s/%s.", Interface::descriptor, name.c_str());
        return EXIT_FAILURE;
    }

    LOG_FATAL_IF(service->isRemote(), "Implementation of %s/%s is remote!", Interface::descriptor, name.c_str());

    status_t status = service->registerAsService(name);

    if (status == OK) {
        ALOGI("Registration complete for %s/%s.", Interface::descriptor, name.c_str());
    } else {
        ALOGE("Could not register service %s/%s (%d).", Interface::descriptor, name.c_str(), status);
    }

    return status;
}
