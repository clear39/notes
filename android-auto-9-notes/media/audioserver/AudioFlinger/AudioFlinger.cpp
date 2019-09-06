//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audioflinger/AudioFlinger.cpp
AudioFlinger::AudioFlinger()
    : BnAudioFlinger(),
      mMediaLogNotifier(new AudioFlinger::MediaLogNotifier()),
      mPrimaryHardwareDev(NULL),
      mAudioHwDevs(NULL),
      mHardwareStatus(AUDIO_HW_IDLE),
      mMasterVolume(1.0f),
      mMasterMute(false),
      // mNextUniqueId(AUDIO_UNIQUE_ID_USE_MAX),
      mMode(AUDIO_MODE_INVALID),
      mBtNrecIsOff(false),
      mIsLowRamDevice(true),
      mIsDeviceTypeKnown(false),
      mTotalMemory(0),
      mClientSharedHeapSize(kMinimumClientSharedHeapSizeBytes),
      mGlobalEffectEnableTime(0),
      mSystemReady(false)
{
    //  定义在 system/media/audio/include/system/audio.h
    // unsigned instead of audio_unique_id_use_t, because ++ operator is unavailable for enum
    for (unsigned use = AUDIO_UNIQUE_ID_USE_UNSPECIFIED; use < AUDIO_UNIQUE_ID_USE_MAX; use++) {
        // zero ID has a special meaning, so unavailable
        mNextUniqueIds[use] = AUDIO_UNIQUE_ID_USE_MAX; //初始化mNextUniqueIds 数组
    }

    getpid_cached = getpid();
    // 设置ro.test_harness属性，开启media.log服务
    const bool doLog = property_get_bool("ro.test_harness", false);
    if (doLog) {
        mLogMemoryDealer = new MemoryDealer(kLogMemorySize, "LogWriters",MemoryHeapBase::READ_ONLY);
        (void) pthread_once(&sMediaLogOnce, sMediaLogInit);
    }

    // reset battery stats.
    // if the audio service has crashed, battery stats could be left
    // in bad state, reset the state upon service start.
    BatteryNotifier::getInstance().noteResetAudio();
    /***
     * 返回一个DevicesFactoryHalHybrid @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/media/libaudiohal/4.0/DevicesFactoryHalHybrid.cpp
     */ 
    mDevicesFactoryHal = DevicesFactoryHalInterface::create();

    /***
     * 返回一个EffectsFactoryHalHidl    @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/media/libaudiohal/4.0/EffectsFactoryHalHidl.cpp
     */ 
    mEffectsFactoryHal = EffectsFactoryHalInterface::create();

    mMediaLogNotifier->run("MediaLogNotifier");

    //  @   frameworks/av/services/audioflinger/Configuration.h
#ifdef TEE_SINK     //可以开启调试方式
    char value[PROPERTY_VALUE_MAX];
    (void) property_get("ro.debuggable", value, "0");
    int debuggable = atoi(value);
    int teeEnabled = 0;
    if (debuggable) {
        (void) property_get("af.tee", value, "0");
        teeEnabled = atoi(value);
    }
    // FIXME symbolic constants here
    if (teeEnabled & 1) {
        mTeeSinkInputEnabled = true;
    }
    if (teeEnabled & 2) {
        mTeeSinkOutputEnabled = true;
    }
    if (teeEnabled & 4) {
        mTeeSinkTrackEnabled = true;
    }
#endif
}

void AudioFlinger::onFirstRef()
{
    Mutex::Autolock _l(mLock);

    /* TODO: move all this work into an Init() function */
    /***
     * 这个是获取flinger的standby时间间隔
     */ 
    char val_str[PROPERTY_VALUE_MAX] = { 0 };
    if (property_get("ro.audio.flinger_standbytime_ms", val_str, NULL) >= 0) {
        uint32_t int_val;
        if (1 == sscanf(val_str, "%u", &int_val)) {
            mStandbyTimeInNsecs = milliseconds(int_val);
            ALOGI("Using %u mSec as standby time.", int_val);
        } else {
            //  09-06 10:47:46.344  3229  3229 I AudioFlinger: Using default 3000 mSec as standby time.
            mStandbyTimeInNsecs = kDefaultStandbyTimeInNsecs;
            ALOGI("Using default %u mSec as standby time.",(uint32_t)(mStandbyTimeInNsecs / 1000000));
        }
    }

    /**
     * 
    */
    mPatchPanel = new PatchPanel(this);

    mMode = AUDIO_MODE_NORMAL;

    gAudioFlinger = this;
}

/** 这里由 AudioPolicyService 中构造 AudioPolicyManager 触发调用，根据模块调用创建
 * 
*/
audio_module_handle_t AudioFlinger::loadHwModule(const char *name)
{
    if (name == NULL) {
        return AUDIO_MODULE_HANDLE_NONE;
    }
    if (!settingsAllowed()) {
        return AUDIO_MODULE_HANDLE_NONE;
    }
    Mutex::Autolock _l(mLock);
    return loadHwModule_l(name);
}
/***
 *
 * 09-06 10:47:46.404  3229  3229 I AudioFlinger: loadHwModule() Loaded primary audio interface, handle 10
 * 09-06 10:47:46.436  3229  3229 I AudioFlinger: loadHwModule() Loaded a2dp audio interface, handle 18
 * 09-06 10:47:46.438  3229  3229 I AudioFlinger: loadHwModule() Loaded usb audio interface, handle 26
 * 09-06 10:47:46.441  3229  3229 I AudioFlinger: loadHwModule() Loaded r_submix audio interface, handle 34
 * */ 
// loadHwModule_l() must be called with AudioFlinger::mLock held
audio_module_handle_t AudioFlinger::loadHwModule_l(const char *name)
{
    for (size_t i = 0; i < mAudioHwDevs.size(); i++) {
        if (strncmp(mAudioHwDevs.valueAt(i)->moduleName(), name, strlen(name)) == 0) {
            ALOGW("loadHwModule() module %s already loaded", name);
            return mAudioHwDevs.keyAt(i);
        }
    }

    sp<DeviceHalInterface> dev;

    int rc = mDevicesFactoryHal->openDevice(name, &dev);
    if (rc) {
        ALOGE("loadHwModule() error %d loading module %s", rc, name);
        return AUDIO_MODULE_HANDLE_NONE;
    }

    mHardwareStatus = AUDIO_HW_INIT;
    rc = dev->initCheck();
    mHardwareStatus = AUDIO_HW_IDLE;
    if (rc) {
        ALOGE("loadHwModule() init check error %d for module %s", rc, name);
        return AUDIO_MODULE_HANDLE_NONE;
    }

    // Check and cache this HAL's level of support for master mute and master
    // volume.  If this is the first HAL opened, and it supports the get
    // methods, use the initial values provided by the HAL as the current
    // master mute and volume settings.

    AudioHwDevice::Flags flags = static_cast<AudioHwDevice::Flags>(0);
    {  // scope for auto-lock pattern
        AutoMutex lock(mHardwareLock);

        if (0 == mAudioHwDevs.size()) {
            mHardwareStatus = AUDIO_HW_GET_MASTER_VOLUME;
            float mv;
            if (OK == dev->getMasterVolume(&mv)) {
                mMasterVolume = mv;
            }

            mHardwareStatus = AUDIO_HW_GET_MASTER_MUTE;
            bool mm;
            if (OK == dev->getMasterMute(&mm)) {
                mMasterMute = mm;
            }
        }

        mHardwareStatus = AUDIO_HW_SET_MASTER_VOLUME;
        if (OK == dev->setMasterVolume(mMasterVolume)) {
            flags = static_cast<AudioHwDevice::Flags>(flags | AudioHwDevice::AHWD_CAN_SET_MASTER_VOLUME);
        }

        mHardwareStatus = AUDIO_HW_SET_MASTER_MUTE;
        if (OK == dev->setMasterMute(mMasterMute)) {
            flags = static_cast<AudioHwDevice::Flags>(flags | AudioHwDevice::AHWD_CAN_SET_MASTER_MUTE);
        }

        mHardwareStatus = AUDIO_HW_IDLE;
    }

    /**
     * 创建一个句柄id 并赋值给 handle
     * */
    audio_module_handle_t handle = (audio_module_handle_t) nextUniqueId(AUDIO_UNIQUE_ID_USE_MODULE);
    mAudioHwDevs.add(handle, new AudioHwDevice(handle, name, dev, flags));

    ALOGI("loadHwModule() Loaded %s audio interface, handle %d", name, handle);

    return handle;

}


status_t AudioFlinger::openOutput(audio_module_handle_t module,
                                  audio_io_handle_t *output,
                                  audio_config_t *config,
                                  audio_devices_t *devices,
                                  const String8& address,
                                  uint32_t *latencyMs,
                                  audio_output_flags_t flags)
{
    ALOGI("openOutput() this %p, module %d Device %#x, SamplingRate %d, Format %#08x, "
              "Channels %#x, flags %#x",
              this, module,
              (devices != NULL) ? *devices : 0,
              config->sample_rate,
              config->format,
              config->channel_mask,
              flags);

    if (devices == NULL || *devices == AUDIO_DEVICE_NONE) {
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mLock);

    sp<ThreadBase> thread = openOutput_l(module, output, config, *devices, address, flags);
    if (thread != 0) {
        if ((flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) == 0) {
            PlaybackThread *playbackThread = (PlaybackThread *)thread.get();
            *latencyMs = playbackThread->latency();

            // notify client processes of the new output creation
            playbackThread->ioConfigChanged(AUDIO_OUTPUT_OPENED);

            // the first primary output opened designates the primary hw device
            if ((mPrimaryHardwareDev == NULL) && (flags & AUDIO_OUTPUT_FLAG_PRIMARY)) {
                ALOGI("Using module %d as the primary audio interface", module);
                mPrimaryHardwareDev = playbackThread->getOutput()->audioHwDev;

                AutoMutex lock(mHardwareLock);
                mHardwareStatus = AUDIO_HW_SET_MODE;
                mPrimaryHardwareDev->hwDevice()->setMode(mMode);
                mHardwareStatus = AUDIO_HW_IDLE;
            }
        } else {
            MmapThread *mmapThread = (MmapThread *)thread.get();
            mmapThread->ioConfigChanged(AUDIO_OUTPUT_OPENED);
        }
        return NO_ERROR;
    }

    return NO_INIT;
}


sp<AudioFlinger::ThreadBase> AudioFlinger::openOutput_l(audio_module_handle_t module,
                                                            audio_io_handle_t *output,
                                                            audio_config_t *config,
                                                            audio_devices_t devices,
                                                            const String8& address,
                                                            audio_output_flags_t flags)
{
    AudioHwDevice *outHwDev = findSuitableHwDev_l(module, devices);
    if (outHwDev == NULL) {
        return 0;
    }

    if (*output == AUDIO_IO_HANDLE_NONE) {
        *output = nextUniqueId(AUDIO_UNIQUE_ID_USE_OUTPUT);
    } else {
        // Audio Policy does not currently request a specific output handle.
        // If this is ever needed, see openInput_l() for example code.
        ALOGE("openOutput_l requested output handle %d is not AUDIO_IO_HANDLE_NONE", *output);
        return 0;
    }

    mHardwareStatus = AUDIO_HW_OUTPUT_OPEN;

    // FOR TESTING ONLY:
    // This if statement allows overriding the audio policy settings
    // and forcing a specific format or channel mask to the HAL/Sink device for testing.
    if (!(flags & (AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD | AUDIO_OUTPUT_FLAG_DIRECT))) {
        // Check only for Normal Mixing mode
        if (kEnableExtendedPrecision) {
            // Specify format (uncomment one below to choose)
            //config->format = AUDIO_FORMAT_PCM_FLOAT;
            //config->format = AUDIO_FORMAT_PCM_24_BIT_PACKED;
            //config->format = AUDIO_FORMAT_PCM_32_BIT;
            //config->format = AUDIO_FORMAT_PCM_8_24_BIT;
            // ALOGV("openOutput_l() upgrading format to %#08x", config->format);
        }
        if (kEnableExtendedChannels) {
            // Specify channel mask (uncomment one below to choose)
            //config->channel_mask = audio_channel_out_mask_from_count(4);  // for USB 4ch
            //config->channel_mask = audio_channel_mask_from_representation_and_bits(
            //        AUDIO_CHANNEL_REPRESENTATION_INDEX, (1 << 4) - 1);  // another 4ch example
        }
    }

    AudioStreamOut *outputStream = NULL;
    status_t status = outHwDev->openOutputStream(
            &outputStream,
            *output,
            devices,
            flags,
            config,
            address.string());

    mHardwareStatus = AUDIO_HW_IDLE;

    if (status == NO_ERROR) {
        if (flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
            sp<MmapPlaybackThread> thread = new MmapPlaybackThread(this, *output, outHwDev, outputStream,devices, AUDIO_DEVICE_NONE, mSystemReady);
            mMmapThreads.add(*output, thread);
            ALOGV("openOutput_l() created mmap playback thread: ID %d thread %p", *output, thread.get());
            return thread;
        } else {
            sp<PlaybackThread> thread;
            if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
                thread = new OffloadThread(this, outputStream, *output, devices, mSystemReady);
                ALOGV("openOutput_l() created offload output: ID %d thread %p",*output, thread.get());
            } else if ((flags & AUDIO_OUTPUT_FLAG_DIRECT)
                    || !isValidPcmSinkFormat(config->format)
                    || !isValidPcmSinkChannelMask(config->channel_mask)) {
                thread = new DirectOutputThread(this, outputStream, *output, devices, mSystemReady);
                ALOGV("openOutput_l() created direct output: ID %d thread %p",*output, thread.get());
            } else {
                thread = new MixerThread(this, outputStream, *output, devices, mSystemReady);
                ALOGV("openOutput_l() created mixer output: ID %d thread %p",*output, thread.get());
            }
            mPlaybackThreads.add(*output, thread);
            return thread;
        }
    }

    return 0;
}


/**
static const char * const audio_interfaces[] = {
    AUDIO_HARDWARE_MODULE_ID_PRIMARY,
    AUDIO_HARDWARE_MODULE_ID_A2DP,
    AUDIO_HARDWARE_MODULE_ID_USB,
};
*/
AudioHwDevice* AudioFlinger::findSuitableHwDev_l(audio_module_handle_t module,audio_devices_t devices)
{
    // if module is 0, the request comes from an old policy manager and we should load
    // well known modules
    if (module == 0) {
        ALOGW("findSuitableHwDev_l() loading well know audio hw modules");
        for (size_t i = 0; i < arraysize(audio_interfaces); i++) {
            loadHwModule_l(audio_interfaces[i]);
        }
        // then try to find a module supporting the requested device.
        for (size_t i = 0; i < mAudioHwDevs.size(); i++) {
            AudioHwDevice *audioHwDevice = mAudioHwDevs.valueAt(i);
            sp<DeviceHalInterface> dev = audioHwDevice->hwDevice();
            uint32_t supportedDevices;
            if (dev->getSupportedDevices(&supportedDevices) == OK && (supportedDevices & devices) == devices) {
                return audioHwDevice;
            }
        }
    } else {
        // check a match for the requested module handle
        AudioHwDevice *audioHwDevice = mAudioHwDevs.valueFor(module);
        if (audioHwDevice != NULL) {
            return audioHwDevice;
        }
    }

    return NULL;
}




/***
 * 由java层的AudioService调用触发
 */ 
status_t AudioFlinger::systemReady()
{
    Mutex::Autolock _l(mLock);
    ALOGI("%s", __FUNCTION__);
    if (mSystemReady) {
        ALOGW("%s called twice", __FUNCTION__);
        return NO_ERROR;
    }

    mSystemReady = true;
    for (size_t i = 0; i < mPlaybackThreads.size(); i++) {
        ThreadBase *thread = (ThreadBase *)mPlaybackThreads.valueAt(i).get();
        thread->systemReady();
    }
    for (size_t i = 0; i < mRecordThreads.size(); i++) {
        ThreadBase *thread = (ThreadBase *)mRecordThreads.valueAt(i).get();
        thread->systemReady();
    }
    return NO_ERROR;
}























//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * dumpsys media.audio_flinger [-m] [--unreachable]
 * -m 打印内存
 * */
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
status_t AudioFlinger::dump(int fd, const Vector<String16>& args)
{
    if (!dumpAllowed()) {
        dumpPermissionDenial(fd, args);
    } else {
        // get state of hardware lock
        bool hardwareLocked = dumpTryLock(mHardwareLock);
        if (!hardwareLocked) {
            String8 result(kHardwareLockedString);
            write(fd, result.string(), result.size());
        } else {
            mHardwareLock.unlock();
        }

        bool locked = dumpTryLock(mLock);

        // failed to lock - AudioFlinger is probably deadlocked
        if (!locked) {
            String8 result(kDeadlockedString);
            write(fd, result.string(), result.size());
        }

        bool clientLocked = dumpTryLock(mClientLock);
        if (!clientLocked) {
            String8 result(kClientLockedString);
            write(fd, result.string(), result.size());
        }

        if (mEffectsFactoryHal != 0) {
            mEffectsFactoryHal->dumpEffects(fd);
        } else {
            String8 result(kNoEffectsFactory);
            write(fd, result.string(), result.size());
        }

        dumpClients(fd, args);
        if (clientLocked) {
            mClientLock.unlock();
        }

        dumpInternals(fd, args);

        // dump playback threads
        for (size_t i = 0; i < mPlaybackThreads.size(); i++) {
            mPlaybackThreads.valueAt(i)->dump(fd, args);
        }

        // dump record threads
        for (size_t i = 0; i < mRecordThreads.size(); i++) {
            mRecordThreads.valueAt(i)->dump(fd, args);
        }

        // dump mmap threads
        for (size_t i = 0; i < mMmapThreads.size(); i++) {
            mMmapThreads.valueAt(i)->dump(fd, args);
        }

        // dump orphan effect chains
        if (mOrphanEffectChains.size() != 0) {
            write(fd, "  Orphan Effect Chains\n", strlen("  Orphan Effect Chains\n"));
            for (size_t i = 0; i < mOrphanEffectChains.size(); i++) {
                mOrphanEffectChains.valueAt(i)->dump(fd, args);
            }
        }
        // dump all hardware devs
        for (size_t i = 0; i < mAudioHwDevs.size(); i++) {
            sp<DeviceHalInterface> dev = mAudioHwDevs.valueAt(i)->hwDevice();
            dev->dump(fd);
        }

#ifdef TEE_SINK
        // dump the serially shared record tee sink
        if (mRecordTeeSource != 0) {
            dumpTee(fd, mRecordTeeSource, AUDIO_IO_HANDLE_NONE, 'C');
        }
#endif

        BUFLOG_RESET;

        if (locked) {
            mLock.unlock();
        }

        // append a copy of media.log here by forwarding fd to it, but don't attempt
        // to lookup the service if it's not running, as it will block for a second
        if (sMediaLogServiceAsBinder != 0) {
            dprintf(fd, "\nmedia.log:\n");
            Vector<String16> args;
            sMediaLogServiceAsBinder->dump(fd, args);
        }

        // check for optional arguments
        bool dumpMem = false;
        bool unreachableMemory = false;
        for (const auto &arg : args) {
            if (arg == String16("-m")) {
                dumpMem = true;
            } else if (arg == String16("--unreachable")) {
                unreachableMemory = true;
            }
        }

        if (dumpMem) {
            dprintf(fd, "\nDumping memory:\n");
            std::string s = dumpMemoryAddresses(100 /* limit */);
            write(fd, s.c_str(), s.size());
        }



        if (unreachableMemory) {
            dprintf(fd, "\nDumping unreachable memory:\n");
            // TODO - should limit be an argument parameter?
            std::string s = GetUnreachableMemoryString(true /* contents */, 100 /* limit */);
            write(fd, s.c_str(), s.size());
        }
    }
    return NO_ERROR;
}





