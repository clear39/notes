//	@frameworks/av/services/audiopolicy/service/AudioPolicyService.h
class AudioPolicyService : public BinderService<AudioPolicyService>,public BnAudioPolicyService, public IBinder::DeathRecipient
{
public:
    // for BinderService
    static const char *getServiceName() ANDROID_API { return "media.audio_policy"; }
}


//	@frameworks/av/media/libaudioclient/include/media/IAudioPolicyService.h
class IAudioPolicyService : public IInterface
{
public:
    DECLARE_META_INTERFACE(AudioPolicyService);

    //
    // IAudioPolicyService interface (see AudioPolicyInterface for method descriptions)
    //
    virtual status_t setDeviceConnectionState(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address,
                                              const char *device_name) = 0;
    virtual audio_policy_dev_state_t getDeviceConnectionState(audio_devices_t device,
                                                                  const char *device_address) = 0;
    virtual status_t handleDeviceConfigChange(audio_devices_t device,
                                              const char *device_address,
                                              const char *device_name) = 0;
    virtual status_t setPhoneState(audio_mode_t state) = 0;
    virtual status_t setForceUse(audio_policy_force_use_t usage,
                                    audio_policy_forced_cfg_t config) = 0;
    virtual audio_policy_forced_cfg_t getForceUse(audio_policy_force_use_t usage) = 0;
    virtual audio_io_handle_t getOutput(audio_stream_type_t stream,
                                        uint32_t samplingRate = 0,
                                        audio_format_t format = AUDIO_FORMAT_DEFAULT,
                                        audio_channel_mask_t channelMask = 0,
                                        audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE,
                                        const audio_offload_info_t *offloadInfo = NULL) = 0;
    virtual status_t getOutputForAttr(const audio_attributes_t *attr,
                                      audio_io_handle_t *output,
                                      audio_session_t session,
                                      audio_stream_type_t *stream,
                                      uid_t uid,
                                      const audio_config_t *config,
                                      audio_output_flags_t flags,
                                      audio_port_handle_t *selectedDeviceId,
                                      audio_port_handle_t *portId) = 0;
    virtual status_t startOutput(audio_io_handle_t output,
                                 audio_stream_type_t stream,
                                 audio_session_t session) = 0;
    virtual status_t stopOutput(audio_io_handle_t output,
                                audio_stream_type_t stream,
                                audio_session_t session) = 0;
    virtual void releaseOutput(audio_io_handle_t output,
                               audio_stream_type_t stream,
                               audio_session_t session) = 0;
    virtual status_t  getInputForAttr(const audio_attributes_t *attr,
                              audio_io_handle_t *input,
                              audio_session_t session,
                              pid_t pid,
                              uid_t uid,
                              const audio_config_base_t *config,
                              audio_input_flags_t flags,
                              audio_port_handle_t *selectedDeviceId,
                              audio_port_handle_t *portId) = 0;
    virtual status_t startInput(audio_io_handle_t input,
                                audio_session_t session) = 0;
    virtual status_t stopInput(audio_io_handle_t input,
                               audio_session_t session) = 0;
    virtual void releaseInput(audio_io_handle_t input,
                              audio_session_t session) = 0;
    virtual status_t initStreamVolume(audio_stream_type_t stream,
                                      int indexMin,
                                      int indexMax) = 0;
    virtual status_t setStreamVolumeIndex(audio_stream_type_t stream,
                                          int index,
                                          audio_devices_t device) = 0;
    virtual status_t getStreamVolumeIndex(audio_stream_type_t stream,
                                          int *index,
                                          audio_devices_t device) = 0;
    virtual uint32_t getStrategyForStream(audio_stream_type_t stream) = 0;
    virtual audio_devices_t getDevicesForStream(audio_stream_type_t stream) = 0;
    virtual audio_io_handle_t getOutputForEffect(const effect_descriptor_t *desc) = 0;
    virtual status_t registerEffect(const effect_descriptor_t *desc,
                                    audio_io_handle_t io,
                                    uint32_t strategy,
                                    audio_session_t session,
                                    int id) = 0;
    virtual status_t unregisterEffect(int id) = 0;
    virtual status_t setEffectEnabled(int id, bool enabled) = 0;
    virtual bool     isStreamActive(audio_stream_type_t stream, uint32_t inPastMs = 0) const = 0;
    virtual bool     isStreamActiveRemotely(audio_stream_type_t stream, uint32_t inPastMs = 0)
                             const = 0;
    virtual bool     isSourceActive(audio_source_t source) const = 0;
    virtual status_t queryDefaultPreProcessing(audio_session_t audioSession,
                                              effect_descriptor_t *descriptors,
                                              uint32_t *count) = 0;
   // Check if offload is possible for given format, stream type, sample rate,
    // bit rate, duration, video and streaming or offload property is enabled
    virtual bool isOffloadSupported(const audio_offload_info_t& info) = 0;

    /* List available audio ports and their attributes */
    virtual status_t listAudioPorts(audio_port_role_t role,
                                    audio_port_type_t type,
                                    unsigned int *num_ports,
                                    struct audio_port *ports,
                                    unsigned int *generation) = 0;

    /* Get attributes for a given audio port */
    virtual status_t getAudioPort(struct audio_port *port) = 0;

    /* Create an audio patch between several source and sink ports */
    virtual status_t createAudioPatch(const struct audio_patch *patch,
                                       audio_patch_handle_t *handle) = 0;

    /* Release an audio patch */
    virtual status_t releaseAudioPatch(audio_patch_handle_t handle) = 0;

    /* List existing audio patches */
    virtual status_t listAudioPatches(unsigned int *num_patches,
                                      struct audio_patch *patches,
                                      unsigned int *generation) = 0;
    /* Set audio port configuration */
    virtual status_t setAudioPortConfig(const struct audio_port_config *config) = 0;

    virtual void registerClient(const sp<IAudioPolicyServiceClient>& client) = 0;

    virtual void setAudioPortCallbacksEnabled(bool enabled) = 0;

    virtual status_t acquireSoundTriggerSession(audio_session_t *session,
                                           audio_io_handle_t *ioHandle,
                                           audio_devices_t *device) = 0;

    virtual status_t releaseSoundTriggerSession(audio_session_t session) = 0;

    virtual audio_mode_t getPhoneState() = 0;

    virtual status_t registerPolicyMixes(const Vector<AudioMix>& mixes, bool registration) = 0;

    virtual status_t startAudioSource(const struct audio_port_config *source,
                                      const audio_attributes_t *attributes,
                                      audio_patch_handle_t *handle) = 0;
    virtual status_t stopAudioSource(audio_patch_handle_t handle) = 0;

    virtual status_t setMasterMono(bool mono) = 0;
    virtual status_t getMasterMono(bool *mono) = 0;
    virtual float    getStreamVolumeDB(
            audio_stream_type_t stream, int index, audio_devices_t device) = 0;
};