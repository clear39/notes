//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/common/all-versions/default/service/android.hardware.audio@2.0-service.rc
service vendor.audio-hal-2-0 /vendor/bin/hw/android.hardware.audio@2.0-service
    class hal
    user audioserver
    # media gid needed for /dev/fm (radio) and for /data/misc/media (tee)
    group audio camera drmrpc inet media mediadrm net_bt net_bt_admin net_bw_acct
    ioprio rt 4
    writepid /dev/cpuset/foreground/tasks /dev/stune/foreground/tasks
    # audioflinger restarts itself when it loses connection with the hal
    # and its .rc file has an "onrestart restart audio-hal" rule, thus
    # an additional auto-restart from the init process isn't needed.   '
    oneshot
    interface android.hardware.audio@4.0::IDevicesFactory default
    interface android.hardware.audio@2.0::IDevicesFactory default





//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/common/all-versions/default/service/service.cpp

int main(int /* argc */, char* /* argv */ []) {
    android::ProcessState::initWithDriver("/dev/vndbinder");
    // start a threadpool for vndbinder interactions(交互作用)
    android::ProcessState::self()->startThreadPool();
    configureRpcThreadpool(16, true /*callerWillJoin*/);

    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/4.0/IDevicesFactory.hal
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/core/4.0/default/DevicesFactory.cpp
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/2.0/IDevicesFactory.hal
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/core/2.0/default/DevicesFactory.cpp

    //registerPassthroughServiceImplementation  @/work/workcodes/aosp-p9.x-auto-alpha/system/libhidl/transport/include/hidl/LegacySupport.h
    bool fail = registerPassthroughServiceImplementation<audio::V4_0::IDevicesFactory>() != OK &&
                registerPassthroughServiceImplementation<audio::V2_0::IDevicesFactory>() != OK;
    LOG_ALWAYS_FATAL_IF(fail, "Could not register audio core API 2.0 nor 4.0");


    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/effect/4.0/IEffectsFactory.hal
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/effect/4.0/default/EffectsFactory.cpp
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/effect/2.0/IEffectsFactory.hal
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/audio/effect/2.0/default/EffectsFactory.cpp
    fail = registerPassthroughServiceImplementation<audio::effect::V4_0::IEffectsFactory>() != OK &&
           registerPassthroughServiceImplementation<audio::effect::V2_0::IEffectsFactory>() != OK,
    LOG_ALWAYS_FATAL_IF(fail, "Could not register audio effect API 2.0 nor 4.0");

    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/soundtrigger/2.1/ISoundTriggerHw.hal
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/soundtrigger/2.0/ISoundTriggerHw.hal
    fail = registerPassthroughServiceImplementation<soundtrigger::V2_1::ISoundTriggerHw>() != OK &&
           registerPassthroughServiceImplementation<soundtrigger::V2_0::ISoundTriggerHw>() != OK,
    ALOGW_IF(fail, "Could not register soundtrigger API 2.0 nor 2.1");

    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/bluetooth/a2dp/1.0/IBluetoothAudioOffload.hal
    fail = registerPassthroughServiceImplementation<bluetooth::a2dp::V1_0::IBluetoothAudioOffload>() != OK;
    ALOGW_IF(fail, "Could not register Bluetooth audio offload 1.0");

    joinRpcThreadpool();
}


/**
 * 
 * /vendor/lib/hw/audio.r_submix.default.so
 * /vendor/lib/hw/audio.usb.default.so
 * /vendor/lib/hw/audio.primary.imx8.so
 * 
*/