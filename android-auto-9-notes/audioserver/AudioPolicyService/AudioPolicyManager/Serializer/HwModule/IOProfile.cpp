class IOProfile : public AudioPort{

}
/***
 * 这里封装 mixPort
    <mixPort name="mixport_bus0_media_out" role="source" flags="AUDIO_OUTPUT_FLAG_PRIMARY">
        <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000"  channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
    </mixPort>
 */ 
//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/IOProfile.cpp
IOProfile::IOProfile(const String8 &name, audio_port_role_t role)
        : AudioPort(name, AUDIO_PORT_TYPE_MIX, role),
          maxOpenCount((role == AUDIO_PORT_ROLE_SOURCE) ? 1 : 0),
          curOpenCount(0),
          maxActiveCount(1),
          curActiveCount(0) {
        
}

bool IOProfile::canOpenNewIo() {
    if (maxOpenCount == 0 || curOpenCount < maxOpenCount) {
        return true;
    }
    return false;
}

/**
 * 获取mSupportedDevices 所有所支持的  audio_devices_t
*/
audio_devices_t  IOProfile::getSupportedDevicesType() const { 
        return mSupportedDevices.types(); 
}


/** 
 *  @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPort.h
 * 这里对应标签 <profile>
 * */
void AudioPort::setAudioProfiles(const AudioProfileVector &profiles) { 
        mProfiles = profiles;
 }

void IOProfile::setFlags(uint32_t flags) override {
    AudioPort::setFlags(flags);
    if (getRole() == AUDIO_PORT_ROLE_SINK && (flags & AUDIO_INPUT_FLAG_MMAP_NOIRQ) != 0) {
        maxActiveCount = 0;
    }
}

//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPort.h
virtual void AudioPort::setFlags(uint32_t flags)
{
    //force direct flag if offload flag is set: offloading implies a direct output stream
    // and all common behaviors are driven by checking only the direct flag
    // this should normally be set appropriately in the policy configuration file
    if (mRole == AUDIO_PORT_ROLE_SOURCE && (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) != 0) {
        flags |= AUDIO_OUTPUT_FLAG_DIRECT;
    }
    mFlags = flags;
}

/**
 *   @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPort.h
 * 对应标签 <gains>子集成员<gain>
 */
void IOProfile::setGains(const AudioGainCollection &gains) { 
    mGains = gains; 
}

/**
 * 这里是 
 * HwModule::setRoutes(const AudioRouteVector &routes) 
 * --> HwModule::refreshSupportedDevices()
*/
void IOProfile::setSupportedDevices(const DeviceVector &devices)
{
    mSupportedDevices = devices;
}

const DeviceVector &getSupportedDevices() const { 
        return mSupportedDevices;
}

bool IOProfile::hasSupportedDevices() const { 
    return !mSupportedDevices.isEmpty(); 
}


/**
 *  chose first device present in mSupportedDevices also part of deviceType
 * 
 */
audio_devices_t IOProfile::getSupportedDeviceForType(audio_devices_t deviceType) const
{
    for (size_t k = 0; k  < mSupportedDevices.size(); k++) {
        audio_devices_t profileType = mSupportedDevices[k]->type();
        if (profileType & deviceType) {
            return profileType;
        }
    }
    return AUDIO_DEVICE_NONE;
}









/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * dump分析
 * */
void IOProfile::dump(int fd)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    AudioPort::dump(fd, 4);

    snprintf(buffer, SIZE, "    - flags: 0x%04x", getFlags());
    result.append(buffer);
    std::string flagsLiteral;
    if (getRole() == AUDIO_PORT_ROLE_SINK) {
        InputFlagConverter::maskToString(getFlags(), flagsLiteral);
    } else if (getRole() == AUDIO_PORT_ROLE_SOURCE) {
        OutputFlagConverter::maskToString(getFlags(), flagsLiteral);
    }
    if (!flagsLiteral.empty()) {
        result.appendFormat(" (%s)", flagsLiteral.c_str());
    }
    result.append("\n");
    write(fd, result.string(), result.size());
    
    mSupportedDevices.dump(fd, String8("Supported"), 4, false);

    result.clear();
    snprintf(buffer, SIZE, "\n    - maxOpenCount: %u - curOpenCount: %u\n",maxOpenCount, curOpenCount);
    result.append(buffer);

    snprintf(buffer, SIZE, "    - maxActiveCount: %u - curActiveCount: %u\n",maxActiveCount, curActiveCount);
    result.append(buffer);

    write(fd, result.string(), result.size());
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPort.cpp
void AudioPort::dump(int fd, int spaces, bool verbose /*= true*/) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    /**
     * 由于这里 构造 IOProfile 时 mName 不为空 
     * 这里 mName 存储的是 mixPort 标签的 name 属性值
     * 注意 这里在 IOProfile 中 mName 会打印，而在 DeviceDescriptor 不会打印
    */
    if (!mName.isEmpty()) {
        /**
         *这里 mName 目前还不知道在哪里设置
         * */ 
        snprintf(buffer, SIZE, "%*s- name: %s\n", spaces, "", mName.string());
        result.append(buffer);
        write(fd, result.string(), result.size());
    }

    /**
     * 这里 verbose 为 true
    */
    if (verbose) {
        /***
         * AudioProfileVector mProfiles; // AudioProfiles supported by this port (format, Rates, Channels)
         * 
         * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioProfile.h:136:class AudioProfileVector : public Vector<sp<AudioProfile> >
         * 
         * */
        mProfiles.dump(fd, spaces);

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

