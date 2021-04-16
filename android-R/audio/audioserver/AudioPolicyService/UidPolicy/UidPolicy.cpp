

void AudioPolicyService::UidPolicy::registerSelf() {
    ActivityManager am;
    am.registerUidObserver(this, ActivityManager::UID_OBSERVER_GONE
            | ActivityManager::UID_OBSERVER_IDLE
            | ActivityManager::UID_OBSERVER_ACTIVE
            | ActivityManager::UID_OBSERVER_PROCSTATE,
            ActivityManager::PROCESS_STATE_UNKNOWN,
            String16("audioserver"));
    status_t res = am.linkToDeath(this);
    if (!res) {
        Mutex::Autolock _l(mLock);
        mObserverRegistered = true;
    } else {
        ALOGE("UidPolicy::registerSelf linkToDeath failed: %d", res);

        am.unregisterUidObserver(this);
    }
}