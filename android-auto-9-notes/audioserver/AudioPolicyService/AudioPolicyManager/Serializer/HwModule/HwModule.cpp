class HwModule : public RefBase{

}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/HwModule.cpp

HwModule::HwModule(const char *name, uint32_t halVersionMajor, uint32_t halVersionMinor)
    : mName(String8(name)),mHandle(AUDIO_MODULE_HANDLE_NONE)
{
    setHalVersion(halVersionMajor, halVersionMinor);
}

void HwModule::setHalVersion(uint32_t major, uint32_t minor) {
    mHalVersion = (major << 8) | (minor & 0xff);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 在配置文件解析中调用
 * 每个成员对应标签 mixPort
*/
void HwModule::setProfiles(const IOProfileCollection &profiles)
{
    for (size_t i = 0; i < profiles.size(); i++) {
        addProfile(profiles[i]);
    }
}

const InputProfileCollection &HwModule::getInputProfiles() const { return mInputProfiles; }

const OutputProfileCollection &HwModule::getOutputProfiles() const { return mOutputProfiles; }

status_t HwModule::addOutputProfile(const String8& name, const audio_config_t *config,
                                    audio_devices_t device, const String8& address)
{
    sp<IOProfile> profile = new OutputProfile(name);

    profile->addAudioProfile(new AudioProfile(config->format, config->channel_mask,config->sample_rate));

    sp<DeviceDescriptor> devDesc = new DeviceDescriptor(device);
    devDesc->mAddress = address;
    profile->addSupportedDevice(devDesc);

    return addOutputProfile(profile);
}



status_t HwModule::addInputProfile(const String8& name, const audio_config_t *config,audio_devices_t device, const String8& address)
{
    sp<IOProfile> profile = new InputProfile(name);
    profile->addAudioProfile(new AudioProfile(config->format, config->channel_mask,config->sample_rate));

    sp<DeviceDescriptor> devDesc = new DeviceDescriptor(device);
    devDesc->mAddress = address;
    profile->addSupportedDevice(devDesc);

    ALOGV("addInputProfile() name %s rate %d mask 0x%08x",name.string(), config->sample_rate, config->channel_mask);

    return addInputProfile(profile);
}



/**
 * 音频策略文件中解析调用
*/
void HwModule::setProfiles(const IOProfileCollection &profiles)
{
    for (size_t i = 0; i < profiles.size(); i++) {
        addProfile(profiles[i]);
    }
}

/**
 * 
 * void HwModule::setProfiles(const IOProfileCollection &profiles)
 * -->
*/
status_t HwModule::addProfile(const sp<IOProfile> &profile)
{
    switch (profile->getRole()) {
    case AUDIO_PORT_ROLE_SOURCE:  // output
        return addOutputProfile(profile);
    case AUDIO_PORT_ROLE_SINK: // input
        return addInputProfile(profile);
    case AUDIO_PORT_ROLE_NONE:
        return BAD_VALUE;
    }
    return BAD_VALUE;
}

/**
 * 注意这里调用该函数有俩个方法
 * status_t HwModule::addProfile(const sp<IOProfile> &profile) // 音频策略文件解析调用
 * 
 * // 
 * status_t HwModule::addInputProfile(const String8& name, const audio_config_t *config,audio_devices_t device, const String8& address)
*/
status_t HwModule::addOutputProfile(const sp<IOProfile> &profile)
{
    profile->attach(this);
    mOutputProfiles.add(profile);
    mPorts.add(profile);
    return NO_ERROR;
}

/**
 * 注意这里调用该函数有俩个方法
 * status_t HwModule::addProfile(const sp<IOProfile> &profile) // 音频策略文件解析调用
 * 
 * // 
 * status_t HwModule::addOutputProfile(const String8& name, const audio_config_t *config,audio_devices_t device, const String8& address)
*/
status_t HwModule::addInputProfile(const sp<IOProfile> &profile)
{
    profile->attach(this);
    mInputProfiles.add(profile);
    mPorts.add(profile);
    return NO_ERROR;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 解析 devicePort 标签
 * */ 
void HwModule::setDeclaredDevices(const DeviceVector &devices)
{
    //  frameworks/av/services/audiopolicy/common/managerdefinitions/include/DeviceDescriptor.h:67:class DeviceVector : public SortedVector<sp<DeviceDescriptor> >
    //  DeviceVector mDeclaredDevices; // devices declared in audio_policy configuration file.
    mDeclaredDevices = devices;
    for (size_t i = 0; i < devices.size(); i++) {
        //  frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioCollections.h:31:class AudioPortVector : public Vector<sp<AudioPort> >
        //AudioPortVector mPorts;
        mPorts.add(devices[i]);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////


sp<AudioPort> findPortByTagName(const String8 &tagName) const
{
    /**
     * mPorts 通过 setDeclaredDevices 以及 addInputProfile 和 addOutputProfile 添加 
     * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioCollections.h
    */
    return mPorts.findByTagName(tagName);
}

//  @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioCollections.h
sp<AudioPort> AudioPortVector::findByTagName(const String8 &tagName) const
{
    for (const auto& port : *this) {
        /**
         * 如果为 IOProfile 这里 getTagName 获取的是 mixPort 标签的 name 属性值
         * 
         * 如果为 DeviceDescriptor 这里 getTagName 获取的是 devicePort 标签的 tagName 属性值 
        */
        if (port->getTagName() == tagName) {
            return port;
        }
    }
    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 解析 route 标签 
 * 在解析配置文件 route 标签 时 会通过 findPortByTagName 查找对应的 AudioPort 通过 addRoute 方法 进行添加 AudioRoute 设置
 * 同时 会通过 setSink 和 setSources 方法将 AudioPort 设置进 AudioRoute 中
 * */ 
void HwModule::setRoutes(const AudioRouteVector &routes)
{
    //
    mRoutes = routes;
    // Now updating the streams (aka IOProfile until now) supported devices
    refreshSupportedDevices();
}
/*
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles primary input
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles sourceDevices.size 1
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles mixport_bus0_media_out
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 1
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles mixport_bus1_system_sound_out
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 1



09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles a2dp input
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles sourceDevices.size 1
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles a2dp output
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 1
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 2
09-11 11:30:10.175  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 3


09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles usb_device input
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles sourceDevices.size 2
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles usb_accessory output
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 1
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles usb_device output
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 1
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 2




09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles r_submix input
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mInputProfiles sourceDevices.size 1
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles r_submix output
09-11 11:30:10.176  1791  1791 V APM::HwModule: refreshSupportedDevices mOutputProfiles sinkDevices.size 1

*/
void HwModule::refreshSupportedDevices()
{
    // Now updating the streams (aka IOProfile until now) supported devices
    for (const auto& stream : mInputProfiles) {
        ALOGV("refreshSupportedDevices mInputProfiles %s",stream->getName().string());
        DeviceVector sourceDevices;
        /**
         * 这里对于 mInputProfiles 有一个以下mixport
         *  <mixPort name="primary input" role="sink">
                <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="8000,11025,16000,22050,24000,32000,44100,48000" channelMasks="AUDIO_CHANNEL_IN_MONO,AUDIO_CHANNEL_IN_STEREO"/>
            </mixPort>

            <devicePort tagName="Built-In Mic" type="AUDIO_DEVICE_IN_BUILTIN_MIC" role="source">
            </devicePort>

            解析到这个route时，会通过 addRoutes 添加
            <route type="mix" sink="primary input" sources="Built-In Mic"/>

            由于 当前配置文件中只会导致 mInputProfiles 有一个成员，而 route 只能通过sink属性值查找到 AudioPort ，
            并且将 AudioPort 通过 Route的setSink方法进行设置

        */
        for (const auto& route : stream->getRoutes()) {
            /**
             *  这里得到的就是
                <mixPort name="primary input" role="sink">
                    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="8000,11025,16000,22050,24000,32000,44100,48000" channelMasks="AUDIO_CHANNEL_IN_MONO,AUDIO_CHANNEL_IN_STEREO"/>
                </mixPort>
            */
            sp<AudioPort> sink = route->getSink();
            if (sink == 0 || stream != sink) {
                ALOGE("%s: Invalid route attached to input stream", __FUNCTION__);
                continue;
            }
            /**
             *  <route type="mix" sink="primary input" sources="Built-In Mic"/>
             * 
                <devicePort tagName="Built-In Mic" type="AUDIO_DEVICE_IN_BUILTIN_MIC" role="source">
                </devicePort>

                //  @   system/media/audio/include/system/audio-base.h:431:    AUDIO_PORT_TYPE_DEVICE = 1
                由于 getRouteSourceDevices 函数中 查找都是 DeviceDescriptor type为 AUDIO_PORT_TYPE_DEVICE 才为有效数据，
                这里注意 DeviceDescriptor 由俩个type ：
                一个是AudioPort继承过来的mType，这个mType是DeviceDescriptor在构造是传递强制设置的值为 AUDIO_PORT_TYPE_DEVICE
                一个是自身的mDeviceType，dumpsys 命令打印的和配置文件中是这个属性的值
                sourceDevicesForRoute 这里是有一个匹配的成员
                这里返回一个成员为：
                <devicePort tagName="Built-In Mic" type="AUDIO_DEVICE_IN_BUILTIN_MIC" role="source">
                </devicePort>
            */
            DeviceVector sourceDevicesForRoute = getRouteSourceDevices(route);
            if (sourceDevicesForRoute.isEmpty()) {
                ALOGE("%s: invalid source devices for %s", __FUNCTION__, stream->getName().string());
                continue;
            }
            sourceDevices.add(sourceDevicesForRoute);
            ALOGV("refreshSupportedDevices mInputProfiles sourceDevicesForRoute.size %zu sourceDevices.size %zu",sourceDevicesForRoute.size(),sourceDevices.size());
        }
        if (sourceDevices.isEmpty()) {
            ALOGE("%s: invalid source devices for %s", __FUNCTION__, stream->getName().string());
            continue;
        }
        stream->setSupportedDevices(sourceDevices);
    }

    /***
        <mixPort name="mixport_bus0_media_out" role="source" flags="AUDIO_OUTPUT_FLAG_PRIMARY">
            <!--AudioProfile-->
            <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
        </mixPort>
        <mixPort name="mixport_bus1_system_sound_out" role="source" flags="AUDIO_OUTPUT_FLAG_PRIMARY">
            <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
        </mixPort>

        <devicePort tagName="bus0_media_out" role="sink" type="AUDIO_DEVICE_OUT_BUS" address="bus0_media_out">
            <gains>
                <gain name="" mode="AUDIO_GAIN_MODE_JOINT" minValueMB="-3200" maxValueMB="600" defaultValueMB="0" stepValueMB="100"/>
            </gains>
        </devicePort>
        <devicePort tagName="bus1_system_sound_out" role="sink" type="AUDIO_DEVICE_OUT_BUS" address="bus1_system_sound_out">
            <gains>
                <gain name="" mode="AUDIO_GAIN_MODE_JOINT" minValueMB="-3200" maxValueMB="600" defaultValueMB="0" stepValueMB="100"/>
            </gains>
        </devicePort>

        <route type="mix" sink="bus0_media_out" sources="mixport_bus0_media_out"/>
        <route type="mix" sink="bus1_system_sound_out" sources="mixport_bus1_system_sound_out"/>
     * */
    for (const auto& stream : mOutputProfiles) {
        ALOGV("refreshSupportedDevices mOutputProfiles %s",stream->getName().string());
        DeviceVector sinkDevices;
        for (const auto& route : stream->getRoutes()) {
            sp<AudioPort> source = route->getSources().findByTagName(stream->getTagName());
            if (source == 0 || stream != source) {
                ALOGE("%s: Invalid route attached to output stream", __FUNCTION__);
                continue;
            }
            sp<DeviceDescriptor> sinkDevice = getRouteSinkDevice(route);
            if (sinkDevice == 0) {
                ALOGE("%s: invalid sink device for %s", __FUNCTION__, stream->getName().string());
                continue;
            }
            sinkDevices.add(sinkDevice);
            ALOGV("refreshSupportedDevices mOutputProfiles sinkDevice:%s,sinkDevices.size %zu",sinkDevice->getTagName().string(),sinkDevices.size());
        }
        stream->setSupportedDevices(sinkDevices);
    }
}

DeviceVector HwModule::getRouteSourceDevices(const sp<AudioRoute> &route) const
{
    DeviceVector sourceDevices;
    for (const auto& source : route->getSources()) {
        ALOGV("getRouteSourceDevices name:%s,source->getType():%d,AUDIO_PORT_TYPE_DEVICE:%d",source->getTagName().string(),source->getType(),AUDIO_PORT_TYPE_DEVICE);
        /*
        getType 获取的属性是有 AudioPort 继承得来，其值 等于 AUDIO_PORT_TYPE_DEVICE
        只要是 DeviceDescriptor 构造 AudioPort 的 mType 强制设置为 
        */
        if (source->getType() == AUDIO_PORT_TYPE_DEVICE) {
            sourceDevices.add(mDeclaredDevices.getDeviceFromTagName(source->getTagName()));
        }
    }
    return sourceDevices;
}

sp<DeviceDescriptor> HwModule::getRouteSinkDevice(const sp<AudioRoute> &route) const
{
    sp<DeviceDescriptor> sinkDevice = 0;
    
    ALOGV("getRouteSinkDevice name:%s,route->getSink()->getType():%d,AUDIO_PORT_TYPE_DEVICE:%d",route->getSink()->getTagName().string(),route->getSink()->getType(),AUDIO_PORT_TYPE_DEVICE);
    /*
        getType 获取的属性是有 AudioPort 继承得来，其值 等于 AUDIO_PORT_TYPE_DEVICE
        只要是 DeviceDescriptor 构造 AudioPort 的 mType 强制设置为 AUDIO_PORT_TYPE_DEVICE
    */    
    if (route->getSink()->getType() == AUDIO_PORT_TYPE_DEVICE) {
        sinkDevice = mDeclaredDevices.getDeviceFromTagName(route->getSink()->getTagName());
    }
    return sinkDevice;
}



const OutputProfileCollection &getOutputProfiles() const {
     return mOutputProfiles; 
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 在 AudioPolicyManager::initialize()  中通过 AudioFlinger的loadHwModule 加载之后，进行设置
 */ 
void HwModule::setHandle(audio_module_handle_t handle) {
    ALOGW_IF(mHandle != AUDIO_MODULE_HANDLE_NONE,"HwModule handle is changing from %d to %d", mHandle, handle);
    mHandle = handle;
}






/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * dump分析
 * */

void HwModule::dump(int fd)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    /**
     * 配置文件中读取
     */ 
    snprintf(buffer, SIZE, "  - name: %s\n", getName());
    result.append(buffer);

    /**
     * 通过 AudioFlinger::loadHwModule 获取
     * */
    snprintf(buffer, SIZE, "  - handle: %d\n", mHandle);
    result.append(buffer);

    /**
     *  配置文件中读取
     * */
    snprintf(buffer, SIZE, "  - version: %u.%u\n", getHalVersionMajor(), getHalVersionMinor());
    result.append(buffer);
    write(fd, result.string(), result.size());


    /**
     * 
     * frameworks/av/services/audiopolicy/common/managerdefinitions/include/HwModule.h:36:typedef Vector<sp<IOProfile> > OutputProfileCollection;
     * 
     * */
    if (mOutputProfiles.size()) {
        write(fd, "  - outputs:\n", strlen("  - outputs:\n"));
        for (size_t i = 0; i < mOutputProfiles.size(); i++) {
            snprintf(buffer, SIZE, "    output %zu:\n", i);
            write(fd, buffer, strlen(buffer));
            mOutputProfiles[i]->dump(fd);
        }
    }

     /**
     * 
     * frameworks/av/services/audiopolicy/common/managerdefinitions/include/HwModule.h:35:typedef Vector<sp<IOProfile> > InputProfileCollection;
     * 
     * */
    if (mInputProfiles.size()) {
        write(fd, "  - inputs:\n", strlen("  - inputs:\n"));
        for (size_t i = 0; i < mInputProfiles.size(); i++) {
            snprintf(buffer, SIZE, "    input %zu:\n", i);
            write(fd, buffer, strlen(buffer));
            mInputProfiles[i]->dump(fd);
        }
    }

    /**
     * frameworks/av/services/audiopolicy/common/managerdefinitions/include/DeviceDescriptor.h:67:class DeviceVector : public SortedVector<sp<DeviceDescriptor> >
     * 
     * */
    mDeclaredDevices.dump(fd, String8("Declared"),  2, true);

    /**
     * 
     * */
    mRoutes.dump(fd, 2);
}