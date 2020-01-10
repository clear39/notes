//  @   hardware/interfaces/graphics/allocator/2.0/utils/passthrough/include/allocator-passthrough/2.0/Gralloc1Hal.h

/**
 * AllocatorHal @   hardware/interfaces/graphics/allocator/2.0/utils/hal/include/allocator-hal/2.0/AllocatorHal.h
 * 
 * AllocatorHal 为抽象类，全部为虚函数
*/
using Gralloc1Hal = detail::Gralloc1HalImpl<hal::AllocatorHal>;

//  Gralloc1HalImpl 为一个模板实现
template <typename Hal>
class Gralloc1HalImpl : public Hal {

}


bool Gralloc1HalImpl::initWithModule(const hw_module_t* module) {

    int result = gralloc1_open(module, &mDevice);
    if (result) {
        ALOGE("failed to open gralloc1 device: %s", strerror(-result));
        mDevice = nullptr;
        return false;
    }

    initCapabilities();

    if (!initDispatch()) {
        gralloc1_close(mDevice);
        mDevice = nullptr;
        return false;
    }

    return true;
}

virtual void Gralloc1HalImpl::initCapabilities() {
    uint32_t count = 0;
    mDevice->getCapabilities(mDevice, &count, nullptr);

    std::vector<int32_t> capabilities(count);
    mDevice->getCapabilities(mDevice, &count, capabilities.data());
    capabilities.resize(count);

    for (auto capability : capabilities) {
        if (capability == GRALLOC1_CAPABILITY_LAYERED_BUFFERS) {
            mCapabilities.layeredBuffers = true;
            break;
        }
    }
}


virtual bool Gralloc1HalImpl::initDispatch() {

    if (!initDispatch(GRALLOC1_FUNCTION_DUMP, &mDispatch.dump) ||
        !initDispatch(GRALLOC1_FUNCTION_CREATE_DESCRIPTOR, &mDispatch.createDescriptor) ||
        !initDispatch(GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR, &mDispatch.destroyDescriptor) ||
        !initDispatch(GRALLOC1_FUNCTION_SET_DIMENSIONS, &mDispatch.setDimensions) ||
        !initDispatch(GRALLOC1_FUNCTION_SET_FORMAT, &mDispatch.setFormat) ||
        !initDispatch(GRALLOC1_FUNCTION_SET_CONSUMER_USAGE, &mDispatch.setConsumerUsage) ||
        !initDispatch(GRALLOC1_FUNCTION_SET_PRODUCER_USAGE, &mDispatch.setProducerUsage) ||
        !initDispatch(GRALLOC1_FUNCTION_GET_STRIDE, &mDispatch.getStride) ||
        !initDispatch(GRALLOC1_FUNCTION_ALLOCATE, &mDispatch.allocate) ||
        !initDispatch(GRALLOC1_FUNCTION_RELEASE, &mDispatch.release)) {
        return false;
    }

    if (mCapabilities.layeredBuffers) {
        if (!initDispatch(GRALLOC1_FUNCTION_SET_LAYER_COUNT, &mDispatch.setLayerCount)) {
            return false;
        }
    }

    return true;
    
}


template <typename T>
bool Gralloc1HalImpl::initDispatch(gralloc1_function_descriptor_t desc, T* outPfn) {
    auto pfn = mDevice->getFunction(mDevice, desc);
    if (pfn) {
        *outPfn = reinterpret_cast<T>(pfn);
        return true;
    } else {
        ALOGE("failed to get gralloc1 function %d", desc);
        return false;
    }
}


struct {
    GRALLOC1_PFN_DUMP dump;
    GRALLOC1_PFN_CREATE_DESCRIPTOR createDescriptor;
    GRALLOC1_PFN_DESTROY_DESCRIPTOR destroyDescriptor;
    GRALLOC1_PFN_SET_DIMENSIONS setDimensions;
    GRALLOC1_PFN_SET_FORMAT setFormat;
    GRALLOC1_PFN_SET_LAYER_COUNT setLayerCount;
    GRALLOC1_PFN_SET_CONSUMER_USAGE setConsumerUsage;
    GRALLOC1_PFN_SET_PRODUCER_USAGE setProducerUsage;
    GRALLOC1_PFN_GET_STRIDE getStride;
    GRALLOC1_PFN_ALLOCATE allocate;
    GRALLOC1_PFN_RELEASE release;
} Gralloc1HalImpl::mDispatch = {};