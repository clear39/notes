/**
 *  @   frameworks/av/media/libeffects/config/src/EffectsConfig.cpp
*/

ParsingResult parse(const char* path) {
    if (path != nullptr) {
        return parseWithPath(path);
    }
    /**
     * @   frameworks/av/media/libeffects/config/include/media/EffectsConfig.h:41:constexpr const char* DEFAULT_LOCATIONS[] = {"/odm/etc", "/vendor/etc", "/system/etc"};
     * @    frameworks/av/media/libeffects/config/include/media/EffectsConfig.h:36:constexpr const char* DEFAULT_NAME = "audio_effects.xml";
     * 
     * 只存在vendor/etc/audio_effects.xml
    */
    for (std::string location : DEFAULT_LOCATIONS) {
        std::string defaultPath = location + '/' + DEFAULT_NAME;
        if (access(defaultPath.c_str(), R_OK) != 0) {
            continue;
        }
        auto result = parseWithPath(std::move(defaultPath));
        if (result.parsedConfig != nullptr) {
            return result;
        }
    }

    ALOGE("Could not parse effect configuration in any of the default locations.");
    return {nullptr, 0, ""};
}







/** Internal version of the public parse(const char* path) where path always exist. */
ParsingResult parseWithPath(std::string&& path) {
    XMLDocument doc;
    doc.LoadFile(path.c_str());
    if (doc.Error()) {
        ALOGE("Failed to parse %s: Tinyxml2 error (%d): %s", path.c_str(),doc.ErrorID(), doc.ErrorStr());
        return {nullptr, 0, std::move(path)};
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
    return {std::move(config), nbSkippedElements, std::move(path)};
}


/** Parsed configuration.
 * Intended to be a transient structure only used for deserialization.
 * Note: Everything is copied in the configuration from the xml dom.
 *       If copies needed to be avoided due to performance issue,
 *       consider keeping a private handle on the xml dom and replace all strings by dom pointers.
 *       Or even better, use SAX parsing to avoid the allocations all together.
 */
struct Config {
    float version;
    Libraries libraries;
    Effects effects;
    std::vector<OutputStream> postprocess;
    std::vector<InputStream> preprocess;
};



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Library {
    std::string name;
    std::string path;
};

/** Parse a library xml note and push the result in libraries or return false on failure. */
bool parseLibrary(const XMLElement& xmlLibrary, Libraries* libraries) {
    const char* name = xmlLibrary.Attribute("name");
    const char* path = xmlLibrary.Attribute("path");
    if (name == nullptr || path == nullptr) {
        ALOGE("library must have a name and a path: %s", dump(xmlLibrary));
        return false;
    }
    libraries->push_back({name, path});
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct EffectImpl {
    Library* library; //< Only valid as long as the associated library vector is unmodified
    effect_uuid_t uuid;
};

struct Effect : public EffectImpl {
    std::string name;
    bool isProxy;
    EffectImpl libSw; //< Only valid if isProxy
    EffectImpl libHw; //< Only valid if isProxy
};


/** Parse an effect from an xml element describing it.
 * @return true and pushes the effect in effects on success,
 *         false on failure. */
bool parseEffect(const XMLElement& xmlEffect, Libraries& libraries, Effects* effects) {
    Effect effect{};

    const char* name = xmlEffect.Attribute("name");
    if (name == nullptr) {
        ALOGE("%s must have a name: %s", xmlEffect.Value(), dump(xmlEffect));
        return false;
    }
    effect.name = name;

    /**
     * 
    */
    // Function to parse effect.library and effect.uuid from xml
    auto parseImpl = [&libraries](const XMLElement& xmlImpl, EffectImpl& effect) {
        // Retrieve library name and uuid from xml
        const char* libraryName = xmlImpl.Attribute("library");
        const char* uuid = xmlImpl.Attribute("uuid");
        if (libraryName == nullptr || uuid == nullptr) {
            ALOGE("effect must have a library name and a uuid: %s", dump(xmlImpl));
            return false;
        }

        /**
         * 通过 libraryName 到 集合libraries查找并且赋值给effect
        */
        // Convert library name to a pointer to the previously loaded library
        auto* library = findByName(libraryName, libraries);
        if (library == nullptr) {
            ALOGE("Could not find library referenced in: %s", dump(xmlImpl));
            return false;
        }
        effect.library = library;

        if (!stringToUuid(uuid, &effect.uuid)) {
            ALOGE("Invalid uuid in: %s", dump(xmlImpl));
            return false;
        }
        return true;
    };

    if (!parseImpl(xmlEffect, effect)) {
        return false;
    }

    // Handle proxy effects
    effect.isProxy = false;
    if (std::strcmp(xmlEffect.Name(), "effectProxy") == 0) {
        effect.isProxy = true;

        // Function to parse libhw and libsw
        auto parseProxy = [&xmlEffect, &parseImpl](const char* tag, EffectImpl& proxyLib) {
            auto* xmlProxyLib = xmlEffect.FirstChildElement(tag);
            if (xmlProxyLib == nullptr) {
                ALOGE("effectProxy must contain a <%s>: %s", tag, dump(xmlEffect));
                return false;
            }
            return parseImpl(*xmlProxyLib, proxyLib);
        };
        
        if (!parseProxy("libhw", effect.libHw) || !parseProxy("libsw", effect.libSw)) {
            return false;
        }
    }

    effects->push_back(std::move(effect));
    return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** Parse an stream from an xml element describing it.
 * @return true and pushes the stream in streams on success,
 *         false on failure. 
 */
template <class Stream>
bool parseStream(const XMLElement& xmlStream, Effects& effects, std::vector<Stream>* streams) {
    const char* streamType = xmlStream.Attribute("type");
    if (streamType == nullptr) {
        ALOGE("stream must have a type: %s", dump(xmlStream));
        return false;
    }
    Stream stream;
    if (!stringToStreamType(streamType, &stream.type)) {
        ALOGE("Invalid stream type %s: %s", streamType, dump(xmlStream));
        return false;
    }

    for (auto& xmlApply : getChildren(xmlStream, "apply")) {
        const char* effectName = xmlApply.get().Attribute("effect");
        if (effectName == nullptr) {
            ALOGE("stream/apply must have reference an effect: %s", dump(xmlApply));
            return false;
        }
        auto* effect = findByName(effectName, effects);
        if (effect == nullptr) {
            ALOGE("Could not find effect referenced in: %s", dump(xmlApply));
            return false;
        }
        stream.effects.emplace_back(*effect);
    }
    streams->push_back(std::move(stream));
    return true;
}