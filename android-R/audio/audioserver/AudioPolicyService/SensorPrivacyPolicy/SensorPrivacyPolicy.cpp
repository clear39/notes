

// -----------  AudioPolicyService::SensorPrivacyService implementation ----------
void AudioPolicyService::SensorPrivacyPolicy::registerSelf() {
    SensorPrivacyManager spm;
    mSensorPrivacyEnabled = spm.isSensorPrivacyEnabled();
    spm.addSensorPrivacyListener(this);
}

void AudioPolicyService::SensorPrivacyPolicy::unregisterSelf() {
    SensorPrivacyManager spm;
    spm.removeSensorPrivacyListener(this);
}

bool AudioPolicyService::SensorPrivacyPolicy::isSensorPrivacyEnabled() {
    return mSensorPrivacyEnabled;
}

binder::Status AudioPolicyService::SensorPrivacyPolicy::onSensorPrivacyChanged(bool enabled) {
    mSensorPrivacyEnabled = enabled;
    sp<AudioPolicyService> service = mService.promote();
    if (service != nullptr) {
        service->updateUidStates();
    }
    return binder::Status::ok();
}