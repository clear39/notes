/**
 * @    hardware/interfaces/audio/effect/all-versions/default/include/effect/all-versions/default/EffectsFactory.impl.h
 * 
 * 
*/
IEffectsFactory* HIDL_FETCH_IEffectsFactory(const char* /* name */) {
    return new EffectsFactory();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods from ::android::hardware::audio::effect::AUDIO_HAL_VERSION::IEffectsFactory follow.
Return<void> EffectsFactory::getAllDescriptors(getAllDescriptors_cb _hidl_cb) {
    Result retval(Result::OK);
    hidl_vec<EffectDescriptor> result;
    uint32_t numEffects;
    status_t status;

restart:
    numEffects = 0;
    /**
     * 
    */
    status = EffectQueryNumberEffects(&numEffects);
    if (status != OK) {
        retval = Result::NOT_INITIALIZED;
        ALOGE("Error querying number of effects: %s", strerror(-status));
        goto exit;
    }
    result.resize(numEffects);

    for (uint32_t i = 0; i < numEffects; ++i) {
        /**
         * 
        */
        effect_descriptor_t halDescriptor;
        status = EffectQueryEffect(i, &halDescriptor);
        if (status == OK) {
            /**
             * 
            */
            effectDescriptorFromHal(halDescriptor, &result[i]);
        } else {
            ALOGE("Error querying effect at position %d / %d: %s", i, numEffects, strerror(-status));
            switch (status) {
                case -ENOSYS: {
                    // Effect list has changed.
                    goto restart;
                }
                case -ENOENT: {
                    // No more effects available.
                    result.resize(i);
                }
                default: {
                    result.resize(0);
                    retval = Result::NOT_INITIALIZED;
                }
            }
            break;
        }
    }

exit:
    _hidl_cb(retval, result);
    return Void();
}


/**
 * frameworks/av/media/libeffects/factory/EffectsFactory.c
*/
int EffectQueryNumberEffects(uint32_t *pNumEffects)
{
    int ret = init();
    if (ret < 0) {
        return ret;
    }
    if (pNumEffects == NULL) {
        return -EINVAL;
    }

    pthread_mutex_lock(&gLibLock);
    *pNumEffects = gNumEffects;
    gCanQueryEffect = 1;
    pthread_mutex_unlock(&gLibLock);
    ALOGV("EffectQueryNumberEffects(): %d", *pNumEffects);
    return ret;
}

int init() {
    /**
     * @    frameworks/av/media/libeffects/factory/EffectsFactory.c:46:static int gInitDone; // true is global initialization has been preformed
    */
    if (gInitDone) {
        return 0;
    }

    /**
     * frameworks/av/media/libeffects/factory/EffectsFactory.h:30:#define PROPERTY_IGNORE_EFFECTS "ro.audio.ignore_effects"
    */
    // ignore effects or not?
    const bool ignoreFxConfFiles = property_get_bool(PROPERTY_IGNORE_EFFECTS, false);

    pthread_mutex_init(&gLibLock, NULL);

    if (ignoreFxConfFiles) {
        ALOGI("Audio effects in configuration files will be ignored");
    } else {
        /**
         * 
        */
        gConfigNbElemSkipped = EffectLoadXmlEffectConfig(NULL);
        if (gConfigNbElemSkipped < 0) {
            ALOGW("Failed to load XML effect configuration, fallback to .conf");
            EffectLoadEffectConfig();
        } else if (gConfigNbElemSkipped > 0) {
            ALOGE("Effect config is partially invalid, skipped %zd elements", gConfigNbElemSkipped);
        }
    }

    updateNumEffects();
    gInitDone = 1;
    ALOGV("init() done");
    return 0;
}


extern "C" ssize_t EffectLoadXmlEffectConfig(const char* path)
{
    using effectsConfig::parse;
    auto result = path ? parse(path) : parse();
    if (result.parsedConfig == nullptr) {
        ALOGE("Failed to parse XML configuration file");
        return -1;
    }
    /**
     * 
    */
    result.nbSkippedElement += loadLibraries(result.parsedConfig->libraries,  &gLibraryList, &gLibLock, &gLibraryFailedList) +
                               loadEffects(result.parsedConfig->effects, gLibraryList, &gSkippedEffects, &gSubEffectList);

    ALOGE_IF(result.nbSkippedElement != 0, "%zu errors during loading of configuration: %s",
             result.nbSkippedElement,
             result.configPath.empty() ? "No config file found" : result.configPath.c_str());

    return result.nbSkippedElement;
}



size_t loadLibraries(const effectsConfig::Libraries& libs,list_elem_t** libList, pthread_mutex_t* libListLock,list_elem_t** libFailedList)
{
    size_t nbSkippedElement = 0;
    for (auto& library : libs) {

        // Construct a lib entry
        auto libEntry = makeUniqueC<lib_entry_t>();
        libEntry->name = strdup(library.name.c_str());
        libEntry->effects = nullptr;
        pthread_mutex_init(&libEntry->lock, nullptr);

        if (!loadLibrary(library.path.c_str(), libEntry.get())) {
            // Register library load failure
            listPush(std::move(libEntry), libFailedList);
            ++nbSkippedElement;
            continue;
        }
        listPush(std::move(libEntry), libList, libListLock);
    }
    return nbSkippedElement;
}



/**
 * @    /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libeffects/factory/EffectsXmlConfigLoader.cpp
 * 
 *  Loads a library given its relative path and stores the result in libEntry.
 * @return true on success with libEntry's path, handle and desc filled
 *         false on success with libEntry's path filled with the path of the failed lib
 * The caller MUST free the resources path (free) and handle (dlclose) if filled.
 */
bool loadLibrary(const char* relativePath, lib_entry_t* libEntry) noexcept {

    std::string absolutePath;
    /**
     * "/odm/lib/soundfx", "/vendor/lib/soundfx", "/system/lib/soundfx" 
     * 
     * 将 relativePath 和 上面路径进行拼接，判断是否存在
     */
    if (!resolveLibrary(relativePath, &absolutePath)) {
        ALOGE("Could not find library in effect directories: %s", relativePath);
        libEntry->path = strdup(relativePath);
        return false;
    }
    const char* path = absolutePath.c_str();
    libEntry->path = strdup(path);

    // Make sure the lib is closed on early return
    std::unique_ptr<void, decltype(dlclose)*> libHandle(dlopen(path, RTLD_NOW),dlclose);
    if (libHandle == nullptr) {
        ALOGE("Could not dlopen library %s: %s", path, dlerror());
        return false;
    }

    /**
     * @    hardware/libhardware/include/hardware/audio_effect.h:306:#define AUDIO_EFFECT_LIBRARY_INFO_SYM_AS_STR  "AELI"
     * 
     * audio_effect_library_t   @hardware/libhardware/include/hardware/audio_effect.h
    */
    auto* description = static_cast<audio_effect_library_t*>(dlsym(libHandle.get(), AUDIO_EFFECT_LIBRARY_INFO_SYM_AS_STR));
    if (description == nullptr) {
        ALOGE("Invalid effect library, failed not find symbol '%s' in %s: %s",AUDIO_EFFECT_LIBRARY_INFO_SYM_AS_STR, path, dlerror());
        return false;
    }

    /**
     * @    hardware/libhardware/include/hardware/audio_effect.h:211:#define AUDIO_EFFECT_LIBRARY_TAG ((('A') << 24) | (('E') << 16) | (('L') << 8) | ('T'))
    */
    if (description->tag != AUDIO_EFFECT_LIBRARY_TAG) {
        ALOGE("Bad tag %#08x in description structure, expected %#08x for library %s",description->tag, AUDIO_EFFECT_LIBRARY_TAG, path);
        return false;
    }

    /**
     * @hardware/libhardware/include/hardware/audio_effect.h:40:#define EFFECT_API_VERSION_MAJOR(v)    ((v)>>16)
    */
    uint32_t majorVersion = EFFECT_API_VERSION_MAJOR(description->version);
    /**
     * @    hardware/libhardware/include/hardware/audio_effect.h:39:#define EFFECT_MAKE_API_VERSION(M, m)  (((M)<<16) | ((m) & 0xFFFF))
     * @    hardware/libhardware/include/hardware/audio_effect.h:209:#define        EFFECT_LIBRARY_API_VERSION         EFFECT_MAKE_API_VERSION(3,0)
    */
    uint32_t expectedMajorVersion = EFFECT_API_VERSION_MAJOR(EFFECT_LIBRARY_API_VERSION);
    if (majorVersion != expectedMajorVersion) {
        ALOGE("Unsupported major version %#08x, expected %#08x for library %s",majorVersion, expectedMajorVersion, path);
        return false;
    }

    libEntry->handle = libHandle.release();
    libEntry->desc = description;
    return true;
}



size_t loadEffects(const Effects& effects, list_elem_t* libList, list_elem_t** skippedEffects,list_sub_elem_t** subEffectList) {
    size_t nbSkippedElement = 0;

    for (auto& effect : effects) {

        auto effectLoadResult = loadEffect(effect, effect.name, libList);
        if (!effectLoadResult.success) {
            if (effectLoadResult.effectDesc != nullptr) {
                listPush(std::move(effectLoadResult.effectDesc), skippedEffects);
            }
            ++nbSkippedElement;
            continue;
        }

        /**
         * 目前没有一个 isProxy 为 true
        */
        if (effect.isProxy) {
            auto swEffectLoadResult = loadEffect(effect.libSw, effect.name + " libsw", libList);
            auto hwEffectLoadResult = loadEffect(effect.libHw, effect.name + " libhw", libList);
            if (!swEffectLoadResult.success || !hwEffectLoadResult.success) {
                // Push the main effect in the skipped list even if only a subeffect is invalid
                // as the main effect is not usable without its subeffects.
                listPush(std::move(effectLoadResult.effectDesc), skippedEffects);
                ++nbSkippedElement;
                continue;
            }
            listPush(effectLoadResult.effectDesc.get(), subEffectList);

            // Since we return a dummy descriptor for the proxy during
            // get_descriptor call, we replace it with the corresponding
            // sw effect descriptor, but keep the Proxy UUID
            *effectLoadResult.effectDesc = *swEffectLoadResult.effectDesc;
            effectLoadResult.effectDesc->uuid = effect.uuid;

            effectLoadResult.effectDesc->flags |= EFFECT_FLAG_OFFLOAD_SUPPORTED;

            auto registerSubEffect = [subEffectList](auto&& result) {
                auto entry = makeUniqueC<sub_effect_entry_t>();
                entry->object = result.effectDesc.release();
                // lib_entry_t is stored since the sub effects are not linked to the library
                entry->lib = result.lib;
                listPush(std::move(entry), &(*subEffectList)->sub_elem);
            };
            registerSubEffect(std::move(swEffectLoadResult));
            registerSubEffect(std::move(hwEffectLoadResult));
        }

        listPush(std::move(effectLoadResult.effectDesc), &effectLoadResult.lib->effects);
    }
    return nbSkippedElement;
}

/**
 * 
*/
LoadEffectResult loadEffect(const EffectImpl& effect, const std::string& name, list_elem_t* libList) {
    LoadEffectResult result;

    // Find the effect library
    result.lib = findLibrary(effect.library->name.c_str(), libList);
    if (result.lib == nullptr) {
        ALOGE("Could not find library %s to load effect %s",effect.library->name.c_str(), name.c_str());
        return result;
    }

    result.effectDesc = makeUniqueC<effect_descriptor_t>();

    // Get the effect descriptor
    if (result.lib->desc->get_descriptor(&effect.uuid, result.effectDesc.get()) != 0) {
        ALOGE("Error querying effect %s on lib %s",uuidToString(effect.uuid), result.lib->name);
        result.effectDesc.reset();
        return result;
    }

    // Dump effect for debug
#if (LOG_NDEBUG==0)
    char s[512];
    dumpEffectDescriptor(result.effectDesc.get(), s, sizeof(s), 0 /* indent */);
    ALOGV("loadEffect() read descriptor %p:%s", result.effectDesc.get(), s);
#endif

    // Check effect is supported
    uint32_t expectedMajorVersion = EFFECT_API_VERSION_MAJOR(EFFECT_CONTROL_API_VERSION);

    if (EFFECT_API_VERSION_MAJOR(result.effectDesc->apiVersion) != expectedMajorVersion) {
        ALOGE("Bad API version %#08x for effect %s in lib %s, expected major %#08x",result.effectDesc->apiVersion, name.c_str(), result.lib->name, expectedMajorVersion);
        return result;
    }

    lib_entry_t *_;
    if (findEffect(nullptr, &effect.uuid, &_, nullptr) == 0) {
        ALOGE("Effect %s uuid %s already exist", uuidToString(effect.uuid), name.c_str());
        return result;
    }

    result.success = true;
    return result;
}






//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> EffectsFactory::getDescriptor(const Uuid& uid, getDescriptor_cb _hidl_cb) {
    effect_uuid_t halUuid;
    HidlUtils::uuidToHal(uid, &halUuid);
    effect_descriptor_t halDescriptor;
    status_t status = EffectGetDescriptor(&halUuid, &halDescriptor);
    EffectDescriptor descriptor;
    effectDescriptorFromHal(halDescriptor, &descriptor);
    Result retval(Result::OK);
    if (status != OK) {
        ALOGE("Error querying effect descriptor for %s: %s", uuidToString(halUuid).c_str(), strerror(-status));
        if (status == -ENOENT) {
            retval = Result::INVALID_ARGUMENTS;
        } else {
            retval = Result::NOT_INITIALIZED;
        }
    }
    _hidl_cb(retval, descriptor);
    return Void();
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> EffectsFactory::createEffect(const Uuid& uid, int32_t session, int32_t ioHandle,createEffect_cb _hidl_cb) {
    effect_uuid_t halUuid;
    HidlUtils::uuidToHal(uid, &halUuid);
    effect_handle_t handle;
    Result retval(Result::OK);
    status_t status = EffectCreate(&halUuid, session, ioHandle, &handle);
    sp<IEffect> effect;
    uint64_t effectId = EffectMap::INVALID_ID;
    if (status == OK) {
        effect_descriptor_t halDescriptor;
        memset(&halDescriptor, 0, sizeof(effect_descriptor_t));
        status = (*handle)->get_descriptor(handle, &halDescriptor);
        if (status == OK) {
            effect = dispatchEffectInstanceCreation(halDescriptor, handle);
            effectId = EffectMap::getInstance().add(handle);
        } else {
            ALOGE("Error querying effect descriptor for %s: %s", uuidToString(halUuid).c_str(), strerror(-status));
            EffectRelease(handle);
        }
    }
    if (status != OK) {
        ALOGE("Error creating effect %s: %s", uuidToString(halUuid).c_str(), strerror(-status));
        if (status == -ENOENT) {
            retval = Result::INVALID_ARGUMENTS;
        } else {
            retval = Result::NOT_INITIALIZED;
        }
    }
    _hidl_cb(retval, effect, effectId);
    return Void();
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> EffectsFactory::debugDump(const hidl_handle& fd) {
    return debug(fd, {} /* options */);
}

Return<void> EffectsFactory::debug(const hidl_handle& fd,const hidl_vec<hidl_string>& /* options */) {
    if (fd.getNativeHandle() != nullptr && fd->numFds == 1) {
        EffectDumpEffects(fd->data[0]);
    }
    return Void();
}
