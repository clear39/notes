/***
 *   @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/service/AudioPolicyEffects.cpp
 * 
 * void AudioPolicyService::onFirstRef()
 * --> new AudioPolicyEffects()
 * 
 * */
AudioPolicyEffects::AudioPolicyEffects()
{
    status_t loadResult = loadAudioEffectXmlConfig();
    /**
     * 由于 loadResult 大于
    */
    if (loadResult < 0) {
        ALOGW("Failed to load XML effect configuration, fallback to .conf");
        // load automatic audio effect modules
        if (access(AUDIO_EFFECT_VENDOR_CONFIG_FILE, R_OK) == 0) {
            loadAudioEffectConfig(AUDIO_EFFECT_VENDOR_CONFIG_FILE);
        } else if (access(AUDIO_EFFECT_DEFAULT_CONFIG_FILE, R_OK) == 0) {
            loadAudioEffectConfig(AUDIO_EFFECT_DEFAULT_CONFIG_FILE);
        }
    } else if (loadResult > 0) {
        ALOGE("Effect config is partially invalid, skipped %d elements", loadResult);
    }
}

status_t AudioPolicyEffects::loadAudioEffectXmlConfig() {
    //  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/media/libeffects/config/src/EffectsConfig.cpp
    /***
     * 搜索 "/odm/etc", "/vendor/etc", "/system/etc" 目录下 audio_effects.xml 文件，
     * 
     * 
     * */
    auto result = effectsConfig::parse();
    if (result.parsedConfig == nullptr) {
        return -ENOENT;
    }

    auto loadProcessingChain = [](auto& processingChain, auto& streams) {
        for (auto& stream : processingChain) {
            auto effectDescs = std::make_unique<EffectDescVector>();
            for (auto& effect : stream.effects) {
                effectDescs->mEffects.add(new EffectDesc{effect.get().name.c_str(), effect.get().uuid});
            }
            streams.add(stream.type, effectDescs.release());
        }
    };

    //由于 result.parsedConfig->preprocess 和 result.parsedConfig->postprocess 为空，这里不分析
    loadProcessingChain(result.parsedConfig->preprocess, mInputSources);
    loadProcessingChain(result.parsedConfig->postprocess, mOutputStreams);
    // Casting from ssize_t to status_t is probably safe, there should not be more than 2^31 errors
    return result.nbSkippedElement;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
 * status_t AudioPolicyService::startOutput(audio_io_handle_t output,audio_stream_type_t stream,audio_session_t session)
 * ---> status_t status = audioPolicyEffects->addOutputSessionEffects(output, stream, session);
 * 
*/
status_t AudioPolicyEffects::addOutputSessionEffects(audio_io_handle_t output,audio_stream_type_t stream,audio_session_t audioSession)
{
    status_t status = NO_ERROR;

    Mutex::Autolock _l(mLock);
    // create audio processors according to stream
    // FIXME: should we have specific post processing settings for internal streams?
    // default to media for now.
    if (stream >= AUDIO_STREAM_PUBLIC_CNT) {
        stream = AUDIO_STREAM_MUSIC;
    }
    /**
     * KeyedVector< audio_stream_type_t, EffectDescVector* > mOutputStreams;   // Automatic output effects are organized per audio_stream_type_t 
     * mOutputStreams 由配置文件进行配置获得
    */
    ssize_t index = mOutputStreams.indexOfKey(stream);
    if (index < 0) {
        ALOGV("addOutputSessionEffects(): no output processing needed for this stream");
        return NO_ERROR;
    }

    ssize_t idx = mOutputSessions.indexOfKey(audioSession);
    EffectVector *procDesc;
    if (idx < 0) {
        procDesc = new EffectVector(audioSession);
        mOutputSessions.add(audioSession, procDesc);
    } else {
        // EffectVector is existing and we just need to increase ref count
        procDesc = mOutputSessions.valueAt(idx);
    }
    procDesc->mRefCount++;

    ALOGV("addOutputSessionEffects(): session: %d, refCount: %d", audioSession, procDesc->mRefCount);
    if (procDesc->mRefCount == 1) {
        // make sure effects are associated to audio server even if we are executing a binder call
        int64_t token = IPCThreadState::self()->clearCallingIdentity();
        Vector <EffectDesc *> effects = mOutputStreams.valueAt(index)->mEffects;
        for (size_t i = 0; i < effects.size(); i++) {
            EffectDesc *effect = effects[i];
            sp<AudioEffect> fx = new AudioEffect(NULL, String16("android"), &effect->mUuid, 0, 0, 0, audioSession, output);
            status_t status = fx->initCheck();
            if (status != NO_ERROR && status != ALREADY_EXISTS) {
                ALOGE("addOutputSessionEffects(): failed to create Fx  %s on session %d", effect->mName, audioSession);
                // fx goes out of scope and strong ref on AudioEffect is released
                continue;
            }
            ALOGV("addOutputSessionEffects(): added Fx %s on session: %d for stream: %d", effect->mName, audioSession, (int32_t)stream);
            procDesc->mEffects.add(fx);
        }

        procDesc->setProcessorEnabled(true);
        IPCThreadState::self()->restoreCallingIdentity(token);
    }
    return status;
}


status_t AudioPolicyEffects::releaseOutputSessionEffects(audio_io_handle_t output,audio_stream_type_t stream,audio_session_t audioSession)
{
    status_t status = NO_ERROR;
    (void) output; // argument not used for now
    (void) stream; // argument not used for now

    Mutex::Autolock _l(mLock);
    /**
     * KeyedVector< audio_stream_type_t, EffectDescVector* > mOutputStreams;   // Automatic output effects are organized per audio_stream_type_t 
     * mOutputStreams 由配置文件进行配置获得
    */
    ssize_t index = mOutputSessions.indexOfKey(audioSession);
    if (index < 0) {
        ALOGV("releaseOutputSessionEffects: no output processing was attached to this stream");
        return NO_ERROR;
    }

    EffectVector *procDesc = mOutputSessions.valueAt(index);
    procDesc->mRefCount--;
    ALOGV("releaseOutputSessionEffects(): session: %d, refCount: %d",audioSession, procDesc->mRefCount);
    if (procDesc->mRefCount == 0) {
        procDesc->setProcessorEnabled(false);
        procDesc->mEffects.clear();
        delete procDesc;
        mOutputSessions.removeItemsAt(index);
        ALOGV("releaseOutputSessionEffects(): output processing released from session: %d",audioSession);
    }
    return status;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

status_t AudioPolicyEffects::addInputEffects(audio_io_handle_t input,audio_source_t inputSource,audio_session_t audioSession)
{
    status_t status = NO_ERROR;

    // create audio pre processors according to input source
    audio_source_t aliasSource = (inputSource == AUDIO_SOURCE_HOTWORD) ? AUDIO_SOURCE_VOICE_RECOGNITION : inputSource;

    Mutex::Autolock _l(mLock);
      /**
     *  KeyedVector< audio_source_t, EffectDescVector* > mInputSources;// Automatic input effects are configured per audio_source_t
     * mOutputStreams 由配置文件进行配置获得
    */
    ssize_t index = mInputSources.indexOfKey(aliasSource);
    if (index < 0) {
        ALOGV("addInputEffects(): no processing needs to be attached to this source");
        return status;
    }
    ssize_t idx = mInputSessions.indexOfKey(audioSession);
    EffectVector *sessionDesc;
    if (idx < 0) {
        sessionDesc = new EffectVector(audioSession);
        mInputSessions.add(audioSession, sessionDesc);
    } else {
        // EffectVector is existing and we just need to increase ref count
        sessionDesc = mInputSessions.valueAt(idx);
    }
    sessionDesc->mRefCount++;

    ALOGV("addInputEffects(): input: %d, refCount: %d", input, sessionDesc->mRefCount);
    if (sessionDesc->mRefCount == 1) {
        int64_t token = IPCThreadState::self()->clearCallingIdentity();
        Vector <EffectDesc *> effects = mInputSources.valueAt(index)->mEffects;
        for (size_t i = 0; i < effects.size(); i++) {
            EffectDesc *effect = effects[i];
            sp<AudioEffect> fx = new AudioEffect(NULL, String16("android"), &effect->mUuid, -1, 0, 0, audioSession, input);
            status_t status = fx->initCheck();
            if (status != NO_ERROR && status != ALREADY_EXISTS) {
                ALOGW("addInputEffects(): failed to create Fx %s on source %d", effect->mName, (int32_t)aliasSource);
                // fx goes out of scope and strong ref on AudioEffect is released
                continue;
            }
            for (size_t j = 0; j < effect->mParams.size(); j++) {
                fx->setParameter(effect->mParams[j]);
            }
            ALOGV("addInputEffects(): added Fx %s on source: %d",
                  effect->mName, (int32_t)aliasSource);
            sessionDesc->mEffects.add(fx);
        }
        sessionDesc->setProcessorEnabled(true);
        IPCThreadState::self()->restoreCallingIdentity(token);
    }
    return status;
}


status_t AudioPolicyEffects::releaseInputEffects(audio_io_handle_t input,audio_session_t audioSession)
{
    status_t status = NO_ERROR;

    Mutex::Autolock _l(mLock);
    ssize_t index = mInputSessions.indexOfKey(audioSession);
    if (index < 0) {
        return status;
    }
    EffectVector *sessionDesc = mInputSessions.valueAt(index);
    sessionDesc->mRefCount--;
    ALOGV("releaseInputEffects(): input: %d, refCount: %d", input, sessionDesc->mRefCount);
    if (sessionDesc->mRefCount == 0) {
        sessionDesc->setProcessorEnabled(false);
        delete sessionDesc;
        mInputSessions.removeItemsAt(index);
        ALOGV("releaseInputEffects(): all effects released");
    }
    return status;
}