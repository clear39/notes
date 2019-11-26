

//  @   frameworks/base/services/core/jni/BroadcastRadio/BroadcastRadioService.cpp

static jlong nativeInit(JNIEnv *env, jobject obj) {
    ALOGV("%s", __func__);
    lock_guard<mutex> lk(gContextMutex);

    auto nativeContext = new ServiceContext();
    static_assert(sizeof(jlong) >= sizeof(nativeContext), "jlong is smaller than a pointer");
    return reinterpret_cast<jlong>(nativeContext);
}


static jobject nativeLoadModules(JNIEnv *env, jobject obj, jlong nativeContext) {
    ALOGV("%s", __func__);
    lock_guard<mutex> lk(gContextMutex);
    auto& ctx = getNativeContext(nativeContext);

    // Get list of registered HIDL HAL implementations.
    auto manager = hardware::defaultServiceManager();
    hidl_vec<hidl_string> services;
    if (manager == nullptr) {
        ALOGE("Can't reach service manager, using default service implementation only");
        services = std::vector<hidl_string>({ "default" });
    } else {
        manager->listByInterface(V1_0::IBroadcastRadioFactory::descriptor,[&services](const hidl_vec<hidl_string> &registered) {
            services = registered;
        });
    }

    // Scan provided list for actually implemented modules.
    ctx.mModules.clear();
    auto jModules = make_javaref(env, env->NewObject(gjni.ArrayList.clazz, gjni.ArrayList.cstor));
    /**
     * 
    */
    for (auto&& serviceName : services) {
        ALOGV("checking service: %s", serviceName.c_str());

        auto factory = V1_0::IBroadcastRadioFactory::getService(serviceName);
        if (factory == nullptr) {
            ALOGE("can't load service %s", serviceName.c_str());
            continue;
        }

        auto halRev = HalRevision::V1_0;
        auto halMinor = 0;
        if (V1_1::IBroadcastRadioFactory::castFrom(factory).withDefault(nullptr) != nullptr) {
            halRev = HalRevision::V1_1;
            halMinor = 1;
        }
        /*
        const std::vector<Class> gAllClasses = {
            Class::AM_FM,
            Class::SAT,
            Class::DT,
        };
        */
        // Second level of scanning - that's unfortunate.
        for (auto&& clazz : gAllClasses) {
            sp<V1_0::IBroadcastRadio> module10 = nullptr;
            sp<V1_1::IBroadcastRadio> module11 = nullptr;
            factory->connectModule(clazz, [&](Result res, const sp<V1_0::IBroadcastRadio>& module) {
                if (res == Result::OK) {
                    module10 = module;
                    module11 = V1_1::IBroadcastRadio::castFrom(module).withDefault(nullptr);
                } else if (res != Result::INVALID_ARGUMENTS) {
                    ALOGE("couldn't load %s:%s module",serviceName.c_str(), V1_0::toString(clazz).c_str());
                }
            });
            if (module10 == nullptr) continue;

            auto idx = ctx.mModules.size();
            ctx.mModules.push_back({module10, halRev, {}});
            auto& nModule = ctx.mModules[idx];
            ALOGI("loaded broadcast radio module %zu: %s:%s (HAL 1.%d)",idx, serviceName.c_str(), V1_0::toString(clazz).c_str(), halMinor);

            JavaRef<jobject> jModule = nullptr;
            Result halResult = Result::OK;
            Return<void> hidlResult;
            if (module11 != nullptr) {
                hidlResult = module11->getProperties_1_1([&](const V1_1::Properties& properties) {
                    nModule.bands = properties.base.bands;
                    jModule = convert::ModulePropertiesFromHal(env, properties, idx, serviceName);
                });
            } else {
                hidlResult = module10->getProperties([&](Result result,const V1_0::Properties& properties) {
                    halResult = result;
                    if (result != Result::OK) return;
                    nModule.bands = properties.bands;
                    jModule = convert::ModulePropertiesFromHal(env, properties, idx, serviceName);
                });
            }
            if (convert::ThrowIfFailed(env, hidlResult, halResult)) return nullptr;

            env->CallBooleanMethod(jModules.get(), gjni.ArrayList.add, jModule.get());
        }
    }

    return jModules.release();
}


static void nativeFinalize(JNIEnv *env, jobject obj, jlong nativeContext) {
    ALOGV("%s", __func__);
    lock_guard<mutex> lk(gContextMutex);

    auto ctx = reinterpret_cast<ServiceContext*>(nativeContext);
    delete ctx;
}





static jobject nativeOpenTuner(JNIEnv *env, jobject obj, long nativeContext, jint moduleId,
        jobject bandConfig, bool withAudio, jobject callback) {
    ALOGV("%s", __func__);
    lock_guard<mutex> lk(gContextMutex);
    auto& ctx = getNativeContext(nativeContext);

    if (callback == nullptr) {
        ALOGE("Callback is empty");
        return nullptr;
    }

    if (moduleId < 0 || static_cast<size_t>(moduleId) >= ctx.mModules.size()) {
        ALOGE("Invalid module ID: %d", moduleId);
        return nullptr;
    }

    ALOGI("Opening tuner %d", moduleId);
    auto module = ctx.mModules[moduleId];

    Region region;
    BandConfig bandConfigHal;
    if (bandConfig != nullptr) {
        bandConfigHal = convert::BandConfigToHal(env, bandConfig, region);
    } else {
        region = Region::INVALID;
        if (module.bands.size() == 0) {
            ALOGE("No bands defined");
            return nullptr;
        }
        bandConfigHal = module.bands[0];

        /* Prefer FM to workaround possible program list fetching limitation
         * (if tuner scans only configured band for programs). */
        auto fmIt = std::find_if(module.bands.begin(), module.bands.end(),
            [](const BandConfig & band) { return utils::isFm(band.type); });
        if (fmIt != module.bands.end()) bandConfigHal = *fmIt;

        if (bandConfigHal.spacings.size() > 1) {
            bandConfigHal.spacings = hidl_vec<uint32_t>({ *std::min_element(bandConfigHal.spacings.begin(), bandConfigHal.spacings.end()) });
        }
    }

    auto tuner = make_javaref(env, env->NewObject(gjni.Tuner.clazz, gjni.Tuner.cstor,
            callback, module.halRev, region, withAudio, bandConfigHal.type));
    if (tuner == nullptr) {
        ALOGE("Unable to create new tuner object.");
        return nullptr;
    }

    auto tunerCb = Tuner::getNativeCallback(env, tuner);
    Result halResult;
    sp<ITuner> halTuner = nullptr;

    auto hidlResult = module.radioModule->openTuner(bandConfigHal, withAudio, tunerCb,
            [&](Result result, const sp<ITuner>& tuner) {
                halResult = result;
                halTuner = tuner;
            });
    if (!hidlResult.isOk() || halResult != Result::OK || halTuner == nullptr) {
        ALOGE("Couldn't open tuner");
        ALOGE_IF(hidlResult.isOk(), "halResult = %d", halResult);
        ALOGE_IF(!hidlResult.isOk(), "hidlResult = %s", hidlResult.description().c_str());
        return nullptr;
    }

    Tuner::assignHalInterfaces(env, tuner, module.radioModule, halTuner);
    ALOGD("Opened tuner %p", halTuner.get());

    bool isConnected = true;
    halTuner->getConfiguration([&](Result result, const BandConfig& config) {
        if (result == Result::OK) isConnected = config.antennaConnected;
    });
    if (!isConnected) {
        tunerCb->antennaStateChange(false);
    }

    return tuner.release();
}
