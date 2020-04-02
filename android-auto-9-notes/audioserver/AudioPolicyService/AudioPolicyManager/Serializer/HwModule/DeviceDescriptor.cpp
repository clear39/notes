class DeviceDescriptor : public AudioPort, public AudioPortConfig{

}

//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/DeviceDescriptor.cpp
DeviceDescriptor::DeviceDescriptor(audio_devices_t type, const String8 &tagName) :
    AudioPort(String8(""), AUDIO_PORT_TYPE_DEVICE, audio_is_output_device(type) ? AUDIO_PORT_ROLE_SINK :  AUDIO_PORT_ROLE_SOURCE),
    mAddress(""), mTagName(tagName), mDeviceType(type), mId(0)
{
    if (type == AUDIO_DEVICE_IN_REMOTE_SUBMIX || type == AUDIO_DEVICE_OUT_REMOTE_SUBMIX ) {
        mAddress = String8("0");
    }
}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPort.h
void AudioPort::setAudioProfiles(const AudioProfileVector &profiles) { 
        mProfiles = profiles; 
}

bool AudioPort::isAttached() { 
        return mModule != 0; 
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DeviceDescriptor::toAudioPortConfig(struct audio_port_config *dstConfig,const struct audio_port_config *srcConfig) const
{
    dstConfig->config_mask = AUDIO_PORT_CONFIG_GAIN;
    if (mSamplingRate != 0) {
        dstConfig->config_mask |= AUDIO_PORT_CONFIG_SAMPLE_RATE;
    }   
    if (mChannelMask != AUDIO_CHANNEL_NONE) {
        dstConfig->config_mask |= AUDIO_PORT_CONFIG_CHANNEL_MASK;
    }   
    if (mFormat != AUDIO_FORMAT_INVALID) {
        dstConfig->config_mask |= AUDIO_PORT_CONFIG_FORMAT;
    }   

    if (srcConfig != NULL) {
        dstConfig->config_mask |= srcConfig->config_mask;
    }   

    AudioPortConfig::toAudioPortConfig(dstConfig, srcConfig);

    dstConfig->id = mId;// 在 void DeviceDescriptor::attach(const sp<HwModule>& module) 中赋值
    /**
     * 这里返回 AUDIO_PORT_ROLE_SINK
    */
    dstConfig->role = audio_is_output_device(mDeviceType) ? AUDIO_PORT_ROLE_SINK : AUDIO_PORT_ROLE_SOURCE;
    dstConfig->type = AUDIO_PORT_TYPE_DEVICE;
    dstConfig->ext.device.type = mDeviceType;  //       AUDIO_DEVICE_OUT_BUS

    //TODO Understand why this test is necessary. i.e. why at boot time does it crash
    // without the test?
    // This has been demonstrated to NOT be true (at start up)
    // ALOG_ASSERT(mModule != NULL);
    /**
     * 
    */
    dstConfig->ext.device.hw_module = mModule != 0 ? mModule->getHandle() : AUDIO_MODULE_HANDLE_NONE;
    (void)audio_utils_strlcpy_zerofill(dstConfig->ext.device.address, mAddress.string());
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/DeviceDescriptor.cpp
/**
 * 这里 verbose 传入为true
 */ 
status_t DeviceVector::dump(int fd, const String8 &tag, int spaces, bool verbose) const
{
    if (isEmpty()) {
        return NO_ERROR;
    }
    const size_t SIZE = 256;
    char buffer[SIZE];

    /**
     * tag 在 HwModule 中传入为 Declared
     * */
    // Declared devices:
    snprintf(buffer, SIZE, "%*s- %s devices:\n", spaces, "", tag.string());
    write(fd, buffer, strlen(buffer));

    // 
    for (size_t i = 0; i < size(); i++) {
        itemAt(i)->dump(fd, spaces + 2, i, verbose);
    }
    return NO_ERROR;
}


/**
 * 这里在 DeviceVector 中调用
 * */
status_t DeviceDescriptor::dump(int fd, int spaces, int index, bool verbose /*=true*/) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%*sDevice %d:\n", spaces, "", index+1);
    result.append(buffer);
    if (mId != 0) {
        snprintf(buffer, SIZE, "%*s- id: %2d\n", spaces, "", mId);
        result.append(buffer);
    }
    /*
    这里 mTagName 存储的 是 devicePort标签的tagName属性值
    */
    if (!mTagName.isEmpty()) {
        snprintf(buffer, SIZE, "%*s- tag name: %s\n", spaces, "", mTagName.string());
        result.append(buffer);
    }
    /*
    这里 mTagName 存储的 是 devicePort标签的type属性值
    */
    std::string deviceLiteral;
    if (deviceToString(mDeviceType, deviceLiteral)) {
        snprintf(buffer, SIZE, "%*s- type: %-48s\n", spaces, "", deviceLiteral.c_str());
        result.append(buffer);
    }
    /*
    这里 mTagName 存储的 是 devicePort标签的 address 属性值
    */
    if (mAddress.size() != 0) {
        snprintf(buffer, SIZE, "%*s- address: %-32s\n", spaces, "", mAddress.string());
        result.append(buffer);
    }

    write(fd, result.string(), result.size());

    AudioPort::dump(fd, spaces, verbose /*=true*/);

    return NO_ERROR;
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPort.cpp
void AudioPort::dump(int fd, int spaces, bool verbose /*= true*/) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    /**
     * 由于这里 构造 DeviceDescriptor 时 mName 默认为空，所以下面的不执行（在 DeviceDescriptor 时没有打印）
    */
    if (!mName.isEmpty()) {
        /**
         *这里 mName 目前还不知道在哪里设置
         * */ 
        snprintf(buffer, SIZE, "%*s- name: %s\n", spaces, "", mName.string());
        result.append(buffer);
        write(fd, result.string(), result.size());
    }


    if (verbose) {
        mProfiles.dump(fd, spaces);

        //  @ frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPort.h:34:typedef Vector<sp<AudioGain> > AudioGainCollection;
        if (mGains.size() != 0) {
            snprintf(buffer, SIZE, "%*s- gains:\n", spaces, "");
            result = buffer;
            write(fd, result.string(), result.size());
            for (size_t i = 0; i < mGains.size(); i++) {
                mGains[i]->dump(fd, spaces + 2, i);
            }
        }
    }
}