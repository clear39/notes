
//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/utils/include/mediautils/BatteryNotifier.h
class BatteryNotifier : public Singleton<BatteryNotifier> {

}

//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/utils/BatteryNotifier.cpp
BatteryNotifier::BatteryNotifier() {

}


void BatteryNotifier::noteResetCamera() {
    Mutex::Autolock _l(mLock);
    sp<IBatteryStats> batteryService = getBatteryService_l();
    mCameraState.clear();
    if (batteryService != nullptr) {
        batteryService->noteResetCamera();
    }
}


void BatteryNotifier::noteResetFlashlight() {
    Mutex::Autolock _l(mLock);
    sp<IBatteryStats> batteryService = getBatteryService_l();
    mFlashlightState.clear();
    if (batteryService != nullptr) {
        batteryService->noteResetFlashlight();
    }
}

sp<IBatteryStats> BatteryNotifier::getBatteryService_l() {
    if (mBatteryStatService != nullptr) {
        return mBatteryStatService;
    }
    // Get battery service from service manager
    const sp<IServiceManager> sm(defaultServiceManager());
    if (sm != nullptr) {
        const String16 name("batterystats");
        mBatteryStatService = interface_cast<IBatteryStats>(sm->checkService(name));
        if (mBatteryStatService == nullptr) {
            // this may occur normally during the init sequence as mediaserver
            // and audioserver start before the batterystats service is available.
            ALOGW("batterystats service unavailable!");
            return nullptr;
        }

        mDeathNotifier = new DeathNotifier();
        IInterface::asBinder(mBatteryStatService)->linkToDeath(mDeathNotifier);

        // Notify start now if mediaserver or audioserver is already started.
        // 1) mediaserver and audioserver is started before batterystats service
        // 2) batterystats server may have crashed.
        std::map<uid_t, int>::iterator it = mVideoRefCounts.begin();
        for (; it != mVideoRefCounts.end(); ++it) {
            mBatteryStatService->noteStartVideo(it->first);
        }
        it = mAudioRefCounts.begin();
        for (; it != mAudioRefCounts.end(); ++it) {
            mBatteryStatService->noteStartAudio(it->first);
        }
        // TODO: Notify for camera and flashlight state as well?
    }
    return mBatteryStatService;
}