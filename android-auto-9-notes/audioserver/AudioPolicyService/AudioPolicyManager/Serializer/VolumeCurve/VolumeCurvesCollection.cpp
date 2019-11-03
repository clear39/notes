
/**
 *   @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/IVolumeCurvesCollection.h
 * 
 * */
class VolumeCurvesCollection : public KeyedVector<audio_stream_type_t, VolumeCurvesForStream>, public IVolumeCurvesCollection{

}


//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/VolumeCurve.cpp
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


virtual void VolumeCurvesCollection::initializeVolumeCurves(bool /*isSpeakerDrcEnabled*/) {
    
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
     * 
     * 
    */
    return editCurvesFor(streamType).add(volumeCurve);
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
