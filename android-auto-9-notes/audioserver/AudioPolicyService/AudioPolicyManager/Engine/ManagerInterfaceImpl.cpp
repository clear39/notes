
/***
 * @ frameworks/av/services/audiopolicy/enginedefault/src/Engine.h
 * 
 * AudioPolicyManager::initialize()
 * --> 
 * */
virtual status_t ManagerInterfaceImpl::setDeviceConnectionState(const sp<DeviceDescriptor> /*devDesc*/, audio_policy_dev_state_t /*state*/)
{
    return NO_ERROR;
}