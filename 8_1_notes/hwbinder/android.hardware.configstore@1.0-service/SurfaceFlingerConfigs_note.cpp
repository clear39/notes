//			@hardware/interfaces/configstore/1.0/default/SurfaceFlingerConfigs.cpp
Return<void> SurfaceFlingerConfigs::startGraphicsAllocatorService(startGraphicsAllocatorService_cb _hidl_cb) {
    bool value = false;
#ifdef START_GRAPHICS_ALLOCATOR_SERVICE //没有定义
    value = true;
#endif
    _hidl_cb({true, value});
    return Void();
}