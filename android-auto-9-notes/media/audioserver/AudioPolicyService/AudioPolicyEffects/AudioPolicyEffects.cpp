//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/service/AudioPolicyEffects.cpp
AudioPolicyEffects::AudioPolicyEffects()
{
    status_t loadResult = loadAudioEffectXmlConfig();
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