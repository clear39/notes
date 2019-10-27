














void AudioPolicyManager::checkOutputForStrategy(routing_strategy strategy)
{
    audio_devices_t oldDevice = getDeviceForStrategy(strategy, true /*fromCache*/);
    audio_devices_t newDevice = getDeviceForStrategy(strategy, false /*fromCache*/);
    SortedVector<audio_io_handle_t> srcOutputs = getOutputsForDevice(oldDevice, mPreviousOutputs);
    SortedVector<audio_io_handle_t> dstOutputs = getOutputsForDevice(newDevice, mOutputs);

    // also take into account external policy-related changes: add all outputs which are
    // associated with policies in the "before" and "after" output vectors
    ALOGVV("checkOutputForStrategy(): policy related outputs");
    for (size_t i = 0 ; i < mPreviousOutputs.size() ; i++) {
        const sp<SwAudioOutputDescriptor> desc = mPreviousOutputs.valueAt(i);
        if (desc != 0 && desc->mPolicyMix != NULL) {
            srcOutputs.add(desc->mIoHandle);
            ALOGVV(" previous outputs: adding %d", desc->mIoHandle);
        }
    }
    for (size_t i = 0 ; i < mOutputs.size() ; i++) {
        const sp<SwAudioOutputDescriptor> desc = mOutputs.valueAt(i);
        if (desc != 0 && desc->mPolicyMix != NULL) {
            dstOutputs.add(desc->mIoHandle);
            ALOGVV(" new outputs: adding %d", desc->mIoHandle);
        }
    }

    if (!vectorsEqual(srcOutputs,dstOutputs)) {
        // get maximum latency of all source outputs to determine the minimum mute time guaranteeing
        // audio from invalidated tracks will be rendered when unmuting
        uint32_t maxLatency = 0;
        for (audio_io_handle_t srcOut : srcOutputs) {
            sp<SwAudioOutputDescriptor> desc = mPreviousOutputs.valueFor(srcOut);
            if (desc != 0 && maxLatency < desc->latency()) {
                maxLatency = desc->latency();
            }
        }
        ALOGV("checkOutputForStrategy() strategy %d, moving from output %d to output %d",
              strategy, srcOutputs[0], dstOutputs[0]);
        // mute strategy while moving tracks from one output to another
        for (audio_io_handle_t srcOut : srcOutputs) {
            sp<SwAudioOutputDescriptor> desc = mPreviousOutputs.valueFor(srcOut);
            if (desc != 0 && isStrategyActive(desc, strategy)) {
                setStrategyMute(strategy, true, desc);
                setStrategyMute(strategy, false, desc, maxLatency * LATENCY_MUTE_FACTOR, newDevice);
            }
            sp<AudioSourceDescriptor> source =
                    getSourceForStrategyOnOutput(srcOut, strategy);
            if (source != 0){
                connectAudioSource(source);
            }
        }

        // Move effects associated to this strategy from previous output to new output
        if (strategy == STRATEGY_MEDIA) {  // STRATEGY_MEDIA = 0
            selectOutputForMusicEffects();
        }
        // Move tracks associated to this strategy from previous output to new output
        for (int i = 0; i < AUDIO_STREAM_FOR_POLICY_CNT; i++) {
            /**
             * 
             * getStrategy 内部调用 Engine::getStrategyForStream
             * 
            */
            if (getStrategy((audio_stream_type_t)i) == strategy) {
                mpClientInterface->invalidateStream((audio_stream_type_t)i);
            }
        }
    }
}
