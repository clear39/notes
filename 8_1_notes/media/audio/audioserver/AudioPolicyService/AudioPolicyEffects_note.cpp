//	@frameworks/av/services/audiopolicy/service/AudioPolicyEffects.h
class AudioPolicyEffects : public RefBase
{}





//	@frameworks/av/services/audiopolicy/service/AudioPolicyEffects.cpp
AudioPolicyEffects::AudioPolicyEffects()
{
    status_t loadResult = loadAudioEffectXmlConfig();//这里返回小于0
    if (loadResult < 0) {
        ALOGW("Failed to load XML effect configuration, fallback to .conf");
        // load automatic audio effect modules
        if (access(AUDIO_EFFECT_VENDOR_CONFIG_FILE, R_OK) == 0) {//system/media/audio/include/system/audio_effects/audio_effects_conf.h:27:#define AUDIO_EFFECT_VENDOR_CONFIG_FILE "/vendor/etc/audio_effects.conf"
            loadAudioEffectConfig(AUDIO_EFFECT_VENDOR_CONFIG_FILE);//执行这里
        } else if (access(AUDIO_EFFECT_DEFAULT_CONFIG_FILE, R_OK) == 0) {// system/media/audio/include/system/audio_effects/audio_effects_conf.h:26:#define AUDIO_EFFECT_DEFAULT_CONFIG_FILE "/system/etc/audio_effects.conf"
            loadAudioEffectConfig(AUDIO_EFFECT_DEFAULT_CONFIG_FILE);
        }
    } else if (loadResult > 0) {
        ALOGE("Effect config is partially invalid, skipped %d elements", loadResult);
    }
}




status_t AudioPolicyEffects::loadAudioEffectXmlConfig() {
    auto result = effectsConfig::parse();   //  @frameworks/av/media/libeffects/config/src/EffectsConfig.cpp
    if (result.parsedConfig == nullptr) {
        return -ENOENT;//这里返回
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
    loadProcessingChain(result.parsedConfig->preprocess, mInputSources);
    loadProcessingChain(result.parsedConfig->postprocess, mOutputStreams);
    // Casting from ssize_t to status_t is probably safe, there should not be more than 2^31 errors
    return result.nbSkippedElement;
}

 //  @frameworks/av/media/libeffects/config/src/EffectsConfig.cpp
ParsingResult parse(const char* path = "/vendor/etc/audio_effects.xml") {//由于该文件 本平台没有，所以不执行
    XMLDocument doc;
    doc.LoadFile(path);
    if (doc.Error()) {
        ALOGE("Failed to parse %s: Tinyxml2 error (%d): %s %s", path,
              doc.ErrorID(), doc.GetErrorStr1(), doc.GetErrorStr2());
        return {nullptr, 0};
    }

    auto config = std::make_unique<Config>();
    size_t nbSkippedElements = 0;
    auto registerFailure = [&nbSkippedElements](bool result) {
        nbSkippedElements += result ? 0 : 1;
    };
    for (auto& xmlConfig : getChildren(doc, "audio_effects_conf")) {

        // Parse library
        for (auto& xmlLibraries : getChildren(xmlConfig, "libraries")) {
            for (auto& xmlLibrary : getChildren(xmlLibraries, "library")) {
                registerFailure(parseLibrary(xmlLibrary, &config->libraries));
            }
        }

        // Parse effects
        for (auto& xmlEffects : getChildren(xmlConfig, "effects")) {
            for (auto& xmlEffect : getChildren(xmlEffects)) {
                registerFailure(parseEffect(xmlEffect, config->libraries, &config->effects));
            }
        }

        // Parse pre processing chains
        for (auto& xmlPreprocess : getChildren(xmlConfig, "preprocess")) {
            for (auto& xmlStream : getChildren(xmlPreprocess, "stream")) {
                registerFailure(parseStream(xmlStream, config->effects, &config->preprocess));
            }
        }

        // Parse post processing chains
        for (auto& xmlPostprocess : getChildren(xmlConfig, "postprocess")) {
            for (auto& xmlStream : getChildren(xmlPostprocess, "stream")) {
                registerFailure(parseStream(xmlStream, config->effects, &config->postprocess));
            }
        }
    }
    return {std::move(config), nbSkippedElements};
}




status_t AudioPolicyEffects::loadAudioEffectConfig(const char *path = "/vendor/etc/audio_effects.conf")
{
    cnode *root;
    char *data;

    data = (char *)load_file(path, NULL);
    if (data == NULL) {
        return -ENODEV;
    }
    root = config_node("", "");
    config_load(root, data);

    Vector <EffectDesc *> effects;
    loadEffects(root, effects);
    loadInputEffectConfigurations(root, effects);
    loadStreamEffectConfigurations(root, effects);

    for (size_t i = 0; i < effects.size(); i++) {
        delete effects[i];
    }

    config_free(root);
    free(root);
    free(data);

    return NO_ERROR;
}
