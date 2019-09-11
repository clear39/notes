

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/include/IVolumeCurvesCollection.h
class VolumeCurvesCollection : public KeyedVector<audio_stream_type_t, VolumeCurvesForStream>, public IVolumeCurvesCollection{

}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/VolumeCurve.cpp
VolumeCurvesCollection::VolumeCurvesCollection()
{
    /***
     * system/media/audio/include/system/audio-base.h:38:    AUDIO_STREAM_PATCH = 12,
     * 
     * system/media/audio/include/system/audio-base-utils.h:37:    AUDIO_STREAM_CNT          = AUDIO_STREAM_PATCH + 1,
     * 
     * */
    // Create an empty collection of curves
    for (ssize_t i = 0 ; i < AUDIO_STREAM_CNT; i++) {
        audio_stream_type_t stream = static_cast<audio_stream_type_t>(i);
        KeyedVector::add(stream, VolumeCurvesForStream());
    }
}

VolumeCurvesForStream::VolumeCurvesForStream() : mIndexMin(0), mIndexMax(1), mCanBeMuted(true)
{
    mIndexCur.add(AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME, 0);
}

/**
 * 这里是在配置文件解析时调用
*/
ssize_t VolumeCurvesCollection::add(const sp<VolumeCurve> &volumeCurve)
{
    audio_stream_type_t streamType = volumeCurve->getStreamType();
    /**
     * editCurvesFor(streamType) 获得 对应流类型的 VolumeCurvesForStream
     * 会调用到 VolumeCurvesForStream::add
    */
    return editCurvesFor(streamType).add(volumeCurve);
}

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




virtual int VolumeCurvesCollection::getVolumeIndex(audio_stream_type_t stream, audio_devices_t device)
{
    return getCurvesFor(stream).getVolumeIndex(device);
}

const VolumeCurvesForStream &VolumeCurvesCollection::getCurvesFor(audio_stream_type_t stream) const
{
    ALOG_ASSERT(indexOfKey(stream) >= 0, "Invalid stream type for Volume Curve");
    return valueFor(stream);
}


int VolumeCurvesForStream::getVolumeIndex(audio_devices_t device) const
{
    //  AUDIO_DEVICE_OUT_SPEAKER
    device = Volume::getDeviceForVolume(device);
    // there is always a valid entry for AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME
    if (mIndexCur.indexOfKey(device) < 0) {
        device = AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME;
    }
    return mIndexCur.valueFor(device);
}

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

    ALOGW_IF(popcount(device) != 1,
                "getDeviceForVolume() invalid device combination: %08x",
                device);

    return device;
}