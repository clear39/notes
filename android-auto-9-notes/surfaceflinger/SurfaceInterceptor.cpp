//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/services/surfaceflinger/SurfaceInterceptor.cpp
SurfaceInterceptor::SurfaceInterceptor(SurfaceFlinger* flinger)
    :   mFlinger(flinger)
{
}


void SurfaceInterceptor::saveDisplayCreation(const DisplayDeviceState& info) {
    if (!mEnabled) {
        return;
    }
    ATRACE_CALL();
    std::lock_guard<std::mutex> protoGuard(mTraceMutex);
    addDisplayCreationLocked(createTraceIncrementLocked(), info);
}


Increment* SurfaceInterceptor::createTraceIncrementLocked() {
    Increment* increment(mTrace.add_increment());
    increment->set_time_stamp(systemTime());
    return increment;
}

void SurfaceInterceptor::addDisplayCreationLocked(Increment* increment,const DisplayDeviceState& info)
{
    DisplayCreation* creation(increment->mutable_display_creation());
    creation->set_id(info.displayId);
    creation->set_name(info.displayName);
    creation->set_type(info.type);
    creation->set_is_secure(info.isSecure);
}