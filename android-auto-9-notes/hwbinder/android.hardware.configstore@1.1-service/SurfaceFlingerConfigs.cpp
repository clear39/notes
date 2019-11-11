//			@hardware/interfaces/configstore/1.0/default/SurfaceFlingerConfigs.cpp
Return<void> SurfaceFlingerConfigs::startGraphicsAllocatorService(startGraphicsAllocatorService_cb _hidl_cb) {
    bool value = false;
#ifdef START_GRAPHICS_ALLOCATOR_SERVICE //没有定义
    value = true;
#endif
    _hidl_cb({true, value});
    return Void();
}




// Methods from ::android::hardware::configstore::V1_0::ISurfaceFlingerConfigs
// follow.
Return<void> SurfaceFlingerConfigs::vsyncEventPhaseOffsetNs(vsyncEventPhaseOffsetNs_cb _hidl_cb) {
#ifdef VSYNC_EVENT_PHASE_OFFSET_NS  //没有定义
    _hidl_cb({true, VSYNC_EVENT_PHASE_OFFSET_NS});
#else
    _hidl_cb({false, 0});
#endif
    return Void();
}



Return<void> SurfaceFlingerConfigs::primaryDisplayOrientation(
    primaryDisplayOrientation_cb _hidl_cb) {
    using ::android::hardware::configstore::V1_1::DisplayOrientation;

    bool specified = false;
    DisplayOrientation value = DisplayOrientation::ORIENTATION_0;

    int orientation = 0;
#ifdef PRIMARY_DISPLAY_ORIENTATION  // 目前没有定义
    specified = true;
    orientation = PRIMARY_DISPLAY_ORIENTATION;
#endif

    switch (orientation) {
        case 0: {
            value = DisplayOrientation::ORIENTATION_0;
            break;
        }
        case 90: {
            value = DisplayOrientation::ORIENTATION_90;
            break;
        }
        case 180: {
            value = DisplayOrientation::ORIENTATION_180;
            break;
        }
        case 270: {
            value = DisplayOrientation::ORIENTATION_270;
            break;
        }
        default: {
            // statically checked above -> memory corruption
            LOG_ALWAYS_FATAL("Invalid orientation %d", orientation);
        }
    }

    _hidl_cb({specified, value});
    return Void();
}