
/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPatch.h
*/
class AudioPatch : public RefBase, private HandleGenerator<audio_patch_handle_t>
{
public:
        audio_patch_handle_t mHandle;
        struct audio_patch mPatch;
        uid_t mUid;
        audio_patch_handle_t mAfPatchHandle;
}

/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPatch.h
*/

AudioPatch::AudioPatch(const struct audio_patch *patch, uid_t uid) :
    mHandle(HandleGenerator<audio_patch_handle_t>::getNextHandle()),
    mPatch(*patch),
    mUid(uid),
    mAfPatchHandle(AUDIO_PATCH_HANDLE_NONE)
{
}




status_t AudioPatch::dump(int fd, int spaces, int index) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%*sAudio patch %d:\n", spaces, "", index+1);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- handle: %2d\n", spaces, "", mHandle);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- audio flinger handle: %2d\n", spaces, "", mAfPatchHandle);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- owner uid: %2d\n", spaces, "", mUid);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- %d sources:\n", spaces, "", mPatch.num_sources);
    result.append(buffer);
    for (size_t i = 0; i < mPatch.num_sources; i++) {
        if (mPatch.sources[i].type == AUDIO_PORT_TYPE_DEVICE) {
            std::string device;
            deviceToString(mPatch.sources[i].ext.device.type, device);
            snprintf(buffer, SIZE, "%*s- Device ID %d %s\n", spaces + 2, "", mPatch.sources[i].id, device.c_str());
        } else {
            snprintf(buffer, SIZE, "%*s- Mix ID %d I/O handle %d\n", spaces + 2, "", mPatch.sources[i].id, mPatch.sources[i].ext.mix.handle);
        }
        result.append(buffer);
    }   
    snprintf(buffer, SIZE, "%*s- %d sinks:\n", spaces, "", mPatch.num_sinks);
    result.append(buffer);
    for (size_t i = 0; i < mPatch.num_sinks; i++) {
        if (mPatch.sinks[i].type == AUDIO_PORT_TYPE_DEVICE) {
            std::string device;
            deviceToString(mPatch.sinks[i].ext.device.type, device);
            snprintf(buffer, SIZE, "%*s- Device ID %d %s\n", spaces + 2, "", mPatch.sinks[i].id,device.c_str());
        } else {
            snprintf(buffer, SIZE, "%*s- Mix ID %d I/O handle %d\n", spaces + 2, "", mPatch.sinks[i].id, mPatch.sinks[i].ext.mix.handle);
        }
        result.append(buffer);
    }

    write(fd, result.string(), result.size());
    return NO_ERROR;
}

