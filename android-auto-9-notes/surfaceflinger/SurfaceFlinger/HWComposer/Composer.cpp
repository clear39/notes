

//  @   frameworks/native/services/surfaceflinger/DisplayHardware/ComposerHal.h
// Composer is a wrapper to IComposer, a proxy to server-side composer.
class Composer final : public Hwc2::Composer {
    
}



//  @   frameworks/native/services/surfaceflinger/DisplayHardware/ComposerHal.cpp

Composer::Composer(const std::string& serviceName)
    : mWriter(kWriterInitialSize),
      mIsUsingVrComposer(serviceName == std::string("vr"))
{
    mComposer = V2_1::IComposer::getService(serviceName);

    if (mComposer == nullptr) {
        LOG_ALWAYS_FATAL("failed to get hwcomposer service");
    }

    mComposer->createClient(
            [&](const auto& tmpError, const auto& tmpClient)
            {
                if (tmpError == Error::NONE) {
                    mClient = tmpClient;
                }
            });
    if (mClient == nullptr) {
        LOG_ALWAYS_FATAL("failed to create composer client");
    }

    // 2.2 support is optional
    sp<IComposer> composer_2_2 = IComposer::castFrom(mComposer);
    if (composer_2_2 != nullptr) {
        mClient_2_2 = IComposerClient::castFrom(mClient);
        LOG_ALWAYS_FATAL_IF(mClient_2_2 == nullptr, "IComposer 2.2 did not return IComposerClient 2.2");
    }

    if (mIsUsingVrComposer) {
        sp<IVrComposerClient> vrClient = IVrComposerClient::castFrom(mClient);
        if (vrClient == nullptr) {
            LOG_ALWAYS_FATAL("failed to create vr composer client");
        }
    }
}