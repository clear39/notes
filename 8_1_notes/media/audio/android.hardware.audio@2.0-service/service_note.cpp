//	@hardware/interfaces/audio/2.0/default/service.cpp
int main(int /* argc */, char* /* argv */ []) {
    configureRpcThreadpool(16, true /*callerWillJoin*/);
    android::status_t status;
    status = registerPassthroughServiceImplementation<IDevicesFactory>();   //@hardware/interfaces/audio/2.0/default/DevicesFactory.cpp
    LOG_ALWAYS_FATAL_IF(status != OK, "Error while registering audio service: %d", status);
    status = registerPassthroughServiceImplementation<IEffectsFactory>();   //@hardware/interfaces/audio/effect/2.0/default/EffectsFactory.cpp
    LOG_ALWAYS_FATAL_IF(status != OK, "Error while registering audio effects service: %d", status);
    // Soundtrigger might be not present.
    status = registerPassthroughServiceImplementation<ISoundTriggerHw>();// @hardware/interfaces/soundtrigger/2.0/default/SoundTriggerHalImpl.cpp
    ALOGE_IF(status != OK, "Error while registering soundtrigger service: %d", status);
    joinRpcThreadpool();
    return status;
}


interface IDevicesFactory {
    typedef android.hardware.audio@2.0::Result Result;

    enum Device : int32_t {
        PRIMARY,
        A2DP,
        USB,
        R_SUBMIX,
        STUB
    };

    /**
     * Opens an audio device. To close the device, it is necessary to release
     * references to the returned device object.
     *
     * @param device device type.
     * @return retval operation completion status. Returns INVALID_ARGUMENTS
     *         if there is no corresponding hardware module found,
     *         NOT_INITIALIZED if an error occured while opening the hardware
     *         module.
     * @return result the interface for the created device.
     */
    openDevice(Device device) generates (Result retval, IDevice result);
};


interface IEffectsFactory {
    /**
     * Returns descriptors of different effects in all loaded libraries.
     *
     * @return retval operation completion status.
     * @return result list of effect descriptors.
     */
    getAllDescriptors() generates(Result retval, vec<EffectDescriptor> result);

    /**
     * Returns a descriptor of a particular effect.
     *
     * @return retval operation completion status.
     * @return result effect descriptor.
     */
    getDescriptor(Uuid uid) generates(Result retval, EffectDescriptor result);

    /**
     * Creates an effect engine of the specified type.  To release the effect
     * engine, it is necessary to release references to the returned effect
     * object.
     *
     * @param uid effect uuid.
     * @param session audio session to which this effect instance will be
     *                attached.  All effects created with the same session ID
     *                are connected in series and process the same signal
     *                stream.
     * @param ioHandle identifies the output or input stream this effect is
     *                 directed to in audio HAL.
     * @return retval operation completion status.
     * @return result the interface for the created effect.
     * @return effectId the unique ID of the effect to be used with
     *                  IStream::addEffect and IStream::removeEffect methods.
     */
    createEffect(Uuid uid, AudioSession session, AudioIoHandle ioHandle)
        generates (Result retval, IEffect result, uint64_t effectId);

    /**
     * Dumps information about effects into the provided file descriptor.
     * This is used for the dumpsys facility.
     *
     * @param fd dump file descriptor.
     */
    debugDump(handle fd);
};


