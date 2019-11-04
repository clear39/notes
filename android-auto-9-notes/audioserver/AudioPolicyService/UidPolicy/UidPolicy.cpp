class UidPolicy : public BnUidObserver, public virtual IBinder::DeathRecipient
{

}

//  @ frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp

explicit UidPolicy::UidPolicy(wp<AudioPolicyService> service)
    : mService(service), mObserverRegistered(false)
{
}

void AudioPolicyService::UidPolicy::registerSelf()
{
    //  @   frameworks/native/libs/binder/ActivityManager.cpp
    ActivityManager am;
    am.registerUidObserver(this, ActivityManager::UID_OBSERVER_GONE 
                                | ActivityManager::UID_OBSERVER_IDLE 
                                | ActivityManager::UID_OBSERVER_ACTIVE,
                           ActivityManager::PROCESS_STATE_UNKNOWN,
                           String16("audioserver"));

    status_t res = am.linkToDeath(this);
    if (!res)
    {
        Mutex::Autolock _l(mLock);
        mObserverRegistered = true;
    }
    else
    {
        ALOGE("UidPolicy::registerSelf linkToDeath failed: %d", res);
        am.unregisterUidObserver(this);
    }
}





/**
 * 在c层代码中只实现一下三个方法
*/
void AudioPolicyService::UidPolicy::onUidActive(uid_t uid) {
    updateUidCache(uid, true, true);
}

void AudioPolicyService::UidPolicy::onUidGone(uid_t uid, __unused bool disabled) {
    updateUidCache(uid, false, false);
}

void AudioPolicyService::UidPolicy::onUidIdle(uid_t uid, __unused bool disabled) {
    updateUidCache(uid, false, true);
}

void AudioPolicyService::UidPolicy::updateUidCache(uid_t uid, bool active, bool insert) {
    if (isServiceUid(uid)) return;
    bool wasActive = false;
    {
        Mutex::Autolock _l(mLock);
        updateUidLocked(&mCachedUids, uid, active, insert, nullptr, &wasActive);
        // Do not notify service if currently overridden.
        if (mOverrideUids.find(uid) != mOverrideUids.end()) return;
    }
    bool nowActive = active && insert;
    if (wasActive != nowActive) notifyService(uid, nowActive);
}

bool AudioPolicyService::UidPolicy::isServiceUid(uid_t uid) const {
    /**
     * system/core/libcutils/include/private/android_filesystem_config.h:165:#define AID_APP_START 10000 // first app user 
     * 
    */  
    return multiuser_get_app_id(uid) < AID_APP_START;
}


void AudioPolicyService::UidPolicy::updateUidLocked(std::unordered_map<uid_t, bool> *uids,uid_t uid, bool active, bool insert, bool *wasThere, bool *wasActive) {
    auto it = uids->find(uid);
    if (it != uids->end()) {
        if (wasThere != nullptr) *wasThere = true;
        if (wasActive != nullptr) *wasActive = it->second;
        if (insert) {
            it->second = active;
        } else {
            uids->erase(it);
        }
    } else if (insert) {
        uids->insert(std::pair<uid_t, bool>(uid, active));
    }
}


void AudioPolicyService::UidPolicy::notifyService(uid_t uid, bool active) {
    sp<AudioPolicyService> service = mService.promote();
    if (service != nullptr) {
        service->setRecordSilenced(uid, !active);
    }
}
