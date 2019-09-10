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


status_t HwModule::addOutputProfile(const sp<IOProfile> &profile)
{
    profile->attach(this);
    mOutputProfiles.add(profile);
    mPorts.add(profile);
    return NO_ERROR;
}

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

void HwModule::refreshSupportedDevices()
{
    // Now updating the streams (aka IOProfile until now) supported devices
    for (const auto& stream : mInputProfiles) {
        DeviceVector sourceDevices;
        for (const auto& route : stream->getRoutes()) {
            sp<AudioPort> sink = route->getSink();
            if (sink == 0 || stream != sink) {
                ALOGE("%s: Invalid route attached to input stream", __FUNCTION__);
                continue;
            }
            DeviceVector sourceDevicesForRoute = getRouteSourceDevices(route);
            if (sourceDevicesForRoute.isEmpty()) {
                ALOGE("%s: invalid source devices for %s", __FUNCTION__, stream->getName().string());
                continue;
            }
            sourceDevices.add(sourceDevicesForRoute);
        }
        if (sourceDevices.isEmpty()) {
            ALOGE("%s: invalid source devices for %s", __FUNCTION__, stream->getName().string());
            continue;
        }
        stream->setSupportedDevices(sourceDevices);
    }

    for (const auto& stream : mOutputProfiles) {
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
        }
        stream->setSupportedDevices(sinkDevices);
    }
}

DeviceVector HwModule::getRouteSourceDevices(const sp<AudioRoute> &route) const
{
    DeviceVector sourceDevices;
    for (const auto& source : route->getSources()) {
        if (source->getType() == AUDIO_PORT_TYPE_DEVICE) {
            sourceDevices.add(mDeclaredDevices.getDeviceFromTagName(source->getTagName()));
        }
    }
    return sourceDevices;
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