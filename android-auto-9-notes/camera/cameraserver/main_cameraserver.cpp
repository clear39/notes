
//  @   frameworks/av/camera/cameraserver/main_cameraserver.cpp

#include "CameraService.h"
#include <hidl/HidlTransportSupport.h>

using namespace android;

int main(int argc __unused, char** argv __unused)
{
    signal(SIGPIPE, SIG_IGN);

    // Set 3 threads for HIDL calls
    hardware::configureRpcThreadpool(3, /*willjoin*/ false);

    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    ALOGI("ServiceManager: %p", sm.get());
    CameraService::instantiate();       //由于 CameraService 继承 BinderService
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
}


//  @   frameworks/av/camera/cameraserver/cameraserver.rc

service cameraserver /system/bin/cameraserver
    class main
    user cameraserver
    group audio camera input drmrpc
    ioprio rt 4
    writepid /dev/cpuset/camera-daemon/tasks /dev/stune/top-app/tasks
    rlimit rtprio 10 10