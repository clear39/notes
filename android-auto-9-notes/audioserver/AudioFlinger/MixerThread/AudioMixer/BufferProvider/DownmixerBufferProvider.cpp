/**
 * @    frameworks/av/include/media/AudioBufferProvider.h
 * AudioBufferProvider 为一个抽象类
*/
class AudioBufferProvider{

}

/**
 * @    frameworks/av/include/media/BufferProviders.h
*/
class PassthruBufferProvider : public AudioBufferProvider {

}

class CopyBufferProvider : public PassthruBufferProvider {

}
class DownmixerBufferProvider : public CopyBufferProvider {

}

/**
 * @    frameworks/av/media/libaudioprocessing/BufferProviders.cpp
 * 
 * AudioMixer::AudioMixer(size_t frameCount, uint32_t sampleRate)
 * --> pthread_once(&sOnceControl, &sInitRoutine);
 * ---> DownmixerBufferProvider::init(); // for the downmixer
 * 
*/
/* call once in a pthread_once handler. */
/*static*/ status_t DownmixerBufferProvider::init()
{
    // find multichannel downmix effect if we have to play multichannel content
    sp<EffectsFactoryHalInterface> effectsFactory = EffectsFactoryHalInterface::create();
    if (effectsFactory == 0) {
        ALOGE("AudioMixer() error: could not obtain the effects factory");
        return NO_INIT;
    }
    uint32_t numEffects = 0;
    int ret = effectsFactory->queryNumberEffects(&numEffects);
    if (ret != 0) {
        ALOGE("AudioMixer() error %d querying number of effects", ret);
        return NO_INIT;
    }
    
    ALOGV("EffectQueryNumberEffects() numEffects=%d", numEffects);

    for (uint32_t i = 0 ; i < numEffects ; i++) {
        if (effectsFactory->getDescriptor(i, &sDwnmFxDesc) == 0) {
            ALOGV("effect %d is called %s", i, sDwnmFxDesc.name);
            if (memcmp(&sDwnmFxDesc.type, EFFECT_UIID_DOWNMIX, sizeof(effect_uuid_t)) == 0) {
                ALOGI("found effect \"%s\" from %s",sDwnmFxDesc.name, sDwnmFxDesc.implementor);
                sIsMultichannelCapable = true;
                break;
            }
        }
    }
    ALOGW_IF(!sIsMultichannelCapable, "unable to find downmix effect");
    return NO_INIT;
}