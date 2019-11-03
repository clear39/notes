


VolumeCurvesForStream::VolumeCurvesForStream() : mIndexMin(0), mIndexMax(1), mCanBeMuted(true)
{
    mIndexCur.add(AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME, 0);
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


int VolumeCurvesForStream::getVolumeIndex(audio_devices_t device) const
{
    //  AUDIO_DEVICE_OUT_SPEAKER
    device = Volume::getDeviceForVolume(device);
    // there is always a valid entry for AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME
    /**
     *     KeyedVector<audio_devices_t, int> mIndexCur; //< current volume index per device. 
    */
    if (mIndexCur.indexOfKey(device) < 0) {
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

    ALOGW_IF(popcount(device) != 1,
                "getDeviceForVolume() invalid device combination: %08x",
                device);

    return device;
}