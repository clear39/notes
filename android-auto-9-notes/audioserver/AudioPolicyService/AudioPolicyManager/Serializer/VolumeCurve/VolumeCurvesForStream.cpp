
/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/VolumeCurve.h
*/
// Collection of Volume Curves indexed by use case
class VolumeCurvesForStream : public KeyedVector<device_category, sp<VolumeCurve> >{

}

/**
 * 
*/
VolumeCurvesForStream::VolumeCurvesForStream() : mIndexMin(0), mIndexMax(1), mCanBeMuted(true)
{
/**
 * @    frameworks/av/services/audiopolicy/common/include/policy.h:54:#define AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME AUDIO_DEVICE_OUT_DEFAULT
 * @    system/media/audio/include/system/audio-base.h:325:    AUDIO_DEVICE_OUT_DEFAULT                   = 0x40000000u, // BIT_DEFAULT
*/
    mIndexCur.add(AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME, 0);
}


/**
 * 这里是由音频策略文件解析添加
*/
ssize_t VolumeCurvesForStream::add(const sp<VolumeCurve> &volumeCurve)
{
    device_category deviceCategory = volumeCurve->getDeviceCategory();
    ssize_t index = indexOfKey(deviceCategory);
    if (index < 0) {
        // Keep track of original Volume Curves per device category in order to switch curves.
        mOriginVolumeCurves.add(deviceCategory, volumeCurve);
        return KeyedVector::add(deviceCategory, volumeCurve);
    }
    return index;
}



void VolumeCurvesForStream::setVolumeIndexMin(int volIndexMin) { 
        mIndexMin = volIndexMin; 
}

int VolumeCurvesForStream::getVolumeIndexMin() const {
         return mIndexMin; 
}

void VolumeCurvesForStream::setVolumeIndexMax(int volIndexMax) {
         mIndexMax = volIndexMax;
 }

 int VolumeCurvesForStream::getVolumeIndexMax() const {
          return mIndexMax; 
}

/**
 * AudioService.applyDeviceVolume_syncVSS()/applyAllVolumes()
 * status_t AudioSystem::setStreamVolumeIndex(audio_stream_type_t stream,,int index, audio_devices_t device)
 * status_t AudioPolicyManager::setStreamVolumeIndex(audio_stream_type_t stream,int index, audio_devices_t device)
*/
void VolumeCurvesForStream::addCurrentVolumeIndex(audio_devices_t device, int index) { 
        mIndexCur.add(device, index); 
}


int VolumeCurvesForStream::getVolumeIndex(audio_devices_t device) const
{
    /**
     *   AUDIO_DEVICE_OUT_SPEAKER
     */
    device = Volume::getDeviceForVolume(device);
    // there is always a valid entry for AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME
    /**
     *     KeyedVector<audio_devices_t, int> mIndexCur; //< current volume index per device. 
    */
    if (mIndexCur.indexOfKey(device) < 0) {
        /**
         * @    frameworks/av/services/audiopolicy/common/include/policy.h:54:#define AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME AUDIO_DEVICE_OUT_DEFAULT
         * @    system/media/audio/include/system/audio-base.h:325:    AUDIO_DEVICE_OUT_DEFAULT                   = 0x40000000u, // BIT_DEFAULT
        */
        device = AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME;
    }
    return mIndexCur.valueFor(device);
}

/**
 * @    frameworks/av/services/audiopolicy/common/include/Volume.h
*/
static audio_devices_t Volume::getDeviceForVolume(audio_devices_t device)
{
    if (device == AUDIO_DEVICE_NONE) {

        // this happens when forcing a route update and no track is active on an output.
        // In this case the returned category is not important.
        device =  AUDIO_DEVICE_OUT_SPEAKER;

    } else if (popcount(device) > 1) {
        // Multiple device selection is either:
        //  - speaker + one other device: give priority to speaker in this case.
        //  - one A2DP device + another device: happens with duplicated output. In this case
        // retain the device on the A2DP output as the other must not correspond to an active
        // selection if not the speaker.
        //  - HDMI-CEC system audio mode only output: give priority to available item in order.
        if (device & AUDIO_DEVICE_OUT_SPEAKER) {
            device = AUDIO_DEVICE_OUT_SPEAKER;
        } else if (device & AUDIO_DEVICE_OUT_SPEAKER_SAFE) {
            device = AUDIO_DEVICE_OUT_SPEAKER_SAFE;
        } else if (device & AUDIO_DEVICE_OUT_HDMI_ARC) {
            device = AUDIO_DEVICE_OUT_HDMI_ARC;
        } else if (device & AUDIO_DEVICE_OUT_AUX_LINE) {
            device = AUDIO_DEVICE_OUT_AUX_LINE;
        } else if (device & AUDIO_DEVICE_OUT_SPDIF) {
            device = AUDIO_DEVICE_OUT_SPDIF;
        } else {
            device = (audio_devices_t)(device & AUDIO_DEVICE_OUT_ALL_A2DP);
        }

    }

    /*SPEAKER_SAFE is an alias of SPEAKER for purposes of volume control*/
    if (device == AUDIO_DEVICE_OUT_SPEAKER_SAFE)
        device = AUDIO_DEVICE_OUT_SPEAKER;

    ALOGW_IF(popcount(device) != 1,"getDeviceForVolume() invalid device combination: %08x",device);

    return device;
}










/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void VolumeCurvesForStream::dump(int fd, int spaces = 0, bool curvePoints) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    if (!curvePoints) {
        snprintf(buffer, SIZE, "%s         %02d         %02d         ", mCanBeMuted ? "true " : "false", mIndexMin, mIndexMax);
        result.append(buffer);
        for (size_t i = 0; i < mIndexCur.size(); i++) {
            snprintf(buffer, SIZE, "%04x : %02d, ", mIndexCur.keyAt(i), mIndexCur.valueAt(i));
            result.append(buffer);
        }
        result.append("\n");
        write(fd, result.string(), result.size());
        return;
    }

    for (size_t i = 0; i < size(); i++) {
        std::string deviceCatLiteral;
        /**
         * 
        */
        DeviceCategoryConverter::toString(keyAt(i), deviceCatLiteral);
        snprintf(buffer, SIZE, "%*s %s :",spaces, "", deviceCatLiteral.c_str());
        write(fd, buffer, strlen(buffer));
        valueAt(i)->dump(fd);
    }
    result.append("\n");
    write(fd, result.string(), result.size());
}