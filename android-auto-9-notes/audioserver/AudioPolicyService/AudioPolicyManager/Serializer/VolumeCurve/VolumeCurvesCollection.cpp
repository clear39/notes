
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
 * public AudioService(Context context)
 *-->AudioService.createStreamStates()
 *----->streams[i] = new VolumeStreamState(System.VOLUME_SETTINGS_INT[mStreamVolumeAlias[i]], i);
 *-------->status_t AudioSystem::initStreamVolume(audio_stream_type_t stream, int indexMin, int indexMax)
 *-----------> status_t AudioPolicyService::initStreamVolume(audio_stream_type_t stream, int indexMin, int indexMax)
 * ---------------> virtual status_t AudioPolicyManager::initStreamVolume(audio_stream_type_t stream, int indexMin, int indexMax)
 * -------------------> 
*/
// Once XML has been parsed, must be call first to sanity check table and initialize indexes
virtual status_t VolumeCurvesCollection::initStreamVolume(audio_stream_type_t stream, int indexMin, int indexMax)
{
        editValueAt(stream).setVolumeIndexMin(indexMin);
        editValueAt(stream).setVolumeIndexMax(indexMax);
        return NO_ERROR;
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



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
status_t VolumeCurvesCollection::dump(int fd) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];

    snprintf(buffer, SIZE, "\nStreams dump:\n");
    write(fd, buffer, strlen(buffer));
    snprintf(buffer, SIZE," Stream  Can be muted  Index Min  Index Max  Index Cur [device : index]...\n");
    write(fd, buffer, strlen(buffer));
  /**
     * i表示流类型
    */
    for (size_t i = 0; i < size(); i++) {
        snprintf(buffer, SIZE, " %02zu      ", i);
        write(fd, buffer, strlen(buffer));
        valueAt(i).dump(fd);    //  VolumeCurvesForStream::dump
    }
    snprintf(buffer, SIZE, "\nVolume Curves for Use Cases (aka Stream types) dump:\n");
    write(fd, buffer, strlen(buffer));
    /**
     * i表示流类型
    */
    for (size_t i = 0; i < size(); i++) {
        std::string streamTypeLiteral;
        /**
         * typedef TypeConverter<StreamTraits> StreamTypeConverter;
         * 
        */
        StreamTypeConverter::toString(keyAt(i), streamTypeLiteral);
        snprintf(buffer, SIZE," %s (%02zu): Curve points for device category (index, attenuation in millibel)\n",streamTypeLiteral.c_str(), i);
        write(fd, buffer, strlen(buffer));
        valueAt(i).dump(fd, 2, true);// 
    }

    return NO_ERROR;
}


template <>
const StreamTypeConverter::Table StreamTypeConverter::mTable[] = {
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_VOICE_CALL),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_SYSTEM),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_RING),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_MUSIC),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_ALARM),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_NOTIFICATION),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_BLUETOOTH_SCO ),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_ENFORCED_AUDIBLE),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_DTMF),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_TTS),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_ACCESSIBILITY),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_REROUTING),
    MAKE_STRING_FROM_ENUM(AUDIO_STREAM_PATCH),
    TERMINATOR
};

template <class Traits>
inline bool TypeConverter<Traits>::toString(const typename Traits::Type &value, std::string &str)
{
    for (size_t i = 0; mTable[i].literal; i++) {
        if (mTable[i].value == value) {
            str = mTable[i].literal;
            return true;
        }
    }
    char result[64];
    snprintf(result, sizeof(result), "Unknown enum value %d", value);
    str = result;
    return false;
}
