



//@system/libhidl/transport/allocator/1.0/default/	service.cpp
int main() {
    configureRpcThreadpool(1, true /* callerWillJoin */);

    sp<IAllocator> allocator = new AshmemAllocator();

    status_t status = allocator->registerAsService("ashmem");

    if (android::OK != status) {
        LOG(FATAL) << "Unable to register allocator service: " << status;
    }

    joinRpcThreadpool();

    return -1;
}