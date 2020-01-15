/**
 *   @   frameworks/av/media/libaudiohal/EffectsFactoryHalInterface.cpp
*/
// static
sp<EffectsFactoryHalInterface> EffectsFactoryHalInterface::create() {
    if (hardware::audio::effect::V4_0::IEffectsFactory::getService() != nullptr) {
        /**
         * 返回这里
        */
        return new V4_0::EffectsFactoryHalHidl();
    }
    if (hardware::audio::effect::V2_0::IEffectsFactory::getService() != nullptr) {
        return new EffectsFactoryHalHidl();
    }
    return nullptr;
}